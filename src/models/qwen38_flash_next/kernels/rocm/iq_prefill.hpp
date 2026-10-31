#ifndef GUFO_MODELS_QWEN38_FLASH_NEXT_KERNELS_ROCM_IQ_PREFILL_HPP_
#define GUFO_MODELS_QWEN38_FLASH_NEXT_KERNELS_ROCM_IQ_PREFILL_HPP_

#include <hip/hip_fp16.h>
#include <hip/hip_runtime.h>

#include <cstdint>

#include "src/models/qwen38_flash_next/kernels/rocm/kernels.hpp"

namespace gufo::models::qwen38_flash_next::rocm::iq_prefill {
// Reuse the MMQ codebook definitions, including its pinned GGML provenance.
// This namespace keeps their block declarations separate from the Q4 kernel.
#include "src/models/qwen38_flash_next/kernels/rocm/mmq/ggml-common.h"

struct Raw32 {
  uint4 codes;
  float lo_scale;
  float hi_scale;
};

__device__ __forceinline__ unsigned Word(const std::uint8_t* p) {
  unsigned v;
  __builtin_memcpy(&v, p, sizeof(v));
  return v;
}

// A stage retains compact indices/signs in LDS, not expanded weight rows.
// IQ2_S has two scales per 32 weights; the other formats have one.
template<WeightType Type>
__device__ __forceinline__ Raw32 Load(const std::uint8_t* row, int kb) {
  if constexpr (Type == WeightType::kQ2_0) {
    const auto* b = row + (kb / 2) * 18;
    const auto* q = b + 2 + (kb % 2) * 8;
    const float d = __half2float(*reinterpret_cast<const __half*>(b));
    return {make_uint4(Word(q), Word(q + 4), 0, 0), d, d};
  } else {
    constexpr int bytes = Type == WeightType::kIQ2_S     ? 82
                          : Type == WeightType::kIQ3_XXS ? 98
                                                         : 110;
    const auto* b = row + (kb / 8) * bytes;
    const int sb = kb % 8;
    const float d = __half2float(*reinterpret_cast<const __half*>(b));
    if constexpr (Type == WeightType::kIQ2_S) {
      const auto scales = b[74 + sb];
      const float lo = d * (0.5F + float(scales & 15)) * 0.25F;
      const float hi = d * (0.5F + float(scales >> 4)) * 0.25F;
      return {make_uint4(Word(b + 2 + sb * 4), Word(b + 34 + sb * 4),
                         b[66 + sb], 0),
              lo, hi};
    } else if constexpr (Type == WeightType::kIQ3_XXS) {
      const unsigned aux = Word(b + 66 + sb * 4);
      const float scale = d * (0.5F + float(aux >> 28)) * 0.5F;
      return {make_uint4(Word(b + 2 + sb * 8), Word(b + 6 + sb * 8), aux, 0),
              scale, scale};
    } else {
      const unsigned scale = (b[106 + sb / 2] >> (4 * (sb % 2))) & 15;
      const float ds = d * float(1 + 2 * scale);
      return {make_uint4(Word(b + 2 + sb * 8), Word(b + 6 + sb * 8),
                         Word(b + 74 + sb * 4), b[66 + sb]),
              ds, ds};
    }
  }
}

using Half16 = _Float16 __attribute__((vector_size(32)));

// Decode each four/eight-element grid once and reuse its packed bytes.
// The matrix kernel handles routing, LDS, WMMA and every output epilogue.
// Apply FP32 subscales before the one F16 weight rounding, as in the CPU
// formula; the fractional IQ2/IQ3 scales must never use integer division.
template<WeightType Type, int Half>
__device__ __forceinline__ Half16 Decode(uint4 raw, float scale) {
  Half16 result;
#pragma unroll
  for (int group = 0; group < 4; ++group) {
    unsigned grid = 0, signs = 0;
    if constexpr (Type == WeightType::kIQ2_S) {
      const int lane = Half * 2 + group / 2;
      const unsigned index =
          ((raw.x >> (8 * lane)) & 255U) | (((raw.z >> (2 * lane)) & 3U) << 8);
      const auto full_grid = iq2s_grid[index];
      grid = static_cast<unsigned>(full_grid >> (32 * (group % 2)));
      signs = raw.y >> (8 * lane + 4 * (group % 2));
    } else if constexpr (Type == WeightType::kIQ3_XXS) {
      const unsigned codes = Half == 0 ? raw.x : raw.y;
      grid = iq3xxs_grid[(codes >> (8 * group)) & 255U];
      const int lane = Half * 2 + group / 2;
      signs = ksigns_iq2xs[(raw.z >> (7 * lane)) & 127U] >> (4 * (group % 2));
    } else if constexpr (Type == WeightType::kIQ3_S) {
      const unsigned codes = Half == 0 ? raw.x : raw.y;
      const unsigned index = ((codes >> (8 * group)) & 255U) |
                             (((raw.w >> (Half * 4 + group)) & 1U) << 8);
      grid = iq3s_grid[index];
      signs = raw.z >> (Half * 16 + group * 4);
    }
#pragma unroll
    for (int j = 0; j < 4; ++j) {
      int value;
      if constexpr (Type == WeightType::kQ2_0) {
        const unsigned codes = Half == 0 ? raw.x : raw.y;
        value = int((codes >> (2 * (group * 4 + j))) & 3U) - 1;
      } else {
        const int magnitude = int((grid >> (8 * j)) & 255U);
        value = ((signs >> j) & 1U) ? -magnitude : magnitude;
      }
      result[group * 4 + j] =
          static_cast<_Float16>(__fmul_rn(scale, float(value)));
    }
  }
  return result;
}
}  // namespace gufo::models::qwen38_flash_next::rocm::iq_prefill
#endif
