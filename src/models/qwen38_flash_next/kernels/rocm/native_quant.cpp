#include "src/models/qwen38_flash_next/kernels/rocm/native_quant.hpp"

#include <stdexcept>

#include "src/families/qwen/kernels/ops/gemm.hpp"
#include "src/families/qwen/kernels/ops/prefill_fp16.hpp"

namespace gufo::models::qwen38_flash_next::rocm {
void NativeQuantDense(const void* weights, core::GgmlType type,
                      const float* input, void* half_scratch, float* output,
                      std::size_t rows, std::size_t outputs, std::size_t cols,
                      bool matrix_rows, hipStream_t stream) {
  const auto block = quant::QuantizedBlockElements(type);
  if (!IsNativeQuantDense(type) || !rows || !outputs || !cols || cols % block ||
      (matrix_rows && half_scratch == nullptr))
    throw std::invalid_argument("invalid native quantized dense projection");
  // HC down/inject, alpha/beta and indexer projections have few output
  // rows. Keep their long reductions in FP32: a 10240-wide IQ4_XS WMMA
  // probe accumulated 4.7e-5 relative error even after matching half inputs.
  // The exact row-dot path also avoids mostly empty matrix-core tiles.
  if (matrix_rows && NativeQuantDenseF16Route(type, outputs, cols)) {
    gufo::hip::LaunchFloatToFp16(input, half_scratch, rows * cols, stream);
    NativeQuantDenseF16(weights, type, half_scratch, output, rows, outputs,
                        cols, stream);
  } else if (matrix_rows) {
    gufo::hip::LaunchGroupedQuantGEMMFp32(type, weights, input, output, rows,
                                          outputs, cols, stream);
  } else {
    gufo::hip::LaunchBatchedQuantGEMMFp32(type, weights, input, output, rows,
                                          outputs, cols, stream);
  }
}
void NativeQuantDenseF16(const void* weights, core::GgmlType type,
                         const void* input, float* output, std::size_t rows,
                         std::size_t outputs, std::size_t cols,
                         hipStream_t stream) {
  gufo::hip::LaunchBatchedQuantGEMMFp16(type, weights, input, output, rows,
                                        outputs, cols, stream);
}
}  // namespace gufo::models::qwen38_flash_next::rocm
