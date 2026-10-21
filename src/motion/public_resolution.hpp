#pragma once
#include "semantic_candidate.hpp"
namespace ars::motion {
struct RetainedInput {
    security::OwnedFrame frame;
    std::array<float,max_cells> confidence{};
};
struct PublicResolution {
    struct Source {
        ars_source_admin_h id{};
        std::uint64_t epoch=0;
        std::int32_t priority=0;
        RetainedInput *cache=nullptr;
    };
    struct Constraint { float minimum=0,maximum=0; bool has_minimum=false,has_maximum=false; };
    std::array<Source,max_dependencies> sources{};
    std::array<RetainedInput,max_dependencies> next{};
    std::array<Constraint,max_cells> constraints{};
    std::uint32_t count=0;
    ars_status evaluate(SemanticCandidate &,std::span<const security::SemanticChannel>);
    // Bounded trivial assignments only; called inside publication's no-fail
    // commit region, with no allocation, host callback, lock or destruction.
    static void commit(void *) noexcept;
};
} // namespace ars::motion
