#pragma once
#include "ars/ars.h"
#include <array>
#include <cstdint>
#include <span>
#include <atomic>
#include <cstring>
#include <mutex>

namespace ars::core { struct ComponentFixture; }
namespace ars::security {
struct CallbackState {
private:
    static constexpr std::uint32_t callback_bit=1U<<31;
    std::atomic<std::uint32_t> admission_{0};
public:
    std::mutex samples;
    std::uint64_t highwater=0;
    bool observed=false;
    bool active() const noexcept { return (admission_.load(std::memory_order_acquire)&callback_bit)!=0; }
    bool enter_callback() noexcept {
        std::uint32_t expected=0;
        return admission_.compare_exchange_strong(expected,callback_bit,std::memory_order_acq_rel);
    }
    void leave_callback() noexcept { admission_.store(0,std::memory_order_release); }
    bool enter_mutation() noexcept {
        auto value=admission_.load(std::memory_order_acquire);
        for(;;) {
            if((value&callback_bit) || value==callback_bit-1) return false;
            if(admission_.compare_exchange_weak(value,value+1,std::memory_order_acq_rel)) return true;
        }
    }
    void leave_mutation() noexcept { admission_.fetch_sub(1,std::memory_order_acq_rel); }
};
class MutationLease {
    CallbackState *owner_;
public:
    explicit MutationLease(CallbackState &state) noexcept:owner_(state.enter_mutation()?&state:nullptr) {}
    MutationLease(const MutationLease &)=delete;
    MutationLease &operator=(const MutationLease &)=delete;
    ~MutationLease() { if(owner_) owner_->leave_mutation(); }
    explicit operator bool() const noexcept { return owner_!=nullptr; }
};
class CallbackLease {
    CallbackState *owner_;
public:
    explicit CallbackLease(CallbackState &state) noexcept:owner_(state.enter_callback()?&state:nullptr) {}
    CallbackLease(const CallbackLease &)=delete;
    CallbackLease &operator=(const CallbackLease &)=delete;
    ~CallbackLease() { if(owner_) owner_->leave_callback(); }
    explicit operator bool() const noexcept { return owner_!=nullptr; }
};
inline constexpr std::uint32_t max_sources=16,max_source_records=64,max_characters=16;
inline constexpr std::uint32_t max_channels=256,queue_capacity=16,max_workspaces=16;
inline constexpr std::uint32_t max_admissions=8,ledger_capacity=4096,mint_attempts=8;
inline constexpr std::uint64_t max_ttl=3600000000000ULL,credit_unit=1000000000ULL;
inline constexpr std::uint32_t max_input_bytes=49152,max_owned_bytes=8192;
struct SemanticChannel {
    std::array<char,129> id{};
    std::uint32_t kind=ARS_VALUE_SCALAR;
    std::array<float,4> neutral{};
};
struct OwnedChannel { std::uint32_t index=0,kind=0; std::array<float,4> value{}; };
struct OwnedFrame {
    ars_character_h character{}; ars_source_admin_h source{};
    std::uint64_t epoch=0,sequence=0,ordinal=0,timestamp=0;
    std::uint32_t count=0; float confidence=0;
    std::array<OwnedChannel,max_channels> channels{};
};
static_assert(sizeof(OwnedFrame)<=max_owned_bytes);
struct PreparedFrame {
    OwnedFrame frame;
    std::array<std::array<char,129>,max_channels> names{};
};
inline bool same(ars_handle_id a,ars_handle_id b) noexcept {
    return a.runtime_uid==b.runtime_uid && a.slot==b.slot && a.generation==b.generation;
}
inline bool same(ars_ingress_cap a,ars_ingress_cap b) noexcept { return ((a.lo^b.lo)|(a.hi^b.hi))==0; }
inline bool nonzero(ars_ingress_cap cap) noexcept { return (cap.lo|cap.hi)!=0; }
bool canonical_id(ars_utf8_view) noexcept;
bool valid_utf8(ars_utf8_view,std::uint32_t limit) noexcept;
bool valid_value(std::uint32_t,std::span<const float,4>) noexcept;
enum class SourceState { unused,active,rotating,rotate_failed,explicit_revoked,expired,exhausted,closed };
struct EpochTombstone {
    std::atomic<std::uint64_t> invalidated_through{0};
    std::atomic<bool> dirty{false};
    void publish(std::uint64_t epoch) noexcept {
        invalidated_through.store(epoch,std::memory_order_release);
        dirty.store(true,std::memory_order_release);
    }
};
}
