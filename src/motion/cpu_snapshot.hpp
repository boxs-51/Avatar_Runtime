#pragma once
#include "staged_state.hpp"
#include "../core/character_authority.hpp"
namespace ars::motion {
struct Snapshot {
    std::atomic<bool> used{false};
    std::atomic<std::uint32_t> references{0};
    std::uint64_t generation=0;
    StagedState state;
    std::array<Value,max_cells> outputs{};
    std::uint32_t channel_count=0;
    core::ObjectLease character;
    std::array<core::ObjectLease,max_dependencies> sources;
    static void retain(Snapshot *) noexcept;
    static void release(Snapshot *) noexcept;
};
}
