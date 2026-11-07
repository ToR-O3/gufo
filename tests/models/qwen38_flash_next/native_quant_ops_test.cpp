#include <hip/hip_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "src/core/quant/ggml_dequant.hpp"
#include "src/models/qwen38_flash_next/cpu_ops.hpp"
#include "src/models/qwen38_flash_next/kernels/rocm/kernels.hpp"
#include "src/models/qwen38_flash_next/kernels/rocm/native_quant.hpp"

namespace q = gufo::models::qwen38_flash_next;
namespace r = q::rocm;
using gufo::core::GgmlType;
namespace {
void Check(hipError_t e) {
  if (e != hipSuccess)
    throw std::runtime_error(hipGetErrorString(e));
}
struct Buffers {
  std::vector<void*> ptrs;
  ~Buffers() {
    for (auto p : ptrs)
      (void)hipFree(p);
  }
  void* Make(std::size_t n) {
    void* p = nullptr;
    Check(hipMalloc(&p, n));
    ptrs.push_back(p);
    return p;
  }
};
std::uint32_t Next(std::uint32_t& x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}
float Half(float x) {
  return __half2float(__float2half(x));
}
void Case(GgmlType type, std::size_t k, std::size_t m) {
  constexpr std::size_t rows = 257;
  const auto block = gufo::quant::QuantizedBlockElements(type);
  const auto block_bytes = gufo::quant::QuantizedRowBytes(type, block);
  const auto stride = gufo::quant::QuantizedRowBytes(type, k);
  std::uint32_t rng = 0x59323U;
  std::vector<std::uint8_t> packed(m * stride);
  for (auto& b : packed)
    b = static_cast<std::uint8_t>(Next(rng));
  for (std::size_t at = 0; at < packed.size(); at += block_bytes) {
    const std::uint16_t d = 0x1800U + (Next(rng) & 0x7ffU);
    const auto offset = type == GgmlType::kQ6_K ? 208U : 0U;
    std::memcpy(packed.data() + at + offset, &d, 2);
    if (type == GgmlType::kQ4_K || type == GgmlType::kQ5_K) {
      const std::uint16_t dmin = 0x1400U + (Next(rng) & 0x7ffU);
      std::memcpy(packed.data() + at + 2, &dmin, 2);
    }
  }
  std::vector<float> weights(m * k), x(rows * k), got(rows * m);
  q::TensorRef tensor{.data = packed.data(),
                      .type = type,
                      .cols = k,
                      .rows = m,
                      .name = "native-iq4-fixture"};
  for (std::size_t i = 0; i < m; ++i)
    q::cpu::DequantizeRow(tensor, 0, i, weights.data() + i * k);
  // Identical, unsaturated rows reveal batch-dependent dot products.
  for (std::size_t i = 0; i < k; ++i)
    x[i] = (int(Next(rng) % 20001) - 10000) * 0.000073F;
  for (std::size_t t = 1; t < rows; ++t)
    std::copy_n(x.data(), k, x.data() + t * k);
  Buffers b;
  auto dw = b.Make(packed.size());
  auto dx = b.Make(x.size() * 4);
  auto half = b.Make(x.size() * 2);
  auto dy = b.Make(got.size() * 4);
  Check(hipMemcpy(dw, packed.data(), packed.size(), hipMemcpyHostToDevice));
  Check(hipMemcpy(dx, x.data(), x.size() * 4, hipMemcpyHostToDevice));
  std::vector<float> single(m);
  for (bool prefill : {false, true}) {
    r::NativeQuantDense(dw, type, static_cast<float*>(dx), half,
                        static_cast<float*>(dy), 1, m, k, prefill, nullptr);
    Check(hipMemcpy(single.data(), dy, m * 4, hipMemcpyDeviceToHost));
    const bool half_math = prefill && m > 512;
    for (std::size_t n : {1U, 2U, 7U, 8U, 9U, 32U, 33U, 129U, 256U, 257U}) {
      r::NativeQuantDense(dw, type, static_cast<float*>(dx), half,
                          static_cast<float*>(dy), n, m, k, prefill, nullptr);
      Check(hipMemcpy(got.data(), dy, n * m * 4, hipMemcpyDeviceToHost));
      double sq = 0, ref_sq = 0;
      for (std::size_t j = 0; j < m; ++j) {
        double expected = 0;
        for (std::size_t c = 0; c < k; ++c)
          expected += double(half_math ? Half(weights[j * k + c])
                                       : weights[j * k + c]) *
                      double(half_math ? Half(x[c]) : x[c]);
        for (std::size_t t = 0; t < n; ++t) {
          const float v = got[t * m + j];
          if (!std::isfinite(v))
            throw std::runtime_error("nonfinite output");
          sq += (v - expected) * (v - expected);
          ref_sq += expected * expected;
          if (v != single[j])
            throw std::runtime_error("row arithmetic depends on batch width");
        }
      }
      const auto rms = std::sqrt(sq / std::max(ref_sq, 1e-30));
      if (rms > 0.00002) {
        std::cerr << "FAIL " << gufo::core::ToString(type) << " k=" << k
                  << " m=" << m << " n=" << n << " prefill=" << prefill
                  << " rel_rms=" << rms << '\n';
        for (std::size_t j = 0; j < std::min(m, std::size_t{4}); ++j) {
          double expected = 0;
          for (std::size_t c = 0; c < k; ++c)
            expected += double(half_math ? Half(weights[j * k + c])
                                         : weights[j * k + c]) *
                        double(half_math ? Half(x[c]) : x[c]);
          std::cerr << j << " actual=" << got[j] << " expected=" << expected
                    << '\n';
        }
        throw std::runtime_error("independent CPU dot tolerance exceeded");
      }
      std::cout << gufo::core::ToString(type) << " k=" << k << " m=" << m
                << " n=" << n << " prefill=" << prefill << " rel_rms=" << rms
                << '\n';
    }
  }
  if (type == GgmlType::kIQ4_XS) {
    const std::int32_t ids[] = {static_cast<std::int32_t>(m - 1), 0, 1};
    auto dt = b.Make(sizeof(ids));
    auto de = b.Make(3 * 4 * k * sizeof(float));
    Check(hipMemcpy(dt, ids, sizeof(ids), hipMemcpyHostToDevice));
    r::EmbedTokens(dw, r::WeightType::kIQ4_XS, static_cast<std::int32_t*>(dt),
                   static_cast<float*>(de), 3, k, 4, nullptr);
    std::vector<float> emb(3 * 4 * k);
    Check(hipMemcpy(emb.data(), de, emb.size() * 4, hipMemcpyDeviceToHost));
    for (std::size_t t = 0; t < 3; ++t)
      for (std::size_t s = 0; s < 4; ++s)
        for (std::size_t c = 0; c < k; ++c)
          if (emb[(t * 4 + s) * k + c] != weights[ids[t] * k + c])
            throw std::runtime_error(
                "embedding differs from CPU dequantization");
  }
}

// The IQ4_NL shared expert already narrows its SwiGLU output before the
// down projection. A private staging buffer must preserve the same
// dot product, including a one-token prefill tail and the 4K chunk boundary.
void SharedExpertHalfCase(std::size_t rows) {
  constexpr std::size_t k = 640, m = 2560;
  constexpr auto type = GgmlType::kIQ4_NL;
  const auto stride = gufo::quant::QuantizedRowBytes(type, k);
  const auto block_bytes = gufo::quant::QuantizedRowBytes(type, 32);
  std::uint32_t rng = 0xC47139U;
  std::vector<std::uint8_t> packed(m * stride);
  for (auto& v : packed)
    v = static_cast<std::uint8_t>(Next(rng));
  for (std::size_t at = 0; at < packed.size(); at += block_bytes) {
    const std::uint16_t scale = 0x1800U + (Next(rng) & 0x7ffU);
    std::memcpy(packed.data() + at, &scale, sizeof(scale));
  }
  std::vector<float> weights(m * k), gate(rows * k), up(rows * k);
  q::TensorRef tensor{.data = packed.data(),
                      .type = type,
                      .cols = k,
                      .rows = m,
                      .name = "native-iq4-shared-expert-fixture"};
  for (std::size_t j = 0; j < m; ++j)
    q::cpu::DequantizeRow(tensor, 0, j, weights.data() + j * k);
  for (std::size_t i = 0; i < k; ++i) {
    gate[i] = (int(Next(rng) % 20001) - 10000) * 0.00037F;
    up[i] = (int(Next(rng) % 20001) - 10000) * 0.000071F;
  }
  gate[0] = 0.0F;
  gate[1] = -0.0F;
  gate[2] = 0.00001F;
  gate[3] = -0.00001F;
  for (std::size_t t = 1; t < rows; ++t) {
    std::copy_n(gate.data(), k, gate.data() + t * k);
    std::copy_n(up.data(), k, up.data() + t * k);
  }
  Buffers b;
  auto dw = b.Make(packed.size());
  auto dg = static_cast<float*>(b.Make(gate.size() * sizeof(float)));
  auto du = static_cast<float*>(b.Make(up.size() * sizeof(float)));
  auto old_gate = static_cast<float*>(b.Make(gate.size() * sizeof(float)));
  auto old_half = b.Make(gate.size() * sizeof(std::uint16_t));
  auto direct_half =
      static_cast<__half*>(b.Make(gate.size() * sizeof(std::uint16_t)));
  auto old_out = static_cast<float*>(b.Make(rows * m * sizeof(float)));
  auto direct_out = static_cast<float*>(b.Make(rows * m * sizeof(float)));
  Check(hipMemcpy(dw, packed.data(), packed.size(), hipMemcpyHostToDevice));
  Check(hipMemcpy(dg, gate.data(), gate.size() * sizeof(float),
                  hipMemcpyHostToDevice));
  Check(hipMemcpy(old_gate, dg, gate.size() * sizeof(float),
                  hipMemcpyDeviceToDevice));
  Check(hipMemcpy(du, up.data(), up.size() * sizeof(float),
                  hipMemcpyHostToDevice));
  r::Swiglu(old_gate, du, gate.size(), nullptr);
  r::NativeQuantDense(dw, type, old_gate, old_half, old_out, rows, m, k, true,
                      nullptr);
  r::SwigluHalf(dg, du, direct_half, gate.size(), true, nullptr);
  r::NativeQuantDenseF16(dw, type, direct_half, direct_out, rows, m, k,
                         nullptr);
  std::vector<std::uint16_t> ref_half(gate.size()), got_half(gate.size());
  std::vector<float> ref(rows * m), got(rows * m);
  Check(hipMemcpy(ref_half.data(), old_half, ref_half.size() * 2,
                  hipMemcpyDeviceToHost));
  Check(hipMemcpy(got_half.data(), direct_half, got_half.size() * 2,
                  hipMemcpyDeviceToHost));
  Check(hipMemcpy(ref.data(), old_out, ref.size() * 4, hipMemcpyDeviceToHost));
  Check(
      hipMemcpy(got.data(), direct_out, got.size() * 4, hipMemcpyDeviceToHost));
  if (got_half != ref_half ||
      std::memcmp(got.data(), ref.data(), got.size() * sizeof(float)) != 0) {
    for (std::size_t i = 0; i < got_half.size(); ++i) {
      if (got_half[i] != ref_half[i]) {
        std::cerr << "half mismatch n=" << rows << " i=" << i
                  << " gate=" << gate[i] << " up=" << up[i]
                  << " old=" << std::hex << ref_half[i]
                  << " direct=" << got_half[i] << std::dec << '\n';
        break;
      }
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
      if (std::memcmp(&got[i], &ref[i], sizeof(float)) != 0) {
        std::cerr << "output mismatch n=" << rows << " i=" << i
                  << " old=" << std::hexfloat << ref[i] << " direct=" << got[i]
                  << std::defaultfloat << '\n';
        break;
      }
    }
    throw std::runtime_error("private shared-expert input changed output bits");
  }
  // Independent FP64 dot over CPU-dequantized, F16-rounded weights and
  // the validated F16 activation bytes; the existing dense tolerance.
  double sq = 0.0, ref_sq = 0.0;
  for (std::size_t j = 0; j < m; ++j) {
    double expected = 0.0;
    for (std::size_t i = 0; i < k; ++i)
      expected += double(Half(weights[j * k + i])) *
                  double(gufo::quant::Fp16ToFloat(got_half[i]));
    for (std::size_t t = 0; t < rows; ++t) {
      const float v = got[t * m + j];
      if (!std::isfinite(v))
        throw std::runtime_error("nonfinite shared-expert output");
      sq += (v - expected) * (v - expected);
      ref_sq += expected * expected;
    }
  }
  const double rms = std::sqrt(sq / std::max(ref_sq, 1e-30));
  std::cout << "IQ4_NL shared expert n=" << rows
            << " exact_half=1 exact_output=1 rel_rms=" << rms << '\n';
  if (rms > 0.00002)
    throw std::runtime_error("shared-expert CPU dot tolerance exceeded");
}

}  // namespace
int main() {
  try {
    for (std::size_t rows : {1U, 8U, 9U, 32U, 33U, 129U, 257U, 4096U})
      SharedExpertHalfCase(rows);
    Case(GgmlType::kIQ4_XS, 2560, 1057);
    Case(GgmlType::kIQ4_XS, 2560, 4097);
    Case(GgmlType::kIQ4_XS, 2560, 48);
    Case(GgmlType::kIQ4_XS, 10240, 320);
    Case(GgmlType::kIQ4_XS, 10240, 4);
    Case(GgmlType::kIQ4_NL, 320, 1057);
    Case(GgmlType::kIQ4_NL, 320, 4097);
    Case(GgmlType::kIQ4_NL, 640, 1057);
    Case(GgmlType::kQ5_K, 2560, 512);
    Case(GgmlType::kQ5_K, 2560, 1057);
    Case(GgmlType::kQ5_K, 2560, 6144);
    Case(GgmlType::kQ4_K, 2560, 1057);
    Case(GgmlType::kQ4_K, 6144, 2560);
    Case(GgmlType::kQ6_K, 2560, 1057);
    Case(GgmlType::kQ6_K, 6144, 2560);
    std::cout << "PASS native quantized dense / embedding, exact allocation "
                 "boundaries\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
