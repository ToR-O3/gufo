#include <iostream>
#include <map>
#include <stdexcept>

#include "src/models/qwen38_flash_next/weights.hpp"

namespace q = gufo::models::qwen38_flash_next;
using gufo::core::GgmlType;
int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: iq4xs_bind_probe MODEL.gguf MTP.gguf\n";
    return 2;
  }
  try {
    std::string error;
    auto reader = gufo::core::GgufReader::OpenFile(argv[1], &error);
    if (!reader)
      throw std::runtime_error(error);
    auto model = q::ModelWeights::Bind(*reader, &error);
    if (!model)
      throw std::runtime_error(error);
    std::map<GgmlType, std::size_t> types;
    std::uint64_t bytes = 0;
    for (const auto& t : reader->GetTensors()) {
      ++types[t.type];
      q::TensorRef ref{.data = t.data,
                       .type = t.type,
                       .cols = t.dimensions[0],
                       .rows = t.dimensions.size() > 1 ? t.dimensions[1] : 1,
                       .experts = t.dimensions.size() > 2 ? t.dimensions[2] : 1,
                       .name = t.name};
      bytes += ref.SizeBytes();
    }
    const std::map<GgmlType, std::size_t> expected{
        {GgmlType::kIQ4_XS, 568}, {GgmlType::kIQ4_NL, 194},
        {GgmlType::kQ5_K, 48},    {GgmlType::kQ6_K, 1},
        {GgmlType::kF32, 388},    {GgmlType::kBF16, 24},
        {GgmlType::kF16, 1}};
    if (reader->GetTensorCount() != 1224 || bytes != 97462131200ULL ||
        types != expected)
      throw std::runtime_error(
          "artifact differs from inspected OrcaRouter IQ4_XS layout");
    const auto native = [](const q::TensorRef& t, GgmlType type) {
      if (t.type != type)
        throw std::runtime_error("packed format changed");
    };
    native(model->token_embd, GgmlType::kIQ4_XS);
    native(model->output, GgmlType::kQ6_K);
    for (const auto& l : model->layers) {
      native(l.hc_attn.down, GgmlType::kIQ4_XS);
      native(l.hc_attn.up, GgmlType::kIQ4_NL);
      native(l.hc_attn.inject, GgmlType::kIQ4_XS);
      native(l.ffn_gate_exps, GgmlType::kIQ4_XS);
      native(l.ffn_up_exps, GgmlType::kIQ4_XS);
      native(l.ffn_down_exps, GgmlType::kIQ4_NL);
      if (l.linear) {
        native(l.ssm_alpha, GgmlType::kIQ4_XS);
        native(l.ssm_beta, GgmlType::kIQ4_XS);
      }
    }
    auto draft = gufo::core::GgufReader::OpenFile(argv[2], &error);
    if (!draft)
      throw std::runtime_error(error);
    auto mtp = q::MtpWeights::Bind(*draft, model->config, &error);
    if (!mtp)
      throw std::runtime_error(error);
    std::cout << "PASS native IQ4_XS: 1224 tensors, " << bytes
              << " packed bytes; Q8 MTP matched; no additional Q8 conversion\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
