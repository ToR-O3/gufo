#ifndef GUFO_FAMILIES_QWEN_KERNELS_IQ4_HALF_HPP_
#define GUFO_FAMILIES_QWEN_KERNELS_IQ4_HALF_HPP_

#include <hip/hip_fp16.h>
#include <hip/hip_runtime.h>

namespace gufo::hip {

struct HalfWeights {
  uint4 low, high;
};

struct RawIqWeights {
  unsigned data[4], shift;
  float scale;
};

// All 16 IQ4 codebook entries are integers represented exactly in FP16.
// Look up their half bits directly, avoiding signed-byte extraction and
// conversion for each weight. Scaling still uses the original FP32 product.
__device__ __forceinline__ HalfWeights
DecodeIqRaw(const RawIqWeights& weights) {
  union {
    __attribute__((__vector_size__(16 * sizeof(_Float16)))) _Float16 f;
    uint4 u[2];
  } result;
#pragma unroll
  for (unsigned i = 0; i < 4; ++i) {
    const unsigned n = (weights.data[i] >> weights.shift) & 0x0f0f0f0fU;
    const unsigned index = n & 0x07070707U;
    const unsigned select = ((n & 0x08080808U) >> 3) * 255;
    // Low and high bytes of FP16 {-127, -104, -83, -65, -49, -35,
    // -22, -10, 1, 13, 25, 38, 53, 69, 89, 113}, four entries per word.
    const unsigned lo0 = __builtin_amdgcn_perm(0x00806020U, 0x103080f0U, index);
    const unsigned lo1 = __builtin_amdgcn_perm(0x109050a0U, 0xc0408000U, index);
    const unsigned hi0 = __builtin_amdgcn_perm(0xc9cdd0d2U, 0xd4d5d6d7U, index);
    const unsigned hi1 = __builtin_amdgcn_perm(0x57555452U, 0x504e4a3cU, index);
    const unsigned lo = (lo0 & ~select) | (lo1 & select);
    const unsigned hi = (hi0 & ~select) | (hi1 & select);
    const unsigned c0 = __builtin_amdgcn_perm(hi, lo, 0x05010400U);
    const unsigned c1 = __builtin_amdgcn_perm(hi, lo, 0x07030602U);
    result.f[4 * i] = static_cast<_Float16>(
        __fmul_rn(weights.scale, __half2float(__ushort_as_half(c0 & 65535))));
    result.f[4 * i + 1] = static_cast<_Float16>(
        __fmul_rn(weights.scale, __half2float(__ushort_as_half(c0 >> 16))));
    result.f[4 * i + 2] = static_cast<_Float16>(
        __fmul_rn(weights.scale, __half2float(__ushort_as_half(c1 & 65535))));
    result.f[4 * i + 3] = static_cast<_Float16>(
        __fmul_rn(weights.scale, __half2float(__ushort_as_half(c1 >> 16))));
  }
  return {result.u[0], result.u[1]};
}

}  // namespace gufo::hip

#endif  // GUFO_FAMILIES_QWEN_KERNELS_IQ4_HALF_HPP_
