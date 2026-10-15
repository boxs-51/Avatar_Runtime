#pragma once
#include <cstdint>

namespace ars::core {
inline constexpr std::uint32_t max_runtime_count=64;
inline constexpr std::uint32_t max_slot_count=1024;
inline constexpr std::uint32_t max_pin_count=1024;
inline constexpr std::uint64_t max_authority_bytes=8*1024*1024;
// Internal construction policy. The public ABI exposes no override/reset hook.
struct RuntimeBudget {
    std::uint32_t runtimes=max_runtime_count;
    std::uint32_t slots=max_slot_count;
    std::uint32_t pins=max_pin_count;
    std::uint64_t authority_bytes=max_authority_bytes;
};
}
