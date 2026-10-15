#pragma once
#include "cpu_snapshot.hpp"
#include "semantic_candidate.hpp"
namespace ars::security { class SourceAuthority; }
namespace ars::motion {
struct Stage {
    std::atomic<bool> used{false};
    Snapshot *reserved=nullptr,*base=nullptr;
    std::uint64_t expected_generation=0;
    SemanticCandidate candidate;
    std::array<std::uint32_t,max_dependencies> lock_order{};
};
class PublishGate {
    security::SourceAuthority &authority_;
    std::array<Snapshot,max_snapshots> snapshots_;
    std::array<Stage,max_candidates> stages_;
    std::uint32_t snapshot_limit_=max_snapshots,stage_limit_=max_candidates;
    friend struct ars::core::ComponentFixture;
    Snapshot *reserve();
    ars_status sanitize(StagedState &);
public:
    explicit PublishGate(security::SourceAuthority &);
    ars_status prepare(ars_character_h,std::span<const ars_source_admin_h>,const Program &,Stage **);
    ars_status publish(Stage &);
    ars_status read(ars_character_h,Snapshot **);
    void discard(Stage *) noexcept;
};
}
