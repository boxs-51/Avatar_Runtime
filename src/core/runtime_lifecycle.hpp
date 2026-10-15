#pragma once
#include "handle_registry.hpp"
#include <array>
#include <cstdint>
#include <mutex>

namespace ars::core {
struct Runtime;
class RuntimeRouter;
enum class LeaseKind { work, management };
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
    friend class RuntimeLease;
public:
    explicit RuntimeRouter(RuntimeBudget budget={},std::uint64_t initial_uid=1);
    ars_status create(const ars_runtime_desc &,ars_runtime_h *);
    ars_status acquire(ars_runtime_h,LeaseKind,RuntimeLease *);
    ars_status begin_close(ars_runtime_h);
    ars_status query_close(ars_runtime_h,ars_close_state *);
    ars_status finish_close(ars_runtime_h);
    std::uint32_t occupied() const;
};
RuntimeRouter &global_router();
}
