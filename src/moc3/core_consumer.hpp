#pragma once
#include "independent_snapshot.hpp"
#include "../core/runtime_lifecycle.hpp"
#include <thread>

namespace ars::moc3 {
// Private owning consumer of the actual core authority/publication components.
class CoreConsumer {
    core::RuntimeRouter router_;
    ars_runtime_h runtime_{};
    ars_character_h character_{};
    ars_source_admin_h source_{};
    ars_ingress_cap token_{};
    core::StageLease candidate_;
    std::vector<ParameterInfo> mapping_;
    std::thread::id owner_=std::this_thread::get_id();
    bool opened_=false,pending_=false,closed_=false,faulted_=false,recoverable_=false;
public:
    CoreConsumer()=default;
    ~CoreConsumer();
    ars_status open(std::span<const ParameterInfo>,bool recoverable=false);
    ars_status prepare(std::uint64_t sequence,std::span<const Parameter>,std::vector<Parameter>&,
                       std::uint64_t &generation);
    ars_status publish();
    ars_status revoke();
    void discard() noexcept;
    ars_status read(std::vector<Parameter>&,std::uint64_t &generation);
    ars_status close();
};
}
