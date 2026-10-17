#include "close_coordinator.hpp"
#ifdef _WIN32
#include <new>
#include <stdexcept>
namespace ars::graphics::d3d11 {
CloseCoordinator::CloseCoordinator(ID3D11Device *device, ID3D11DeviceContext *context,
                                   std::uint64_t generation, QueryOps *ops)
    : backend_(device,context,generation), ops_(ops) {}

Result CloseCoordinator::submit(ID3D11RenderTargetView *target, TargetInfo info,
                                std::shared_ptr<const void> snapshot_pin) {
    if (!backend_.on_owner_thread()) return Result::wrong_thread;
    if (phase_==Phase::lost_quarantined || phase_==Phase::detached_lost) return Result::device_lost;
    if (close_requested() || phase_!=Phase::active) return Result::busy;
    const auto valid=backend_.validate(target,info);
    if (valid==Result::device_lost) phase_=Phase::lost_quarantined;
    if (valid!=Result::ok) return valid;
    std::unique_ptr<QueryRetirement> retirement;
    try {
        retirement=std::make_unique<QueryRetirement>(ops_);
        // Reserve before any draw: retaining an accepted submission must not allocate.
        pending_.reserve(pending_.size()+1);
    } catch (const std::bad_alloc &) {
        return Result::failed;
    } catch (const std::length_error &) {
        return Result::failed;
    }
    const auto ready=retirement->prepare(backend_.device(),backend_.context(),target,
                                         backend_.generation(),std::move(snapshot_pin));
    if (ready!=Result::ok) {
        if (ready==Result::device_lost) phase_=Phase::lost_quarantined;
        return ready;
    }
    const auto draw=backend_.submit(target,info);
    if (draw==Result::device_lost) {
        retirement->mark_lost();
        pending_.push_back(std::move(retirement));
        phase_=Phase::lost_quarantined;
        return draw;
    }
    if (draw!=Result::ok) {
        retirement->cancel_unsubmitted();
        return draw;
    }
    const auto ended=retirement->end();
    pending_.push_back(std::move(retirement));
    return ended;
}
Result CloseCoordinator::begin_close() noexcept {
    close_requested_.store(true,std::memory_order_release);
    return Result::ok;
}
Result CloseCoordinator::poll_close(std::uint64_t generation) {
    if (!backend_.on_owner_thread()) return Result::wrong_thread;
    if (!generation || generation!=backend_.generation()) return Result::invalid_argument;
    if (phase_==Phase::drained) return Result::ok;
    if (phase_==Phase::lost_quarantined || phase_==Phase::detached_lost) return Result::device_lost;
    if (!close_requested()) return Result::invalid_argument;
    phase_=Phase::closing;
    bool busy=false;
    for (auto &entry : pending_) {
        if (entry->complete()) continue;
        const auto result=entry->poll(generation);
        if (result==Result::busy) busy=true;
        else if (result!=Result::ok) {
            phase_=Phase::lost_quarantined;
            return result;
        }
    }
    if (busy) return Result::busy;
    pending_.clear(); // completed entries release device/context refs on R
    const auto released=backend_.release_owned_references();
    if (released!=Result::ok) return released;
    phase_=Phase::drained;
    return Result::ok;
}
Result CloseCoordinator::acknowledge_lost_teardown(std::uint64_t generation, bool host_quiesced) {
    if (!backend_.on_owner_thread()) return Result::wrong_thread;
    if (!generation || generation!=backend_.generation() || !host_quiesced ||
        phase_!=Phase::lost_quarantined) return Result::invalid_argument;
    for (auto &entry : pending_) {
        if (entry->complete()) continue;
        const auto released=entry->detach_lost(generation);
        if (released!=Result::ok) return released;
    }
    pending_.clear();
    const auto released=backend_.release_owned_references();
    if (released!=Result::ok) return released;
    phase_=Phase::detached_lost;
    return Result::ok;
}
}
#endif
