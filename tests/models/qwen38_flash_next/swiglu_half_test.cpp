#include <hip/hip_fp16.h>
#include <hip/hip_runtime.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "src/models/qwen38_flash_next/kernels/rocm/kernels.hpp"
namespace r = gufo::models::qwen38_flash_next::rocm;

#define CHECK(x)                                              \
  do {                                                        \
    auto e = (x);                                             \
    if (e != hipSuccess) {                                    \
      std::fprintf(stderr, "HIP %s\n", hipGetErrorString(e)); \
      std::exit(2);                                           \
    }                                                         \
  } while (0)
uint32_t Next(uint32_t& v) {
  v ^= v << 13;
  v ^= v >> 17;
  v ^= v << 5;
  return v;
}
float FloatHalf(uint16_t bits) {
  return __half2float(std::bit_cast<__half>(bits));
}
int main() {
  std::vector<float> g, u;
  auto add = [&](float a, float b) {
    g.push_back(a);
    u.push_back(b);
  };
  // Preserve the original failing lane and add signs, subnormals and
  // saturation.
  add(0.f, .3f);
  add(-0.f, .217899f);
  add(.00001f, .2f);
  add(-.00001f, .2f);
  const float edge[] = {
      0.f,       -0.f,       1.f,       -1.f,       0.217899f, -0.217899f,
      0x1p-149f, -0x1p-149f, 0x1p-126f, -0x1p-126f, 0x1p-24f,  -0x1p-24f,
      20.f,      -20.f,      80.f,      -80.f,      100.f,     -100.f};
  for (float a : edge)
    for (float b : edge)
      add(a, b);
  // Every finite FP16 input value at eight signs/magnitudes.
  for (unsigned a = 0; a < 65536; ++a) {
    if ((a & 0x7c00) == 0x7c00)
      continue;
    for (float b :
         {0.f, -0.f, 1.f, -1.f, .217899f, -.217899f, 0x1p-20f, -0x1p-20f})
      add(FloatHalf(a), b);
  }
  uint32_t seed = 0xC47139;
  for (size_t i = 0; i < 1048576; ++i) {
    float a = (int(Next(seed) % 2000001) - 1000000) * .0001f;
    float b = (int(Next(seed) % 2000001) - 1000000) * .00001f;
    add(a, b);
  }
  // Non-vector-sized final allocation tests the scalar tail.
  add(-0.f, 1.f);
  add(-0x1p-24f, 0x1p-4f);
  add(-4.f, .217899f);
  size_t n = g.size();
  float *dg, *du, *df;
  __half *ref, *out;
  CHECK(hipMalloc(&dg, n * 4));
  CHECK(hipMalloc(&du, n * 4));
  CHECK(hipMalloc(&df, n * 4));
  CHECK(hipMalloc(&ref, n * 2));
  CHECK(hipMalloc(&out, n * 2));
  CHECK(hipMemcpy(dg, g.data(), n * 4, hipMemcpyHostToDevice));
  CHECK(hipMemcpy(du, u.data(), n * 4, hipMemcpyHostToDevice));
  CHECK(hipMemcpy(df, dg, n * 4, hipMemcpyDeviceToDevice));
  r::Swiglu(df, du, n, nullptr);
  r::NarrowActivations(df, ref, false, n, nullptr);
  std::vector<uint16_t> a(n), b(n);
  std::vector<float> f(n);
  CHECK(hipMemcpy(a.data(), ref, n * 2, hipMemcpyDeviceToHost));
  CHECK(hipMemcpy(f.data(), df, n * 4, hipMemcpyDeviceToHost));
  std::printf("elements=%zu tail=%zu\n", n, n % 4);
  auto inspect = [&](const char* name) {
    CHECK(hipGetLastError());
    CHECK(hipMemcpy(b.data(), out, n * 2, hipMemcpyDeviceToHost));
    size_t mismatches = 0, zero = 0, nonzero = 0;
    for (size_t i = 0; i < n; ++i)
      if (a[i] != b[i]) {
        ++mismatches;
        if ((a[i] & 0x7fff) == 0 && (b[i] & 0x7fff) == 0)
          ++zero;
        else
          ++nonzero;
        if (mismatches <= 5 || (nonzero <= 5 && (a[i] & 0x7fff) != 0))
          std::printf("%s i=%zu g=%a u=%a f32=%08x ref=%04x got=%04x\n", name,
                      i, g[i], u[i], std::bit_cast<uint32_t>(f[i]), a[i], b[i]);
      }
    std::printf("RESULT %s mismatch=%zu zero_sign=%zu other=%zu\n", name,
                mismatches, zero, nonzero);
    return mismatches;
  };
  r::SwigluHalf(dg, du, out, n, nullptr);
  inspect("gufo-original");
  r::SwigluHalf(dg, du, out, n, true, nullptr);
  const auto candidate_mismatches = inspect("gufo-preserve-f32");
  if (candidate_mismatches != 0)
    return 1;
  CHECK(hipFree(dg));
  CHECK(hipFree(du));
  CHECK(hipFree(df));
  CHECK(hipFree(ref));
  CHECK(hipFree(out));
}
