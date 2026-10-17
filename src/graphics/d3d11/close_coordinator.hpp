#pragma once
#ifdef _WIN32
#include "host_device.hpp"
#include "query_retirement.hpp"
#include <atomic>
#include <memory>
#include <vector>

namespace ars::graphics::d3d11 {
// Private host-device lifecycle. All methods except begin_close run on R.
class CloseCoordinator {
public:
    enum class Phase { active, closing, drained, lost_quarantined, detached_lost };
private:
    HostDevice backend_;
    QueryOps *ops_;
    std::atomic<bool> close_requested_{false};
    std::vector<std::unique_ptr<QueryRetirement>> pending_;
    Phase phase_=Phase::active;
public:
    CloseCoordinator(ID3D11Device *device, ID3D11DeviceContext *context,
                     std::uint64_t generation, QueryOps *ops=nullptr);
    CloseCoordinator(const CloseCoordinator &)=delete;
    CloseCoordinator &operator=(const CloseCoordinator &)=delete;
    Result submit(ID3D11RenderTargetView *target, TargetInfo info,
                  std::shared_ptr<const void> snapshot_pin);
    Result begin_close() noexcept;
    Result poll_close(std::uint64_t generation);
    // The host guarantees no further use of the old device/context before true.
    Result acknowledge_lost_teardown(std::uint64_t generation, bool host_quiesced);
    Phase phase() const noexcept { return phase_; } // R-thread observation only
    std::size_t pending_count() const noexcept { return pending_.size(); }
    bool close_requested() const noexcept { return close_requested_.load(std::memory_order_acquire); }
};
}
#endif
