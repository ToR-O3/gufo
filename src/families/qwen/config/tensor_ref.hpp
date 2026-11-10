#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <gufo/ext/experimental/ggml_dequant.hpp>
#include <gufo/ext/gguf_reader.hpp>
#include <limits>

namespace gufo::models {

/// Non-owning reference to a mapped tensor. Consumers validate the exact
/// formats supported for each tensor role.
struct QwenTensorRef {
  const void* data = nullptr;
  core::GgmlType type = core::GgmlType::kF32;
  std::size_t num_elements = 0;
  /// Bytes available from `data` to the end of its mapped storage region.
  /// Synthetic/in-process tensors default to unbounded trusted storage.
  std::size_t available_bytes = std::numeric_limits<std::size_t>::max();

  [[nodiscard]] bool empty() const noexcept {
    return data == nullptr || num_elements == 0;
  }

  [[nodiscard]] std::size_t EncodedSizeBytes() const noexcept {
    return gufo::quant::EncodedSizeBytes(type, num_elements);
  }

  [[nodiscard]] bool FitsAvailableStorage() const noexcept {
    const std::size_t encoded_bytes = EncodedSizeBytes();
    return encoded_bytes != 0 && encoded_bytes <= available_bytes;
  }

  [[nodiscard]] float Get(std::size_t index) const noexcept {
    if (type == core::GgmlType::kF32) {
      return static_cast<const float*>(data)[index];
    }
    if (type == core::GgmlType::kBF16) {
      const auto u16 = static_cast<const std::uint16_t*>(data)[index];
      const std::uint32_t u32 = static_cast<std::uint32_t>(u16) << 16;
      float f = 0.0F;
      std::memcpy(&f, &u32, sizeof(float));
      return f;
    }
    if (type == core::GgmlType::kF16) {
      return gufo::quant::Fp16ToFloat(
          static_cast<const std::uint16_t*>(data)[index]);
    }
    if (type == core::GgmlType::kQ8_K) {
      constexpr std::size_t kBlockSize = 256;
      const std::size_t block_idx = index / kBlockSize;
      const std::size_t pos = index % kBlockSize;
      const auto* block =
          reinterpret_cast<const gufo::quant::block_q8_K*>(data) + block_idx;
      return block->d * static_cast<float>(block->qs[pos]);
    }
    if (type == core::GgmlType::kQ8_0) {
      // Q8_0 block: fp16 scale + 32 int8 values (QK=32, 34 bytes).
      constexpr std::size_t kBlockSize = 32;
      const std::size_t block_idx = index / kBlockSize;
      const std::size_t pos = index % kBlockSize;
      const auto* block =
          reinterpret_cast<const gufo::quant::block_q8_0*>(data) + block_idx;
      return gufo::quant::Fp16ToFloat(block->d) *
             static_cast<float>(block->qs[pos]);
    }
    if (type == core::GgmlType::kQ3_K || type == core::GgmlType::kQ4_K ||
        type == core::GgmlType::kQ5_K || type == core::GgmlType::kQ6_K) {
      // QK=256 block-packed matmul weights. Dequantize the containing block via
      // the canonical (parity-tested) full-row dequant helpers, then index into
      // it. Previously this fell through to a silent 0.0F.
      constexpr std::size_t kBlockSize = 256;
      const std::size_t block_idx = index / kBlockSize;
      const std::size_t pos = index % kBlockSize;
      const std::size_t block_bytes =
          type == core::GgmlType::kQ3_K   ? sizeof(gufo::quant::block_q3_K)
          : type == core::GgmlType::kQ4_K ? sizeof(gufo::quant::block_q4_K)
          : type == core::GgmlType::kQ5_K ? sizeof(gufo::quant::block_q5_K)
                                          : sizeof(gufo::quant::block_q6_K);
      const auto* block_ptr =
          static_cast<const std::uint8_t*>(data) + block_idx * block_bytes;
      float block_buf[kBlockSize];
      switch (type) {
        case core::GgmlType::kQ3_K:
          gufo::quant::DequantizeQ3_K(block_ptr, block_buf, kBlockSize);
          break;
        case core::GgmlType::kQ4_K:
          gufo::quant::DequantizeQ4_K(block_ptr, block_buf, kBlockSize);
          break;
        case core::GgmlType::kQ5_K:
          gufo::quant::DequantizeQ5_K(block_ptr, block_buf, kBlockSize);
          break;
        case core::GgmlType::kQ6_K:
          gufo::quant::DequantizeQ6_K(block_ptr, block_buf, kBlockSize);
          break;
        default:
          break;
      }
      return block_buf[pos];
    }
    // Unsupported (or non-block-aligned) type: fail loudly instead of silently
    // returning 0.0F.
    assert(false && "QwenTensorRef::Get: unsupported GgmlType");
    std::abort();
  }

  [[nodiscard]] std::span<const float> AsFloatSpan() const noexcept {
    if (type == core::GgmlType::kF32 && data != nullptr) {
      return {static_cast<const float*>(data), num_elements};
    }
    return {};
  }
};

}  // namespace gufo::models
