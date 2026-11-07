// Diagnostic timing of native expert-vector down projections. Rotate the
// selected experts between trials to avoid a cache-resident weight benchmark.
// Usage: native_expert_bench [GGML type id: 6, 7, 8, 20, 42] [repetitions]
#include <hip/hip_fp16.h>
#include <hip/hip_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h"

namespace {
void Check(hipError_t e) {
  if (e != hipSuccess)
    throw std::runtime_error(hipGetErrorString(e));
}
struct Buffer {
  void* p{};
  explicit Buffer(std::size_t n) { Check(hipMalloc(&p, n)); }
  ~Buffer() { (void)hipFree(p); }
  template<class T>
  T* As() {
    return static_cast<T*>(p);
  }
};
std::uint32_t Next(std::uint32_t& x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}
}  // namespace

int main(int argc, char** argv) try {
  const int type = argc > 1 ? std::atoi(argv[1]) : 6;
  const int reps = argc > 2 ? std::atoi(argv[2]) : 31;
  if (reps < 3 ||
      (type != 6 && type != 7 && type != 8 && type != 20 && type != 42))
    throw std::runtime_error(
        "expected type 6/7/8/20/42 and at least 3 repetitions");
  constexpr int k = 640, m = 2560, experts = 512, max_tokens = 641;
  const int elems = type == 42 ? 64 : 32;
  const int bytes = type == 6 ? 22 : type == 7 ? 24 : type == 8 ? 34 : 18;
  std::vector<std::uint8_t> weights(std::size_t(experts) * m * k / elems *
                                    bytes);
  std::uint32_t rng = 0x13579U;
  for (auto& b : weights)
    b = static_cast<std::uint8_t>(Next(rng));
  for (std::size_t i = 0; i < weights.size(); i += bytes) {
    const __half d = __float2half(0.0015F);
    std::memcpy(weights.data() + i, &d, 2);
    if (type == 7) {
      const __half offset = __float2half(-16 * __half2float(d));
      std::memcpy(weights.data() + i + 2, &offset, 2);
    }
  }
  std::vector<float> input(max_tokens * k);
  for (auto& v : input)
    v = (int(Next(rng) % 20001) - 10000) * 0.000073F;
  Buffer w(weights.size()), x(input.size() * 4), ids(max_tokens * 4),
      out(max_tokens * m * 4);
  Check(hipMemcpy(w.p, weights.data(), weights.size(), hipMemcpyHostToDevice));
  Check(hipMemcpy(x.p, input.data(), input.size() * 4, hipMemcpyHostToDevice));
  if (qfn_mmq_init(0) != 0)
    throw std::runtime_error("MMQ initialization failed");
  hipEvent_t begin, end;
  Check(hipEventCreate(&begin));
  Check(hipEventCreate(&end));
  for (bool shared : {false, true}) {
    for (int tokens : {1, 8, 9, 10, 20, 21, 32, 33, 80, 160, 320, 640, 641}) {
      std::vector<std::int32_t> routing(tokens);
      std::vector<float> samples;
      for (int trial = -3; trial < reps; ++trial) {
        for (int t = 0; t < tokens; ++t)
          routing[t] = ((trial + 3) * 31 + (shared ? t / 10 : t)) % experts;
        Check(hipMemcpy(ids.p, routing.data(), routing.size() * 4,
                        hipMemcpyHostToDevice));
        Check(hipEventRecord(begin));
        if (qfn_mmq_moe_vec(type, w.p, x.As<float>(), ids.As<std::int32_t>(),
                            out.As<float>(), m, k, tokens, experts, 1, nullptr))
          throw std::runtime_error("expert-vector launch failed");
        Check(hipEventRecord(end));
        Check(hipEventSynchronize(end));
        float ms;
        Check(hipEventElapsedTime(&ms, begin, end));
        if (trial >= 0)
          samples.push_back(ms);
      }
      std::vector<float> values(tokens * m);
      Check(hipMemcpy(values.data(), out.p, values.size() * 4,
                      hipMemcpyDeviceToHost));
      std::uint64_t hash = 1469598103934665603ULL;
      for (float v : values)
        if (!std::isfinite(v))
          throw std::runtime_error("nonfinite output");
      const auto* raw = reinterpret_cast<const std::uint8_t*>(values.data());
      for (std::size_t i = 0; i < values.size() * 4; ++i)
        hash = (hash ^ raw[i]) * 1099511628211ULL;
      std::sort(samples.begin(), samples.end());
      std::cout << "type=" << type << " shared=" << shared << " n=" << tokens
                << " median_ms=" << samples[reps / 2]
                << " min_ms=" << samples.front() << " max_ms=" << samples.back()
                << " hash=" << std::hex << hash << std::dec << '\n';
    }
  }
  Check(hipEventDestroy(begin));
  Check(hipEventDestroy(end));
} catch (const std::exception& e) {
  std::cerr << e.what() << '\n';
  return 1;
}
