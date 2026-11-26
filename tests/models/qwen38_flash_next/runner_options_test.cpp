#include <cassert>
#include <cstddef>
#include <limits>
#include <string>

#include "src/models/qwen38_flash_next/engine.hpp"
#include "src/models/qwen38_flash_next/runner.hpp"
#include "src/models/qwen38_flash_next/stages.hpp"
#include "src/models/qwen38_flash_next/weight_source.hpp"

namespace qfn = gufo::models::qwen38_flash_next;

static_assert(qfn::DecodeConcurrencyForSessions(0) == 1);
static_assert(qfn::DecodeConcurrencyForSessions(1) == 1);
static_assert(qfn::DecodeConcurrencyForSessions(2) == 2);
static_assert(qfn::DecodeConcurrencyForSessions(4) == 4);
static_assert(qfn::DecodeConcurrencyForSessions(8) == 8);
static_assert(qfn::DecodeConcurrencyForSessions(9) == 8);
static_assert(qfn::DecodeConcurrencyForSessions(
                  std::numeric_limits<std::size_t>::max()) == 8);

int main() {
#ifdef ENGINE_ENABLE_HIP
  // The DFlash backend consumes plain GGUF tensor pointers. An external
  // source may instead expose encoded tensors and requires its own bridge.
  qfn::ModelOptions options;
  options.dflash_model_path = "unused-draft.gguf";
  std::string error;
  auto model = qfn::Model::Load(
      "unused-model.gguf", options, nullptr,
      gufo::ext::qwen::flash_next::CreateGgufWeightSource(), &error);
  assert(!model);
  assert(error == "DFlash2 is not supported with an external weight source");
#endif
}
