#ifndef GUFO_MODELS_QWEN38_FLASH_NEXT_ROCM_NATIVE_QUANT_HPP_
#define GUFO_MODELS_QWEN38_FLASH_NEXT_ROCM_NATIVE_QUANT_HPP_

#include <hip/hip_runtime.h>

#include <cstddef>
#include <gufo/ext/gguf_reader.hpp>
#include <stdexcept>

#include "src/models/qwen38_flash_next/kernels/rocm/kernels.hpp"

namespace gufo::models::qwen38_flash_next::rocm {
// Shared type mapping for small projections and embedding lookups.
inline WeightType SmallType(core::GgmlType type) {
  switch (type) {
    case core::GgmlType::kBF16:
      return WeightType::kBF16;
    case core::GgmlType::kF16:
      return WeightType::kF16;
    case core::GgmlType::kQ8_0:
      return WeightType::kQ8_0;
    case core::GgmlType::kIQ4_XS:
      return WeightType::kIQ4_XS;
    case core::GgmlType::kF32:
      return WeightType::kF32;
    default:
      throw std::logic_error("unsupported Flash-Next matrix format");
  }
}

// Packed formats supported by the shared quantized dense launchers.
constexpr bool IsNativeQuantDense(core::GgmlType type) noexcept {
  return type == core::GgmlType::kIQ4_XS || type == core::GgmlType::kIQ4_NL ||
         type == core::GgmlType::kQ4_K || type == core::GgmlType::kQ5_K ||
         type == core::GgmlType::kQ6_K;
}
// Packed weights remain unchanged. Decode/verification uses FP32 activations
// with width-independent arithmetic. Prefill projections with <=512 outputs
// retain FP32 activations and row dots. Larger prefill projections round
// activations and decoded weights to F16 in the existing WMMA kernel,
// accumulating in FP32. Scratch holds exactly rows * cols half values; callers
// bound rows to its capacity.
void NativeQuantDense(const void* weights, core::GgmlType type,
                      const float* input, void* half_scratch, float* output,
                      std::size_t rows, std::size_t outputs, std::size_t cols,
                      bool prefill, hipStream_t stream);
// Wide prefill over the same cached F16 activation rows as the Q4/Q8 paths.
// No weight storage or conversion is allocated by this launcher.
void NativeQuantDenseF16(const void* weights, core::GgmlType type,
                         const void* input, float* output, std::size_t rows,
                         std::size_t outputs, std::size_t cols,
                         hipStream_t stream);
}  // namespace gufo::models::qwen38_flash_next::rocm
#endif
