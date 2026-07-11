#pragma once
#include <cstdint>
#include <string>

namespace nrvnaai {

// Core job lifecycle states.
enum class Status : std::uint8_t { Queued, Running, Done, Failed, Missing };

// Job modality, serialized to/from type.txt (see nrvna/contract.hpp).
enum class JobType : std::uint8_t {
    Text = 0,
    Embed = 1,
    Vision = 2,
    Tts = 3,
    Stt = 4
};

// Opaque job identifier (string-based for now; can evolve to strong type).
using JobId = std::string;

} // namespace nrvnaai
