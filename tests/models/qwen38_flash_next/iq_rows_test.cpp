#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "src/models/qwen38_flash_next/cpu_ops.hpp"

namespace {
void Require(bool ok, const char* message) {
  if (!ok)
    throw std::runtime_error(message);
}
void Read(std::ifstream& in, void* dst, std::size_t bytes) {
  in.read(static_cast<char*>(dst), bytes);
  Require(bool(in), "truncated upstream scalar fixture");
}
}  // namespace

int main() {
  try {
    using namespace gufo::models::qwen38_flash_next;
    std::ifstream in(std::filesystem::path(__FILE__).parent_path() /
                         "fixtures/iq_expert.bin",
                     std::ios::binary);
    std::array<char, 8> magic{};
    Read(in, magic.data(), magic.size());
    Require(std::string_view(magic.data(), magic.size()) == "GGMLQU01",
            "fixture magic");
    std::uint32_t count;
    Read(in, &count, sizeof(count));
    Require(count == 3, "fixture count");
    for (std::uint32_t i = 0; i < count; ++i) {
      std::array<std::uint32_t, 7> h{};
      Read(in, h.data(), sizeof(h));
      const auto [type, cols, rows, experts, block, bytes, size] = h;
      std::vector<std::uint8_t> packed(size);
      std::vector<float> reference(cols * rows * experts), actual(cols);
      Read(in, packed.data(), size);
      Read(in, reference.data(), reference.size() * sizeof(float));
      TensorRef tensor;
      tensor.data = packed.data();
      tensor.type = static_cast<gufo::core::GgmlType>(type);
      tensor.cols = cols;
      tensor.rows = rows;
      tensor.experts = experts;
      Require(tensor.RowBytes() == cols / block * bytes, "row stride");
      Require(tensor.SizeBytes() == size, "expert stride");
      for (std::uint32_t expert = 0; expert < experts; ++expert)
        for (std::uint32_t row = 0; row < rows; ++row) {
          cpu::DequantizeRow(tensor, expert, row, actual.data());
          const auto begin = reference.begin() + (expert * rows + row) * cols;
          Require(std::equal(actual.begin(), actual.end(), begin),
                  "independent IQ row mismatch");
        }
      std::cout << "PASS type=" << type << " K=" << cols
                << " scalar rows and expert strides\n";
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
