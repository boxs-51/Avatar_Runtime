#include "token_ledger.hpp"

namespace ars::security {
ars_status TokenLedger::reserve(ars_source_admin_h binding,Reservation *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    *out={};
    for(std::uint32_t attempt=0;attempt<mint_attempts;++attempt) {
        {
            std::lock_guard lock(issue_mutex_);
            if(used_>=limit_) return ARS_E_HANDLE_EXHAUSTED;
        }
        ars_ingress_cap token{};
        auto bytes=std::as_writable_bytes(std::span{&token,1});
        const auto status=entropy_(entropy_user_,bytes); // No ARS lock held.
        if(status!=ARS_OK) { wipe(bytes); return ARS_E_INTERNAL; }
        if(!nonzero(token)) { wipe(bytes); continue; }
        std::lock_guard lock(issue_mutex_);
        bool collision=false;
        for(std::uint32_t i=0;i<used_;++i) collision=bool(collision | same(entries_[i].token,token));
        if(collision) { wipe(bytes); continue; }
        if(used_>=limit_) { wipe(bytes); return ARS_E_HANDLE_EXHAUSTED; }
        const auto index=used_++; auto &entry=entries_[index];
        entry.token=token; entry.binding=binding;
        entry.state.store(State::reserved,std::memory_order_release);
        *out={token,index}; wipe(bytes); return ARS_OK;
    }
    return ARS_E_HANDLE_EXHAUSTED;
}
bool TokenLedger::activate(const Reservation &reservation) noexcept {
    if(reservation.index>=ledger_capacity) return false;
    auto &entry=entries_[reservation.index];
    if(!same(entry.token,reservation.token)) return false;
    auto expected=State::reserved;
    return entry.state.compare_exchange_strong(expected,State::issued,std::memory_order_acq_rel);
}
void TokenLedger::retire(std::uint32_t index) noexcept {
    if(index<ledger_capacity) entries_[index].state.store(State::retired,std::memory_order_release);
}
bool TokenLedger::lookup(ars_ingress_cap token,ars_source_admin_h *out) const noexcept {
    if(!out) return false;
    *out={}; bool found=false;
    if(!nonzero(token)) return false;
    for(const auto &entry:entries_) {
        if(entry.state.load(std::memory_order_acquire)==State::issued && same(entry.token,token)) {
            *out=entry.binding; found=true;
        }
    }
    return found;
}
}
