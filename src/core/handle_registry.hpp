#pragma once
#include "ars/ars.h"
#include "runtime_budget.hpp"
#include <array>
#include <cstdint>
#include <mutex>

namespace ars::core {
enum class ObjectType:std::uint8_t { runtime=1, model=2, character=3, source_admin=4 };
class HandleRegistry;
// Owner must outlive leases and retain payload storage until its leases drain.
// Logical invalidation never makes a lease unpin via a public handle lookup.
class RegistryLease {
    HandleRegistry *owner_=nullptr;
    std::uint32_t slot_=0;
    void *payload_=nullptr;
    RegistryLease(HandleRegistry *,std::uint32_t,void *) noexcept;
    friend class HandleRegistry;
public:
    RegistryLease()=default;
    RegistryLease(const RegistryLease &)=delete;
    RegistryLease &operator=(const RegistryLease &)=delete;
    RegistryLease(RegistryLease &&) noexcept;
    RegistryLease &operator=(RegistryLease &&) noexcept;
    ~RegistryLease();
    void reset() noexcept;
    void *payload() const noexcept { return payload_; }
    explicit operator bool() const noexcept { return owner_!=nullptr; }
};
class HandleRegistry {
    struct Slot {
        std::uint32_t generation=1,pins=0;
        ObjectType type=ObjectType::runtime;
        bool active=false,retired=false;
        void *payload=nullptr;
    };
    std::uint64_t uid_;
    std::uint32_t capacity_,pin_limit_,pins_=0;
    mutable std::mutex mutex_;
    std::array<Slot,max_slot_count> slots_{};
    ars_status check_locked(ars_handle_id,ObjectType) const noexcept;
    void unpin(std::uint32_t) noexcept;
    friend class RegistryLease;
public:
    explicit HandleRegistry(std::uint64_t uid,std::uint32_t capacity=max_slot_count,
                            std::uint32_t pin_limit=max_pin_count,std::uint32_t initial_generation=1);
    ars_status insert(ObjectType,void *payload,ars_handle_id *);
    ars_status check(ars_handle_id,ObjectType) const;
    ars_status pin(ars_handle_id,ObjectType,RegistryLease *);
    ars_status invalidate(ars_handle_id,ObjectType);
    std::uint32_t active_slots() const;
    std::uint32_t outstanding_pins() const;
};
}
