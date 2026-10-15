#pragma once
#include "os_entropy.hpp"
#include <mutex>

namespace ars::security {
class TokenLedger {
    enum class State:std::uint8_t { empty,reserved,issued,retired };
    struct Entry {
        ars_ingress_cap token{};
        ars_source_admin_h binding{};
        std::atomic<State> state{State::empty};
    };
    std::array<Entry,ledger_capacity> entries_{};
    std::uint32_t used_=0,limit_=ledger_capacity;
    EntropyRead entropy_=native_entropy;
    void *entropy_user_=nullptr;
    std::mutex issue_mutex_;
    friend struct ars::core::ComponentFixture;
public:
    struct Reservation { ars_ingress_cap token{}; std::uint32_t index=ledger_capacity; };
    ars_status reserve(ars_source_admin_h,Reservation *);
    bool activate(const Reservation &) noexcept;
    void retire(std::uint32_t) noexcept;
    bool lookup(ars_ingress_cap,ars_source_admin_h *) const noexcept;
};
}
