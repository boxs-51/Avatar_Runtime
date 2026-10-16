#pragma once
#include "provenance.hpp"
namespace ars::motion {
struct Trigger { Value value{}; std::uint64_t due=0; };
struct StagedState {
    Provenance provenance;
    std::array<Value,max_cells> cells{};
    std::array<Trigger,max_triggers> triggers{};
    std::uint32_t trigger_count=0;
    void purge(std::uint16_t invalid) noexcept;
    void compact() noexcept;
};
}
