#pragma once
#ifdef _WIN32
#include "../backend.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

namespace ars::rendering { class D3D11DualModeHost; class PublicD3D11Host; }
namespace ars::graphics::d3d11 {
class CloseCoordinator;
class PrivateTeardownHost;
class ContextOwnership;

// A private, single-use statement from the host that owned the old context.
// It is intentionally not a public recovery or physical-device-loss claim.
class HostTeardownReceipt final {
    struct State {
        State(std::uint64_t generation, ID3D11Device *device, ID3D11DeviceContext *context=nullptr,
              std::shared_ptr<const void> pin={})
            : generation(generation), device(device), owner(std::this_thread::get_id()),
              context(context), runtime_pin(std::move(pin)) {}
        const std::uint64_t generation;
        ID3D11Device *const device;
        const std::thread::id owner;
        std::atomic<bool> closing{false};
        std::size_t active_users=0, gpu_owners=0;
        ID3D11DeviceContext *const context;
        std::shared_ptr<const void> runtime_pin;
        bool issued=false, consumed=false;
    };
public:
    class Authority final {
        std::shared_ptr<State> state_;
        explicit Authority(std::shared_ptr<State> state) : state_(std::move(state)) {}
        friend class PrivateTeardownHost;
        friend class ContextOwnership;
        friend class ars::rendering::D3D11DualModeHost;
        friend class CloseCoordinator;
        friend class HostTeardownReceipt;
    public:
        Authority()=default;
    };
    HostTeardownReceipt()=default;
    HostTeardownReceipt(const HostTeardownReceipt &)=delete;
    HostTeardownReceipt &operator=(const HostTeardownReceipt &)=delete;
    HostTeardownReceipt(HostTeardownReceipt &&)=default;
    HostTeardownReceipt &operator=(HostTeardownReceipt &&)=default;
private:
    std::shared_ptr<State> state_;
    explicit HostTeardownReceipt(std::shared_ptr<State> state) : state_(std::move(state)) {}
    bool matches(const Authority &authority,std::uint64_t generation,ID3D11Device *device) const noexcept {
        return state_ && state_==authority.state_ && state_->issued && !state_->consumed &&
            state_->generation==generation && state_->device==device && state_->context &&
            state_->owner==std::this_thread::get_id() && !state_->active_users && !state_->gpu_owners;
    }
    void consume() noexcept {state_->consumed=true;state_->runtime_pin.reset();state_.reset();}
    friend class ContextOwnership;
    friend class ars::rendering::D3D11DualModeHost;
    friend class ars::rendering::PublicD3D11Host;
    friend class PrivateTeardownHost;
    friend class CloseCoordinator;
};

// Owns one isolated host's context/target references and tracks its admitted
// context users. A production host must wire every caller before using this.
class PrivateTeardownHost final {
    std::shared_ptr<HostTeardownReceipt::State> state_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
public:
    PrivateTeardownHost(ID3D11Device *, ID3D11DeviceContext *,
                        ID3D11RenderTargetView *, std::uint64_t generation);
    PrivateTeardownHost(const PrivateTeardownHost &)=delete;
    PrivateTeardownHost &operator=(const PrivateTeardownHost &)=delete;
    HostTeardownReceipt::Authority authority() const { return HostTeardownReceipt::Authority(state_); }
    Result admit_user();
    Result release_user();
    Result begin_close() noexcept;
    Result issue(HostTeardownReceipt &out);
};
} // namespace ars::graphics::d3d11
#endif
