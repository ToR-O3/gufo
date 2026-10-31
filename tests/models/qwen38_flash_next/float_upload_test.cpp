#include "src/models/qwen38_flash_next/kernels/rocm/float_upload.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <iostream>
#include <vector>

using namespace gufo::models::qwen38_flash_next;

int main() {
  using gufo::core::GgmlType;
  void* device = nullptr;
  std::string error;
  // BF16 stack input -> binary16: direct transfer must preserve signed zero
  // and the smallest normal BF16 values that remain representable in F16.
  const std::array<std::uint16_t, 6> bf16{0,      0x8000, 0x3f80,
                                          0xbf80, 0x4000, 0x3880};
  const std::array<std::uint16_t, 6> half{0,      0x8000, 0x3c00,
                                          0xbc00, 0x4000, 0x0400};
  TensorRef small{.data = bf16.data(),
                  .type = GgmlType::kBF16,
                  .cols = 6,
                  .rows = 1,
                  .name = "test.ssm_alpha"};
  assert(hipMalloc(&device, 12) == hipSuccess);
  std::array<gufo::core::GgufMappedRegion, 1> shards{
      {{.data = bf16.data(), .size = sizeof(bf16)}}};
  rocm::FloatUpload upload(shards, &error);
  assert(upload.Copy(small, GgmlType::kF16, 0, device));
  std::array<std::uint16_t, 6> actual{};
  assert(hipMemcpy(actual.data(), device, 12, hipMemcpyDeviceToHost) ==
         hipSuccess);
  assert(actual == half);
  assert(hipFree(device) == hipSuccess);
  // Cross the bounded 8 MiB staging frontier with a ragged final row.
  constexpr std::size_t cols = 65, rows = 65537;
  std::vector<std::uint16_t> large(cols * rows);
  for (std::size_t i = 0; i < large.size(); ++i)
    large[i] = bf16[i % bf16.size()];
  small.data = large.data();
  small.cols = cols;
  small.rows = rows;
  shards[0] = {.data = large.data(), .size = large.size() * 2};
  assert(hipMalloc(&device, large.size() * sizeof(float)) == hipSuccess);
  assert(upload.Copy(small, GgmlType::kF32, 0, device));
  std::vector<float> decoded(large.size());
  assert(hipMemcpy(decoded.data(), device, decoded.size() * sizeof(float),
                   hipMemcpyDeviceToHost) == hipSuccess);
  for (std::size_t i = 0; i < large.size(); ++i)
    assert(std::bit_cast<std::uint32_t>(decoded[i]) ==
           (std::uint32_t(large[i]) << 16));
  assert(upload.scratch_high_water < 18 * 1024 * 1024);
  small.experts = 2;
  assert(!upload.Copy(small, GgmlType::kF32, 0, device));
  assert(!error.empty());
  small.experts = 1;
  assert(!upload.Copy(small, GgmlType::kQ8_0, 0, device));
  large[0] = 0x7f80;
  assert(!upload.Copy(small, GgmlType::kF32, 0, device));
  assert(hipFree(device) == hipSuccess);
  std::cout << "float_upload: BF16->F16 exact; scratch="
            << upload.scratch_high_water << '\n';
}
