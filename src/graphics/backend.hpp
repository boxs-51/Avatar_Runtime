#pragma once
#include <cstdint>

namespace ars::graphics {
enum class Result { ok, invalid_argument, unsupported, wrong_thread, busy, device_lost, failed };
struct TargetInfo {
    std::uint64_t generation = 0;
    std::uint32_t width = 0, height = 0;
};
}
