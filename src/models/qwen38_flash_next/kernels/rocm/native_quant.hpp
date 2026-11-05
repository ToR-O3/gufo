#ifndef GUFO_MODELS_QWEN38_FLASH_NEXT_ROCM_NATIVE_QUANT_HPP_
#define GUFO_MODELS_QWEN38_FLASH_NEXT_ROCM_NATIVE_QUANT_HPP_

#include <hip/hip_runtime.h>

#include <cstddef>
#include <gufo/ext/gguf_reader.hpp>

namespace gufo::models::qwen38_flash_next::rocm {
// Native dense formats used by the OrcaRouter and ISTA mixed GGUF files.
constexpr bool IsNativeIq4Dense(core::GgmlType type) noexcept {
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
void NativeIq4Dense(const void* weights, core::GgmlType type,
                    const float* input, void* half_scratch, float* output,
                    std::size_t rows, std::size_t outputs, std::size_t cols,
                    bool prefill, hipStream_t stream);
// Wide prefill over the same cached F16 activation rows as the Q4/Q8 paths.
// No weight storage or conversion is allocated by this launcher.
void NativeIq4DenseF16(const void* weights, core::GgmlType type,
                       const void* input, float* output, std::size_t rows,
                       std::size_t outputs, std::size_t cols,
                       hipStream_t stream);
}  // namespace gufo::models::qwen38_flash_next::rocm
#endif
