#pragma once
#include "cpu_snapshot.hpp"
#include "semantic_candidate.hpp"
#include "../security/ingress_queue.hpp"
namespace ars::security { class SourceAuthority; }
namespace ars::motion {
struct Stage {
    std::atomic<bool> used{false};
    Snapshot *reserved=nullptr,*base=nullptr;
    std::uint64_t expected_generation=0;
    SemanticCandidate candidate;
    std::array<core::ObjectLease,max_dependencies> inputs;
    security::IngressQueue queue_view;
    struct LockTarget { void *source=nullptr; std::uint32_t slot=0; };
    std::array<LockTarget,max_dependencies*2> lock_order{};
    std::uint32_t lock_count=0;
    void (*commit_extension)(void *) noexcept=nullptr;
    void *commit_user=nullptr;
};
class PublishGate;
// One public update transaction, charged to the runtime CPU context. Reuses
// the existing single-character prepare scratch sequentially; never increases
// the eight private stage slots or 64 immutable snapshot slots.
struct PublicBatch {
    struct Entry { Snapshot *snapshot=nullptr;std::uint64_t generation=0;
        void (*commit)(void *) noexcept=nullptr;void *user=nullptr; };
    struct Input { core::ObjectLease pin;security::OwnedFrame frame; };
    std::array<Entry,security::max_characters> entries{};
    std::array<Input,security::max_sources> inputs{};
    std::uint32_t count=0,input_count=0;
    PublishGate *owner=nullptr;
    PublicBatch()=default;
    PublicBatch(const PublicBatch &)=delete;
    ~PublicBatch(){reset();}
    void reset() noexcept;
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
    ars_status append(Stage &,PublicBatch &);
    ars_status publish(PublicBatch &,void (*)(void *) noexcept,void *);
    ars_status read(ars_character_h,Snapshot **);
    void discard(Stage *) noexcept;
};
}
