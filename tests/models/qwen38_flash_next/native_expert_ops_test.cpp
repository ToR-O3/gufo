#include <hip/hip_runtime.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "tests/models/qwen38_flash_next/iq_expert_test.hpp"

namespace {
void Require(bool ok, const char* message) {
  if (!ok)
    throw std::runtime_error(message);
}
void Hip(hipError_t status) {
  Require(status == hipSuccess, hipGetErrorString(status));
}
void Read(std::ifstream& in, void* dst, std::size_t bytes) {
  in.read(static_cast<char*>(dst), bytes);
  Require(bool(in), "truncated upstream scalar fixture");
}
struct Device {
  void* p{};
  explicit Device(std::size_t bytes) { Hip(hipMalloc(&p, bytes)); }
  ~Device() { (void)hipFree(p); }
  template<class T>
  T* As() {
    return static_cast<T*>(p);
  }
};
void FractionalDots() {
  for (const auto [type, bytes, grid, expected] :
       {std::array<float, 4>{22, 82, 1, 5.375F},
        std::array<float, 4>{18, 98, 5, 15.5F}}) {
    std::vector<std::uint8_t> packed(bytes, 0);
    packed[1] = 0x3c;
    packed[2] = grid;
    std::array<float, 256> input{};
    input[30] = 127;
    input[31] = -127;
    std::int32_t id = 0;
    Device w(bytes), x(sizeof(input)), ids(sizeof(id)), out(sizeof(float));
    Hip(hipMemcpy(w.p, packed.data(), bytes, hipMemcpyHostToDevice));
    Hip(hipMemcpy(ids.p, &id, sizeof(id), hipMemcpyHostToDevice));
    for (float sign : {-1.0F, 1.0F}) {
      input[0] = sign;
      Hip(hipMemcpy(x.p, input.data(), sizeof(input), hipMemcpyHostToDevice));
      Require(qfn_mmq_moe_vec(type, w.p, x.As<float>(), ids.As<std::int32_t>(),
                              out.As<float>(), 1, 256, 1, 1, 1, nullptr) == 0,
              "fractional dot dispatch");
      float actual;
      Hip(hipMemcpy(&actual, out.p, sizeof(actual), hipMemcpyDeviceToHost));
      Require(actual == sign * expected,
              "fractional IQ subscale was truncated");
    }
  }
}
void Case(int type, int cols, int rows, const std::vector<std::uint8_t>& packed,
          const std::vector<float>& reference, bool paired = false,
          int used = 2) {
  constexpr int experts = 512, tokens = 33;
  const auto stride = packed.size() / 2;
  // Match DeviceModel's tensor-level zero tail, including K640 IQ4_NL.
  Device w(experts * stride + 4096), x(tokens * cols * 4),
      ids(tokens * used * 4), y(tokens * used * rows * 4),
      y_b(paired ? tokens * used * rows * 4 : 4);
  Hip(hipMemset(w.p, 0, experts * stride + 4096));
  Hip(hipMemcpy(w.p, packed.data(), stride, hipMemcpyHostToDevice));
  Hip(hipMemcpy(w.As<std::uint8_t>() + 511 * stride, packed.data() + stride,
                stride, hipMemcpyHostToDevice));
  std::vector<float> input(tokens * cols);
  for (std::size_t i = 0; i < input.size(); ++i)
    input[i] = float(int((i * 37) % 255) - 127) / 128;
  // Exactly representable scale and codes isolate packed weight arithmetic.
  for (std::size_t i = 0; i < input.size(); i += 32)
    input[i] = 127.0F / 128;
  std::vector<std::int32_t> routing(tokens * used);
  for (std::size_t i = 0; i < routing.size(); ++i)
    routing[i] = (i % 3 == 0 ? -1 : i % 2 == 0 ? 0 : 511);
  Hip(hipMemcpy(x.p, input.data(), input.size() * 4, hipMemcpyHostToDevice));
  Hip(hipMemcpy(ids.p, routing.data(), routing.size() * 4,
                hipMemcpyHostToDevice));
  for (bool tiled : {false, true})
    for (int n : {1, 8, 9, 32, 33}) {
      // The vector adapter has optional pair arguments; call it explicitly.
      int rc = tiled ? gufo::test::NativeExpertProjection(
                           type, w.p, x.As<float>(), ids.As<std::int32_t>(),
                           y.As<float>(), rows, cols, n, experts, used, nullptr)
                     : qfn_mmq_moe_vec(type, w.p, x.As<float>(),
                                       ids.As<std::int32_t>(), y.As<float>(),
                                       rows, cols, n, experts, used, nullptr,
                                       paired ? w.p : nullptr,
                                       paired ? y_b.As<float>() : nullptr);
      Require(rc == 0, "native expert dispatch failed");
      std::vector<float> actual(n * used * rows);
      Hip(hipMemcpy(actual.data(), y.p, actual.size() * 4,
                    hipMemcpyDeviceToHost));
      if (paired && !tiled) {
        std::vector<float> actual_b(actual.size());
        Hip(hipMemcpy(actual_b.data(), y_b.p, actual_b.size() * 4,
                      hipMemcpyDeviceToHost));
        Require(actual_b == actual, "paired expert projection mismatch");
      }
      double worst = 0;
      for (int s = 0; s < n * used; ++s)
        for (int row = 0; row < rows; ++row) {
          double expected = 0, absolute = 0;
          if (routing[s] >= 0)
            for (int col = 0; col < cols; ++col) {
              const auto expert = routing[s] == 0 ? 0 : 1;
              double product =
                  double(reference[(expert * rows + row) * cols + col]) *
                  input[(s / used) * cols + col];
              expected += product;
              absolute += std::abs(product);
            }
          const double u = std::ldexp(1.0, -24), ops = 2.0 * cols + 32;
          const double limit = ops * u / (1 - ops * u) * absolute + 1e-7;
          const double error =
              std::abs(double(actual[s * rows + row]) - expected);
          Require(std::isfinite(actual[s * rows + row]) && error <= limit,
                  "independent packed expert dot mismatch");
          worst = std::max(worst, error);
        }
      std::cout << "type=" << type << " K=" << cols << " M=" << rows
                << " experts=512 used=" << used << " n=" << n
                << " tiled=" << tiled << " max_abs=" << worst << '\n';
    }
}

void Q5_0Dots() {
  // Encode every five-bit code directly; the reference does not call a
  // production decoder. Include both signs of the FP16 block scale.
  for (const auto [cols, rows] :
       {std::array<int, 2>{640, 7}, {2560, 7}, {640, 2560}, {2560, 640}}) {
    constexpr int experts = 2;
    std::vector<std::uint8_t> packed(experts * rows * (cols / 32) * 22, 0);
    std::vector<float> reference(experts * rows * cols);
    for (std::size_t b = 0; b < reference.size() / 32; ++b) {
      auto* block = packed.data() + b * 22;
      block[1] = b % 2 ? 0xb0 : 0x30;  // FP16 -/+ 0.125.
      const float d = b % 2 ? -0.125F : 0.125F;
      for (unsigned j = 0; j < 32; ++j) {
        const unsigned code = (j * 13 + b * 7) % 32;
        block[2 + j / 8] |= ((code >> 4) & 1) << (j % 8);
        block[6 + j % 16] |= (code & 15) << (4 * (j / 16));
        reference[b * 32 + j] = d * (int(code) - 16);
      }
    }
    Case(static_cast<int>(gufo::core::GgmlType::kQ5_0), cols, rows, packed,
         reference, true, cols == 640 && rows == 2560 ? 1 : 2);
    if (cols == 640)
      Case(static_cast<int>(gufo::core::GgmlType::kQ5_0), cols, rows, packed,
           reference, false, 1);
  }
}
}  // namespace
int main() {
  try {
    Require(qfn_mmq_init(0) == 0, "MMQ init");
    Q5_0Dots();
    FractionalDots();
    std::ifstream in(std::filesystem::path(__FILE__).parent_path() /
                         "fixtures/native_quant.bin",
                     std::ios::binary);
    std::array<char, 8> magic{};
    Read(in, magic.data(), 8);
    Require(std::string_view(magic.data(), 8) == "GGMLQU01", "fixture magic");
    std::uint32_t count;
    Read(in, &count, 4);
    Require(count == 9, "fixture count");
    for (std::uint32_t i = 0; i < count; ++i) {
      std::array<std::uint32_t, 7> h{};
      Read(in, h.data(), sizeof(h));
      const auto [type, cols, rows, experts, block, bytes, size] = h;
      Require(experts == 2, "fixture expert count");
      std::vector<std::uint8_t> packed(size);
      std::vector<float> ref(cols * rows * experts);
      Read(in, packed.data(), size);
      Read(in, ref.data(), ref.size() * 4);
      if (i < 6)
        Case(type, cols, rows, packed, ref);
      if (type == static_cast<unsigned>(gufo::core::GgmlType::kIQ4_NL) ||
          type == static_cast<unsigned>(gufo::core::GgmlType::kQ2_0))
        // Down-view inputs are independent per slot. Three fixture rows
        // also exercise the partial tail of a four-row wave.
        Case(type, cols, rows, packed, ref, false, 1);
    }
    std::cout << "PASS independent native expert vector/prefill dots and "
                 "fractional goldens\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
