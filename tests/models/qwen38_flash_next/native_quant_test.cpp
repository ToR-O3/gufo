#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "src/core/quant/ggml_dequant.hpp"
#include "src/models/qwen38_flash_next/cpu_ops.hpp"

using gufo::core::GgmlType;
namespace {
void Require(bool value, const char* message) {
  if (!value)
    throw std::runtime_error(message);
}
void Read(std::ifstream& input, void* output, std::size_t bytes) {
  input.read(static_cast<char*>(output), static_cast<std::streamsize>(bytes));
  Require(bool(input), "truncated independent quantization fixture");
}
using Decoder = void (*)(const void*, float*, std::size_t);
struct Format {
  GgmlType type;
  Decoder decode;
};

void Q5_0Rows() {
  constexpr std::size_t cols = 64, rows = 3, experts = 2;
  std::array<std::uint8_t, cols / 32 * 22 * rows * experts> packed{};
  std::array<float, cols * rows * experts> expected{};
  for (std::size_t b = 0; b < expected.size() / 32; ++b) {
    auto* block = packed.data() + b * 22;
    block[1] = b % 2 ? 0xb0 : 0x30;  // FP16 -/+ 0.125.
    const float d = b % 2 ? -0.125F : 0.125F;
    for (std::size_t j = 0; j < 32; ++j) {
      const unsigned code = (j + b * 7) % 32;
      block[2 + j / 8] |= ((code >> 4) & 1) << (j % 8);
      block[6 + j % 16] |= (code & 15) << (4 * (j / 16));
      expected[b * 32 + j] = d * (int(code) - 16);
    }
  }
  const gufo::models::qwen38_flash_next::TensorRef tensor{
      .data = packed.data(),
      .type = GgmlType::kQ5_0,
      .cols = cols,
      .rows = rows,
      .experts = experts};
  Require(tensor.RowBytes() == 44, "Q5_0 row stride");
  std::array<float, cols> actual{};
  for (std::size_t e = 0; e < experts; ++e)
    for (std::size_t r = 0; r < rows; ++r) {
      gufo::models::qwen38_flash_next::cpu::DequantizeRow(tensor, e, r,
                                                          actual.data());
      for (std::size_t c = 0; c < cols; ++c)
        Require(actual[c] == expected[(e * rows + r) * cols + c],
                "Q5_0 independent model row decoder mismatch");
    }
}
}  // namespace
int main() {
  using namespace gufo::quant;
  try {
    Q5_0Rows();
    const auto path = std::filesystem::path(__FILE__).parent_path() /
                      "fixtures/native_quant.bin";
    std::ifstream input(path, std::ios::binary);
    Require(bool(input), "missing independent quantization fixture");
    std::array<char, 8> magic{};
    Read(input, magic.data(), magic.size());
    Require(std::string_view(magic.data(), magic.size()) == "GGMLQU01",
            "fixture magic");
    const std::array<Format, 9> formats{
        {{GgmlType::kIQ2_S, DequantizeIQ2_S},
         {GgmlType::kIQ3_XXS, DequantizeIQ3_XXS},
         {GgmlType::kIQ3_S, DequantizeIQ3_S},
         {GgmlType::kIQ4_XS, DequantizeIQ4_XS},
         {GgmlType::kIQ4_NL, DequantizeIQ4_NL},
         {GgmlType::kQ2_0, DequantizeQ2_0},
         {GgmlType::kQ4_K, DequantizeQ4_K},
         {GgmlType::kQ5_K, DequantizeQ5_K},
         {GgmlType::kQ6_K, DequantizeQ6_K}}};
    std::uint32_t count = 0;
    Read(input, &count, sizeof(count));
    Require(count == formats.size(), "fixture format count");
    std::size_t checked = 0;
    for (const auto& f : formats) {
      std::array<std::uint32_t, 7> h{};
      Read(input, h.data(), sizeof(h));
      const auto [type, cols, rows, experts, block, block_bytes, size] = h;
      Require(type == static_cast<std::uint32_t>(f.type),
              "fixture format order");
      Require(QuantizedBlockElements(f.type) == block, "packed block width");
      Require(QuantizedRowBytes(f.type, cols) == cols / block * block_bytes,
              "packed row stride");
      Require(size == cols / block * block_bytes * rows * experts,
              "packed expert stride");
      std::vector<std::uint8_t> packed(size);
      std::vector<float> expected(cols * rows * experts), actual(cols);
      Read(input, packed.data(), packed.size());
      Read(input, expected.data(), expected.size() * sizeof(float));
      const gufo::models::qwen38_flash_next::TensorRef tensor{
          .data = packed.data(),
          .type = f.type,
          .cols = cols,
          .rows = rows,
          .experts = experts};
      std::vector<float> model_row(cols);
      for (std::size_t row = 0; row < rows * experts; ++row) {
        gufo::models::qwen38_flash_next::cpu::DequantizeRow(
            tensor, row / rows, row % rows, model_row.data());
        f.decode(packed.data() + row * QuantizedRowBytes(f.type, cols),
                 actual.data(), cols);
        for (std::size_t col = 0; col < cols; ++col) {
          Require(std::isfinite(actual[col]) &&
                      actual[col] == expected[row * cols + col],
                  "independent scalar decoder mismatch");
          Require(model_row[col] == expected[row * cols + col],
                  "independent model row decoder mismatch");
          ++checked;
        }
      }
    }
    Require(input.peek() == std::char_traits<char>::eof(),
            "unexpected fixture tail");
    std::cout << "PASS native GGUF decoders: 9 formats, " << checked
              << " exact independent scalar values\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
