#pragma once

#include <algorithm>

namespace nrvna::detail {

inline int threadsPerWorker(int totalThreads, int workers) noexcept {
    return std::max(1, totalThreads / std::max(1, workers));
}

} // namespace nrvna::detail
