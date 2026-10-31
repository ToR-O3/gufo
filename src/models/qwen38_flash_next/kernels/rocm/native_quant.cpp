#include "src/models/qwen38_flash_next/kernels/rocm/native_quant.hpp"

#include <stdexcept>

#include "src/models/qwen/hip/ops/gemm.hpp"
#include "src/models/qwen/hip/ops/prefill_fp16.hpp"

namespace gufo::models::qwen38_flash_next::rocm {
void NativeIq4Dense(const void* weights, core::GgmlType type,
                    const float* input, void* half_scratch, float* output,
                    std::size_t rows, std::size_t outputs, std::size_t cols,
                    bool prefill, hipStream_t stream) {
  const auto block = type == core::GgmlType::kIQ4_NL ? 32U : 256U;
  if (!IsNativeIq4Dense(type) || !rows || !outputs || !cols || cols % block ||
      (prefill && half_scratch == nullptr))
    throw std::invalid_argument("invalid native IQ4 dense projection");
  // HC down/inject, alpha/beta and indexer projections have few output
  // rows. Keep their long reductions in FP32: a 10240-wide IQ4_XS WMMA
  // probe accumulated 4.7e-5 relative error even after matching half inputs.
  // The exact row-dot path also avoids mostly empty matrix-core tiles.
  if (prefill && outputs > 512) {
    gufo::hip::LaunchFloatToFp16(input, half_scratch, rows * cols, stream);
    NativeIq4DenseF16(weights, type, half_scratch, output, rows, outputs, cols,
                      stream);
  } else if (prefill) {
    gufo::hip::LaunchGroupedQuantGEMMFp32(type, weights, input, output, rows,
                                          outputs, cols, stream);
  } else {
    gufo::hip::LaunchBatchedQuantGEMMFp32(type, weights, input, output, rows,
                                          outputs, cols, stream);
  }
}
void NativeIq4DenseF16(const void* weights, core::GgmlType type,
                       const void* input, float* output, std::size_t rows,
                       std::size_t outputs, std::size_t cols,
                       hipStream_t stream) {
  gufo::hip::LaunchBatchedQuantGEMMFp16(type, weights, input, output, rows,
                                        outputs, cols, stream);
}
}  // namespace gufo::models::qwen38_flash_next::rocm
