#pragma once
#include "handle_registry.hpp"
#include "../security/authority_types.hpp"
#include "character_authority.hpp"
#include <array>
#include <cstdint>
#include <mutex>

namespace ars::security { class SourceAuthority; }
namespace ars::motion { class PublishGate; struct Stage; struct Snapshot; struct Program; }
namespace ars::core {
struct Runtime;
class RuntimeRouter;
enum class LeaseKind { work, management, ingress, snapshot };
class RuntimeLease {
    RuntimeRouter *owner_=nullptr;
    Runtime *runtime_=nullptr;
    LeaseKind kind_=LeaseKind::work;
    RegistryLease root_;
    friend class RuntimeRouter;
public:
    RuntimeLease()=default;
    RuntimeLease(const RuntimeLease &)=delete;
    RuntimeLease &operator=(const RuntimeLease &)=delete;
    RuntimeLease(RuntimeLease &&) noexcept;
    RuntimeLease &operator=(RuntimeLease &&) noexcept;
    ~RuntimeLease();
    void reset() noexcept;
    explicit operator bool() const noexcept { return runtime_!=nullptr; }
};
class FrameLease {
    RuntimeLease runtime_;
    ObjectLease character_,source_;
    friend class RuntimeRouter;
public:
    security::OwnedFrame frame{};
    FrameLease()=default;
    FrameLease(const FrameLease &)=delete;
    FrameLease &operator=(const FrameLease &)=delete;
    ~FrameLease() { reset(); }
    void reset() noexcept { source_.reset(); character_.reset(); runtime_.reset(); frame={}; }
};
class StageLease {
    RuntimeLease runtime_;
    motion::PublishGate *owner_=nullptr;
    motion::Stage *stage_=nullptr;
    friend class RuntimeRouter;
    friend struct ComponentFixture;
public:
    StageLease()=default;
    StageLease(const StageLease &)=delete;
    StageLease &operator=(const StageLease &)=delete;
    ~StageLease();
    void reset() noexcept;
};
class SnapshotLease {
    RuntimeLease runtime_;
    motion::Snapshot *snapshot_=nullptr;
    friend class RuntimeRouter;
public:
    SnapshotLease()=default;
    SnapshotLease(const SnapshotLease &)=delete;
    SnapshotLease &operator=(const SnapshotLease &)=delete;
    ~SnapshotLease();
    void reset() noexcept;
    const motion::Snapshot *get() const noexcept { return snapshot_; }
};
// Internal trusted component API. No fixture/reset/counter API is exported.
class RuntimeRouter {
    struct Entry { std::uint64_t uid=0; Runtime *runtime=nullptr; bool reserved=false; };
    RuntimeBudget budget_;
    std::uint64_t next_uid_;
    bool uid_exhausted_=false;
    mutable std::mutex mutex_;
    std::array<Entry,max_runtime_count> entries_{};
    ars_status resolve_locked(ars_runtime_h,Runtime **) const;
    void release(Runtime *,LeaseKind) noexcept;
    void cancel_reservation(std::uint32_t) noexcept;
    ars_status ensure_authority(RuntimeLease &,security::SourceAuthority **);
    security::SourceAuthority *authority_for(const RuntimeLease &) const;
    ars_status ensure_publication(RuntimeLease &,motion::PublishGate **);
    friend class RuntimeLease;
    friend struct ComponentFixture;
public:
    explicit RuntimeRouter(RuntimeBudget budget={});
    ars_status create(const ars_runtime_desc &,ars_runtime_h *);
    ars_status acquire(ars_runtime_h,LeaseKind,RuntimeLease *);
    ars_status begin_close(ars_runtime_h);
    ars_status query_close(ars_runtime_h,ars_close_state *);
    ars_status finish_close(ars_runtime_h);
    ars_status bind_character(ars_runtime_h,std::span<const security::SemanticChannel>,ars_character_h *);
    ars_status destroy_character(ars_runtime_h,ars_character_h);
    ars_status register_source(ars_runtime_h,ars_character_h,const ars_source_desc *,ars_source_admin_h *,ars_ingress_cap *);
    ars_status rotate_source(ars_runtime_h,ars_source_admin_h,ars_ingress_cap *);
    ars_status revoke_source(ars_runtime_h,ars_source_admin_h);
    ars_status release_source(ars_runtime_h,ars_source_admin_h);
    ars_status submit_frame(ars_runtime_h,ars_ingress_cap,const ars_motion_frame *);
    ars_status dequeue_frame(ars_runtime_h,ars_source_admin_h,FrameLease *);
    ars_status prepare_cpu(ars_runtime_h,ars_character_h,std::span<const ars_source_admin_h>,const motion::Program &,StageLease *);
    ars_status publish_cpu(StageLease *);
    // Private consumer read: caller serializes access to its StageLease.
    // Copies scalars without exposing mutable candidate storage; final publish
    // still performs the existing source/epoch/clock validation fence.
    ars_status copy_cpu_candidate(const StageLease &,std::span<float>,std::uint64_t *) const;
    ars_status read_cpu(ars_runtime_h,ars_character_h,SnapshotLease *);
    std::uint32_t occupied() const;
};
RuntimeRouter &global_router();
}
