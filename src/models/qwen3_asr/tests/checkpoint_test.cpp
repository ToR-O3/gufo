#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "src/models/qwen3_asr/loader.hpp"

namespace qwen3_asr = gufo::models::qwen3_asr;
namespace fs = std::filesystem;

namespace {

void Check(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << "FAIL qwen3_asr_checkpoint_test: " << message << '\n';
    std::exit(1);
  }
}

void WriteWeights(const fs::path& path, std::string_view name) {
  const std::string header =
      "{\"" + std::string(name) +
      "\":{\"dtype\":\"BF16\",\"shape\":[2],\"data_offsets\":[0,4]}}";
  std::ofstream output(path, std::ios::binary);
  const std::uint64_t size = header.size();
  for (unsigned index = 0; index < 8; ++index)
    output.put(static_cast<char>((size >> (index * 8U)) & 0xffU));
  output << header;
  output.write("\0\0\0\0", 4);
  Check(output.good(), "fixture must be written");
}

void WriteConfig(const fs::path& root, bool tied) {
  std::ofstream output(root / "config.json");
  output << R"({"model_type":"qwen3_asr",
    "architectures":["Qwen3ASRForConditionalGeneration"],
    "thinker_config":{"dtype":"bfloat16","audio_config":{},
    "text_config":{"tie_word_embeddings":)"
         << (tied ? "true" : "false") << "}}}";
}

}  // namespace

int main() {
  const fs::path root =
      fs::temp_directory_path() /
      ("gufo-asr-checkpoint-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  Check(fs::create_directory(root),
        "temporary model directory must be created");
  WriteWeights(root / "model.safetensors", "thinker.model.embed_tokens.weight");
  WriteConfig(root, true);
  {
    auto loaded = qwen3_asr::LoadModelDirectory(root.string());
    Check(loaded.ok, loaded.error);
    const auto* embedding =
        loaded.store->Find("thinker.model.embed_tokens.weight");
    const auto* output = loaded.store->Find("thinker.lm_head.weight");
    Check(loaded.mapped_regions.size() == 1 && embedding && output,
          "single-file tied checkpoint must load both tensor names");
    Check(
        embedding->data == output->data && embedding->shape == output->shape &&
            embedding->dtype == output->dtype,
        "tied output must alias the original storage and retain its metadata");
    const auto regions = loaded.RegionsFor({"thinker.lm_head."});
    Check(regions.size() == 1,
          "output alias must remain visible to weight uploads");
  }
  WriteConfig(root, false);
  {
    auto loaded = qwen3_asr::LoadModelDirectory(root.string());
    Check(loaded.ok && !loaded.store->Find("thinker.lm_head.weight"),
          "untied checkpoints must not silently substitute output weights");
  }
  fs::remove(root / "model.safetensors");
  WriteConfig(root, true);
  WriteWeights(root / "model-00001-of-00002.safetensors",
               "thinker.model.embed_tokens.weight");
  WriteWeights(root / "model-00002-of-00002.safetensors",
               "thinker.lm_head.weight");
  {
    auto loaded = qwen3_asr::LoadModelDirectory(root.string());
    Check(loaded.ok && loaded.mapped_regions.size() == 2,
          "original two-shard checkpoints must still load");
    Check(loaded.store->Find("thinker.model.embed_tokens.weight")->data !=
              loaded.store->Find("thinker.lm_head.weight")->data,
          "explicit output weights must take precedence over tying metadata");
  }
  fs::remove(root / "model-00002-of-00002.safetensors");
  Check(!qwen3_asr::LoadModelDirectory(root.string()).ok,
        "an incomplete sharded checkpoint must fail");
  WriteWeights(root / "model.safetensors", "thinker.audio_tower.test.weight");
  Check(!qwen3_asr::LoadModelDirectory(root.string()).ok,
        "missing embeddings must fail when the output head is tied");
  fs::remove_all(root);
  std::cout << "PASS qwen3_asr_checkpoint_test\n";
  return 0;
}
