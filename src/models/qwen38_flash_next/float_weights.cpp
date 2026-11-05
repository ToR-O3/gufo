#include "src/models/qwen38_flash_next/float_weights.hpp"

#include <bit>
#include <cmath>
#include <cstring>
#include <gufo/ext/experimental/ggml_dequant.hpp>
#include <stdexcept>

namespace gufo::models::qwen38_flash_next {
std::uint16_t FloatToHalf(float value) {
  // IEEE binary32 -> binary16, nearest ties to even, independent of fenv.
  if (!std::isfinite(value))
    throw std::runtime_error("nonfinite weight in floating weight conversion");
  const auto bits = std::bit_cast<std::uint32_t>(value);
  const auto sign = static_cast<std::uint16_t>((bits >> 16) & 0x8000U);
  const int exponent = static_cast<int>((bits >> 23) & 255U) - 127 + 15;
  std::uint32_t mantissa = bits & 0x7fffffU;
  if (exponent >= 31)
    throw std::runtime_error("weight exceeds FP16 range");
  if (exponent <= 0) {
    if (exponent < -10)
      return sign;
    mantissa |= 0x800000U;
    const int shift = 14 - exponent;
    const auto rounded =
        (mantissa + ((1U << (shift - 1)) - 1U) + ((mantissa >> shift) & 1U)) >>
        shift;
    return static_cast<std::uint16_t>(sign | rounded);
  }
  mantissa += 0xfffU + ((mantissa >> 13) & 1U);
  const auto rounded =
      static_cast<std::uint32_t>(exponent << 10) + (mantissa >> 13);
  if (rounded >= 0x7c00U)
    throw std::runtime_error("weight rounds outside FP16 range");
  return static_cast<std::uint16_t>(sign | rounded);
}

void DecodeFloatRow(core::GgmlType type, const void* source,
                    std::span<float> values) {
  using core::GgmlType;
  switch (type) {
    case GgmlType::kF32:
      std::memcpy(values.data(), source, values.size_bytes());
      break;
    case GgmlType::kF16:
    case GgmlType::kBF16: {
      const auto* bytes = static_cast<const std::uint8_t*>(source);
      for (std::size_t i = 0; i < values.size(); ++i) {
        std::uint16_t h;
        std::memcpy(&h, bytes + i * 2, 2);
        values[i] =
            type == GgmlType::kF16
                ? quant::Fp16ToFloat(h)
                : std::bit_cast<float>(static_cast<std::uint32_t>(h) << 16);
      }
      break;
    }
    default:
      throw std::invalid_argument("unsupported floating weight type");
  }
  for (float v : values)
    if (!std::isfinite(v))
      throw std::runtime_error("nonfinite floating weight");
}
}  // namespace gufo::models::qwen38_flash_next
