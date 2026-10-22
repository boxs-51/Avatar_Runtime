#pragma once
#ifdef _WIN32
#include "host_device.hpp"
#include "query_retirement.hpp"
#include "teardown_receipt.hpp"
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
    HostTeardownReceipt::Authority host_authority_;
    Phase phase_=Phase::active;
public:
    CloseCoordinator(ID3D11Device *device, ID3D11DeviceContext *context,
                     std::uint64_t generation, QueryOps *ops=nullptr,
                     HostTeardownReceipt::Authority authority={});
    CloseCoordinator(const CloseCoordinator &)=delete;
    CloseCoordinator &operator=(const CloseCoordinator &)=delete;
    Result submit(ID3D11RenderTargetView *target, TargetInfo info,
                  std::shared_ptr<const void> snapshot_pin);
    Result begin_close() noexcept;
    Result poll_close(std::uint64_t generation);
    // Accept only the registered host's issued, single-use generation receipt.
    Result acknowledge_lost_teardown(std::uint64_t generation, HostTeardownReceipt &receipt);
    Phase phase() const noexcept { return phase_; } // R-thread observation only
    std::size_t pending_count() const noexcept { return pending_.size(); }
    bool close_requested() const noexcept { return close_requested_.load(std::memory_order_acquire); }
};
}
#endif
