#include "handle_registry.hpp"
#include <limits>
#include <utility>

namespace ars::core {
RegistryLease::RegistryLease(HandleRegistry *owner,std::uint32_t slot,void *payload) noexcept
    :owner_(owner),slot_(slot),payload_(payload) {}
RegistryLease::RegistryLease(RegistryLease &&other) noexcept { *this=std::move(other); }
RegistryLease &RegistryLease::operator=(RegistryLease &&other) noexcept {
    if(this!=&other) {
        reset(); owner_=std::exchange(other.owner_,nullptr); slot_=other.slot_;
        payload_=std::exchange(other.payload_,nullptr);
    }
    return *this;
}
RegistryLease::~RegistryLease() { reset(); }
void RegistryLease::reset() noexcept {
    auto *owner=std::exchange(owner_,nullptr); payload_=nullptr;
    if(owner) owner->unpin(slot_);
}
HandleRegistry::HandleRegistry(std::uint64_t uid,std::uint32_t capacity,
                               std::uint32_t pin_limit)
    :uid_(uid),capacity_(capacity<=max_slot_count?capacity:0),
     pin_limit_(pin_limit<=max_pin_count?pin_limit:0) {}
ars_status HandleRegistry::check_locked(ars_handle_id id,ObjectType type) const noexcept {
    if(id.runtime_uid==0) return ARS_E_INVALID_ARGUMENT;
    if(id.runtime_uid!=uid_ || id.slot>=capacity_) return ARS_E_STALE_HANDLE;
    const auto &slot=slots_[id.slot];
    return slot.active && slot.generation==id.generation && slot.type==type
        ? ARS_OK:ARS_E_STALE_HANDLE;
}
ars_status HandleRegistry::insert(ObjectType type,void *payload,ars_handle_id *out) {
    if(out) *out={};
    if(!out || !payload || !uid_ || type<ObjectType::runtime || type>ObjectType::source_admin)
        return ARS_E_INVALID_ARGUMENT;
    std::lock_guard lock(mutex_);
    bool all_retired=capacity_!=0;
    for(std::uint32_t i=0;i<capacity_;++i) {
        auto &slot=slots_[i];
        all_retired=all_retired && slot.retired;
        if(!slot.active && !slot.retired && slot.pins==0) {
            slot.type=type; slot.payload=payload; slot.active=true;
            *out={uid_,i,slot.generation}; return ARS_OK;
        }
    }
    return all_retired?ARS_E_HANDLE_EXHAUSTED:ARS_E_OUT_OF_BUDGET;
}
ars_status HandleRegistry::check(ars_handle_id id,ObjectType type) const {
    std::lock_guard lock(mutex_); return check_locked(id,type);
}
ars_status HandleRegistry::pin(ars_handle_id id,ObjectType type,RegistryLease *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    out->reset();
    std::lock_guard lock(mutex_);
    auto status=check_locked(id,type); if(status!=ARS_OK) return status;
    if(pins_>=pin_limit_) return ARS_E_BUSY;
    auto &slot=slots_[id.slot]; ++slot.pins; ++pins_;
    *out=RegistryLease(this,id.slot,slot.payload); return ARS_OK;
}
ars_status HandleRegistry::invalidate(ars_handle_id id,ObjectType type) {
    std::lock_guard lock(mutex_);
    auto status=check_locked(id,type); if(status!=ARS_OK) return status;
    auto &slot=slots_[id.slot]; slot.active=false;
    if(slot.generation==std::numeric_limits<std::uint32_t>::max()) slot.retired=true;
    else ++slot.generation;
    if(slot.pins==0) slot.payload=nullptr;
    return slot.retired?ARS_E_HANDLE_EXHAUSTED:ARS_OK;
}
void HandleRegistry::unpin(std::uint32_t index) noexcept {
    std::lock_guard lock(mutex_);
    auto &slot=slots_[index]; --slot.pins; --pins_;
    if(!slot.active && slot.pins==0) slot.payload=nullptr;
}
std::uint32_t HandleRegistry::active_slots() const {
    std::lock_guard lock(mutex_); std::uint32_t count=0;
    for(std::uint32_t i=0;i<capacity_;++i) if(slots_[i].active) ++count;
    return count;
}
std::uint32_t HandleRegistry::outstanding_pins() const { std::lock_guard lock(mutex_); return pins_; }
}
