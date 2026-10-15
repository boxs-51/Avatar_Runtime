#include "admission_gate.hpp"

namespace ars::security {
AdmissionGate::Lease::~Lease() { if(owner_) owner_->leave(); }
ars_status AdmissionGate::enter(Lease *lease) {
    if(!lease || lease->owner_) return ARS_E_INVALID_ARGUMENT;
    std::lock_guard lock(mutex_);
    if(closing_) return ARS_E_CLOSED;
    if(active_>=limit_) return ARS_E_BUSY;
    ++active_; lease->owner_=this; cv_.notify_all(); return ARS_OK;
}
void AdmissionGate::leave() noexcept { std::lock_guard lock(mutex_); --active_; cv_.notify_all(); }
void AdmissionGate::request_close() {
    std::lock_guard lock(mutex_); closing_=true;
    requested_.store(true,std::memory_order_release); cv_.notify_all();
}
void AdmissionGate::drain() { std::unique_lock lock(mutex_); cv_.wait(lock,[&]{return active_==0;}); }
bool AdmissionGate::closing() { return requested_.load(std::memory_order_acquire); }
std::uint32_t AdmissionGate::active() { std::lock_guard lock(mutex_); return active_; }
}
