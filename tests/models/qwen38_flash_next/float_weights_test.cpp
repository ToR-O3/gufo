#include "src/models/qwen38_flash_next/float_weights.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace q = gufo::models::qwen38_flash_next;

int main() {
  const std::array<std::pair<float, std::uint16_t>, 10> cases{
      {{0.0F, 0x0000},
       {-0.0F, 0x8000},
       {1.0F, 0x3c00},
       {-1.0F, 0xbc00},
       {65504.0F, 0x7bff},
       {0x1p-14F, 0x0400},
       {0x1p-24F, 0x0001},
       {0x1p-25F, 0x0000},
       {1.0F + 0x1p-11F, 0x3c00},
       {1.0F + 3.0F * 0x1p-11F, 0x3c02}}};
  for (const auto& [value, bits] : cases)
    assert(q::FloatToHalf(value) == bits);
  for (float value : {65520.0F, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
    bool rejected = false;
    try {
      (void)q::FloatToHalf(value);
    } catch (const std::runtime_error&) {
      rejected = true;
    }
    assert(rejected);
  }
  const std::array<std::uint16_t, 6> bf16{0,      0x8000, 0x3f80,
                                          0xbf80, 0x4000, 0x3880};
  std::array<float, 6> values{};
  q::DecodeFloatRow(gufo::core::GgmlType::kBF16, bf16.data(), values);
  const std::array<float, 6> expected{0.0F, -0.0F, 1.0F, -1.0F, 2.0F, 0x1p-14F};
  for (std::size_t i = 0; i < values.size(); ++i)
    assert(std::bit_cast<std::uint32_t>(values[i]) ==
           std::bit_cast<std::uint32_t>(expected[i]));
  bool rejected = false;
  try {
    q::DecodeFloatRow(gufo::core::GgmlType::kQ8_0, bf16.data(), values);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);
}
