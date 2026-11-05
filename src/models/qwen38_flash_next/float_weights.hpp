#ifndef GUFO_MODELS_QWEN38_FLASH_NEXT_FLOAT_WEIGHTS_HPP_
#define GUFO_MODELS_QWEN38_FLASH_NEXT_FLOAT_WEIGHTS_HPP_

#include <cstdint>
#include <gufo/ext/gguf_reader.hpp>
#include <span>

namespace gufo::models::qwen38_flash_next {
// Bounded floating-weight upload uses one explicit round-to-nearest-even step.
std::uint16_t FloatToHalf(float value);
void DecodeFloatRow(core::GgmlType type, const void* source,
                    std::span<float> values);
}  // namespace gufo::models::qwen38_flash_next
#endif
