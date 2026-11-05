#ifndef GUFO_MODELS_QWEN38_FLASH_NEXT_ROCM_FLOAT_UPLOAD_HPP_
#define GUFO_MODELS_QWEN38_FLASH_NEXT_ROCM_FLOAT_UPLOAD_HPP_

#include <hip/hip_runtime.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <gufo/ext/gguf_reader.hpp>
#include <gufo/ext/sha256.hpp>
#include <iostream>
#include <span>
#include <stdexcept>
#include <system_error>
#include <vector>

#include "src/models/qwen38_flash_next/float_weights.hpp"
#include "src/models/qwen38_flash_next/weights.hpp"

namespace gufo::models::qwen38_flash_next::rocm {

// Separate from the existing raw WeightUpload pool. Source GGUF mappings
// remain untouched: ReadAt -> bounded source -> one F32 row -> pinned output.
// Each synchronous HIP copy completes before the pinned output is reused.
class FloatUpload {
public:
  FloatUpload(std::span<const core::GgufMappedRegion> shards,
              std::string* error)
      : shards_(shards), error_(error) {}
  ~FloatUpload() {
    if (pinned_)
      (void)hipHostFree(pinned_);
  }
  std::size_t scratch_high_water{0};
  static constexpr std::size_t kBufferBytes = 8 * 1024 * 1024;

  static std::size_t RowBytes(const TensorRef& t, core::GgmlType type) {
    return t.cols * (type == core::GgmlType::kF32 ? 4 : 2);
  }

  bool Copy(const TensorRef& t, core::GgmlType destination_type,
            std::uint32_t shard_base, void* destination) {
    const auto start = std::chrono::steady_clock::now();
    try {
      if (t.experts != 1 || t.shard + shard_base >= shards_.size())
        throw std::runtime_error(
            "conversion requires an ordinary matrix and valid shard");
      if (destination_type != core::GgmlType::kF16 &&
          destination_type != core::GgmlType::kF32)
        throw std::runtime_error("unsupported conversion destination");
      const auto src_row = t.RowBytes();
      const auto dst_row = RowBytes(t, destination_type);
      if (!src_row || !dst_row || src_row > kBufferBytes ||
          dst_row > kBufferBytes)
        throw std::runtime_error("conversion row exceeds bounded scratch");
      if (!pinned_) {
        source_.resize(kBufferBytes);
        if (hipHostMalloc(reinterpret_cast<void**>(&pinned_), kBufferBytes) !=
            hipSuccess)
          throw std::runtime_error("floating weight pinned allocation failed");
      }
      values_.resize(t.cols);
      scratch_high_water =
          std::max(scratch_high_water, source_.capacity() + kBufferBytes +
                                           values_.capacity() * sizeof(float));
      if (scratch_high_water > 64000000)
        throw std::runtime_error(
            "floating weight scratch exceeds 64000000 bytes");
      const auto batch =
          std::min(kBufferBytes / src_row, kBufferBytes / dst_row);
      const auto& shard = shards_[t.shard + shard_base];
      crypto::Sha256Hasher digest;
      for (std::size_t first = 0; first < t.rows; first += batch) {
        const auto count = std::min<std::size_t>(batch, t.rows - first);
        const auto input_bytes = count * src_row;
        if (platform::FileValid(shard.file_descriptor)) {
          std::size_t done = 0;
          while (done < input_bytes) {
            std::error_code ec;
            const auto n = platform::ReadAt(
                shard.file_descriptor, t.file_offset + first * src_row + done,
                source_.data() + done, input_bytes - done, ec);
            if (n == 0 && ec == std::errc::interrupted)
              continue;
            if (n == 0)
              throw std::runtime_error("floating weight source read failed");
            done += n;
          }
        } else {
          std::memcpy(
              source_.data(),
              static_cast<const std::uint8_t*>(t.data) + first * src_row,
              input_bytes);
        }
        for (std::size_t row = 0; row < count; ++row) {
          DecodeFloatRow(t.type, source_.data() + row * src_row, values_);
          auto* output = pinned_ + row * dst_row;
          if (destination_type == core::GgmlType::kF32) {
            std::memcpy(output, values_.data(), dst_row);
          } else {
            for (std::size_t i = 0; i < t.cols; ++i) {
              const auto h = FloatToHalf(values_[i]);
              std::memcpy(output + i * 2, &h, 2);
            }
          }
        }
        const auto output_bytes = count * dst_row;
        digest.Update({pinned_, output_bytes});
        const auto status =
            hipMemcpy(static_cast<std::uint8_t*>(destination) + first * dst_row,
                      pinned_, output_bytes, hipMemcpyHostToDevice);
        if (status != hipSuccess)
          throw std::runtime_error(hipGetErrorString(status));
      }
      std::clog << "weight_conversion name=" << t.name
                << " source=" << core::ToString(t.type)
                << " runtime=" << core::ToString(destination_type)
                << " source_bytes=" << t.SizeBytes()
                << " runtime_bytes=" << dst_row * t.rows
                << " sha256=" << digest.FinishHex() << " seconds="
                << std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - start)
                       .count()
                << '\n';
      return true;
    } catch (const std::exception& ex) {
      if (error_)
        *error_ = "conversion of " + std::string(t.name) + ": " + ex.what();
      return false;
    }
  }

private:
  std::span<const core::GgufMappedRegion> shards_;
  std::string* error_;
  std::vector<std::uint8_t> source_;
  std::vector<float> values_;
  std::uint8_t* pinned_{nullptr};
};
}  // namespace gufo::models::qwen38_flash_next::rocm
#endif
