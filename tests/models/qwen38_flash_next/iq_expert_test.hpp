#ifndef GUFO_TESTS_QWEN38_FLASH_NEXT_IQ_EXPERT_TEST_HPP_
#define GUFO_TESTS_QWEN38_FLASH_NEXT_IQ_EXPERT_TEST_HPP_

#include "src/core/gguf_reader.hpp"
#include "src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h"

namespace gufo::test {
inline int NativeExpertProjection(int type, const void* weights,
                                  const float* input, const int32_t* ids,
                                  float* output, int rows, int cols, int tokens,
                                  int experts, int used, hipStream_t stream) {
  using Projection = decltype(&qfn_mmq_iq3_s_moe_raw);
  Projection project = nullptr;
  switch (static_cast<core::GgmlType>(type)) {
    case core::GgmlType::kQ5_0:
      project = qfn_mmq_q5_0_moe_raw;
      break;
    case core::GgmlType::kIQ2_S:
      project = qfn_mmq_iq2_s_moe_raw;
      break;
    case core::GgmlType::kIQ3_XXS:
      project = qfn_mmq_iq3_xxs_moe_raw;
      break;
    case core::GgmlType::kIQ3_S:
      project = qfn_mmq_iq3_s_moe_raw;
      break;
    case core::GgmlType::kIQ4_XS:
      project = qfn_mmq_iq4_xs_moe_raw;
      break;
    case core::GgmlType::kIQ4_NL:
      project = qfn_mmq_iq4_nl_moe_raw;
      break;
    case core::GgmlType::kQ2_0:
      project = qfn_mmq_q2_0_moe_raw;
      break;
    default:
      return -1;
  }
  return project(weights, input, ids, output, rows, cols, tokens, experts, used,
                 stream);
}
}  // namespace gufo::test
#endif
