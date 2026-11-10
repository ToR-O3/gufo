#pragma once

#include <cstddef>

namespace gufo::hip {

struct QwenGpuMemoryUsage {
  std::size_t request_state_bytes{0};
  std::size_t temporary_scratch_bytes{0};

  [[nodiscard]] std::size_t TotalBytes() const noexcept {
    return request_state_bytes + temporary_scratch_bytes;
  }
};

}  // namespace gufo::hip
