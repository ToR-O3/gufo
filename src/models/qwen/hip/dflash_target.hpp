#pragma once

#include "src/families/qwen/dflash/dflash.hpp"
#include "src/models/qwen/hip/executor.hpp"

#if defined(ENGINE_ENABLE_HIP)
namespace gufo::hip {

[[nodiscard]] inline DFlashTarget MakeDFlashTarget(
    const std::shared_ptr<const QwenGpuModel>& model) {
  if (!model)
    return {};
  const auto& config = model->GetConfig();
  const auto& weights = model->GetWeights();
  return {std::shared_ptr<const tokenization::QwenTokenizer>(
              model, &model->GetTokenizer()),
          weights.token_embd,
          weights.output,
          config.hidden_size,
          config.vocab_size,
          config.num_layers};
}

}  // namespace gufo::hip
#endif
