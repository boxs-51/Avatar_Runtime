#pragma once
#include "security_clock.hpp"
#include "token_ledger.hpp"
#include "admission_gate.hpp"
#include "ingress_queue.hpp"
#include "../core/character_authority.hpp"

namespace ars::motion { class PublishGate; struct Snapshot; }
namespace ars::security {
class SourceAuthority {
    struct Source {
        std::mutex mutex;
        std::atomic<std::uint32_t> users{0};
        bool allocated=false;
        ars_source_admin_h id{};
        ars_character_h character{};
        SourceState state=SourceState::unused;
        std::uint64_t epoch=1,control=1,ticket=1,expiry=0,sequence=0,ordinal=0,refill=0,credit=0;
        std::uint32_t ledger=ledger_capacity,data=max_sources,channels=0,rate=0;
        std::int32_t priority=0;
        std::array<bool,max_channels> allowed{};
        std::array<char,257> label{};
        EpochTombstone tombstone;
    };
    struct QueueSlot { std::atomic<bool> used{false}; IngressQueue queue; };
    struct Workspace { std::atomic<bool> used{false}; PreparedFrame prepared; };
    core::HandleRegistry &registry_;
    const ars_runtime_desc descriptor_;
    const std::uint64_t uid_;
    CallbackState &callback_;
    std::mutex objects_;
    std::array<core::CharacterAuthority,max_characters> characters_{};
    std::array<Source,max_source_records> sources_{};
    std::array<QueueSlot,max_sources> queues_{};
    std::array<Workspace,max_workspaces> workspaces_{};
    std::uint32_t character_limit_=max_characters,record_limit_=max_source_records,source_limit_=max_sources;
    std::uint32_t workspace_limit_=max_workspaces;
    NativeClock capture_clock_{};
    ars_status capture_init_=ARS_OK;
    SecurityClock clock_;
    TokenLedger ledger_;
    AdmissionGate admission_;
    ars_status pin_character(ars_character_h,core::ObjectLease *);
    ars_status pin_source(ars_source_admin_h,core::ObjectLease *);
    ars_status prepare(const ars_motion_frame *,PreparedFrame *);
    void terminal(Source &,SourceState) noexcept;
    ars_status observe_source(Source &,std::uint64_t);
    ars_status state_error(const Source &) const noexcept;
    void abandon(Source &);
    friend struct ars::core::ComponentFixture;
    friend class ars::motion::PublishGate;
public:
    // Shared with publication in B3; ordinary lookup never holds router here.
    std::mutex commit;
    SourceAuthority(core::HandleRegistry &,const ars_runtime_desc &,std::uint64_t,CallbackState &);
    ars_status bind(std::span<const SemanticChannel>,ars_character_h *);
    ars_status destroy(ars_character_h,motion::Snapshot **);
    ars_status register_source(ars_character_h,const ars_source_desc *,ars_source_admin_h *,ars_ingress_cap *);
    ars_status rotate(ars_source_admin_h,ars_ingress_cap *);
    ars_status revoke(ars_source_admin_h);
    ars_status release(ars_source_admin_h);
    ars_status submit(ars_ingress_cap,const ars_motion_frame *);
    ars_status dequeue(ars_source_admin_h,OwnedFrame *,core::ObjectLease *,core::ObjectLease *);
    void request_close();
    void drain();
    void close_all(std::array<motion::Snapshot *,max_characters> *);
};
}
