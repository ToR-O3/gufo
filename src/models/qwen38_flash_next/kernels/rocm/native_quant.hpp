#ifndef GUFO_MODELS_QWEN38_FLASH_NEXT_ROCM_NATIVE_QUANT_HPP_
#define GUFO_MODELS_QWEN38_FLASH_NEXT_ROCM_NATIVE_QUANT_HPP_

#include <hip/hip_runtime.h>

#include <cstddef>
#include <gufo/ext/experimental/ggml_dequant.hpp>
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
// The matrix route retains FP32 reductions for small output projections.
// Its F16 kernels require complete encoded weight blocks. The caller owns
// the phase policy and bounds the activation rows to its staging capacity.
constexpr bool NativeQuantDenseF16Route(core::GgmlType type,
                                        std::size_t outputs,
                                        std::size_t cols) noexcept {
  const auto block = quant::QuantizedBlockElements(type);
  return IsNativeQuantDense(type) && outputs > 512 && cols != 0 && block != 0 &&
         cols % block == 0;
}
// Packed weights remain unchanged. Vector mode retains FP32 activations;
// matrix mode uses F16 weights/activations only for the qualified route above.
// Other matrix projections use the grouped FP32 row-dot path. Scratch holds
// rows * cols half values; callers bound rows to its capacity.
void NativeQuantDense(const void* weights, core::GgmlType type,
                      const float* input, void* half_scratch, float* output,
                      std::size_t rows, std::size_t outputs, std::size_t cols,
                      bool matrix_rows, hipStream_t stream);
// Wide prefill over the same cached F16 activation rows as the Q4/Q8 paths.
// No weight storage or conversion is allocated by this launcher.
void NativeQuantDenseF16(const void* weights, core::GgmlType type,
                         const void* input, float* output, std::size_t rows,
                         std::size_t outputs, std::size_t cols,
                         hipStream_t stream);
}  // namespace gufo::models::qwen38_flash_next::rocm
#endif
