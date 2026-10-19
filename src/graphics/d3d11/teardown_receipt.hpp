#pragma once
#ifdef _WIN32
#include "../backend.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

namespace ars::graphics::d3d11 {
class CloseCoordinator;
class PrivateTeardownHost;

// A private, single-use statement from the host that owned the old context.
// It is intentionally not a public recovery or physical-device-loss claim.
class HostTeardownReceipt final {
    struct State {
        State(std::uint64_t generation, ID3D11Device *device)
            : generation(generation), device(device), owner(std::this_thread::get_id()) {}
        const std::uint64_t generation;
        ID3D11Device *const device;
        const std::thread::id owner;
        std::atomic<bool> closing{false};
        std::size_t active_users=0;
        bool issued=false, consumed=false;
    };
public:
    class Authority final {
        std::shared_ptr<State> state_;
        explicit Authority(std::shared_ptr<State> state) : state_(std::move(state)) {}
        friend class PrivateTeardownHost;
        friend class CloseCoordinator;
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
