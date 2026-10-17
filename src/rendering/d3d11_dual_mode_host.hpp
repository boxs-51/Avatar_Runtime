#pragma once
#ifdef _WIN32
#include "d3d11_scene2d.hpp"
#include "d3d11_scene3d.hpp"
#include <atomic>
#include <memory>

namespace ars::rendering {
// Private M2-D host transaction. Only begin_close may be called off R.
// The host owns the published offscreen target; publication follows exact
// completion of a candidate query, so an unready frame cannot replace it.
class D3D11DualModeHost {
public:
    enum class Mode { none, two_d, three_d };
private:
    struct Pending;
    graphics::d3d11::HostDevice backend_;
    graphics::d3d11::QueryOps *ops_=nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> active_texture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> active_view_;
    std::unique_ptr<Pending> pending_;
    std::atomic<bool> closing_{false};
    Mode mode_=Mode::none;
    std::uint64_t pose_generation_=0, target_epoch_=0;
    std::uint32_t width_=0, height_=0;
    bool lost_=false, closed_=false;
    graphics::Result prepare_target(Pending &,std::uint32_t,std::uint32_t,bool);
    graphics::Result start(std::unique_ptr<Pending>,graphics::Result);
public:
    D3D11DualModeHost(ID3D11Device *,ID3D11DeviceContext *,std::uint64_t generation,
                      graphics::d3d11::QueryOps *ops=nullptr);
    ~D3D11DualModeHost();
    D3D11DualModeHost(const D3D11DualModeHost &)=delete;
    D3D11DualModeHost &operator=(const D3D11DualModeHost &)=delete;
    graphics::Result request_2d(std::shared_ptr<const PreparedScene2D>,
        std::shared_ptr<const assets::Image>,std::uint64_t pose_generation,
        std::uint32_t width,std::uint32_t height,bool fail_target_create=false);
    graphics::Result request_3d(std::shared_ptr<const PreparedScene3D>,
        const Camera3D &,std::uint64_t pose_generation,
        std::uint32_t width,std::uint32_t height,GpuCreateProbe *probe=nullptr,
        bool fail_target_create=false);
    graphics::Result poll_commit();
    graphics::Result begin_close() noexcept;
    graphics::Result poll_close();
    // Isolated private host only: all its context users and targets are owned
    // here. This does not prove teardown of an external production host.
    graphics::Result detach_lost();
    bool accepts(ID3D11RenderTargetView *,std::uint64_t target_epoch) const noexcept;
    Mode mode() const noexcept { return mode_; }
    std::uint64_t pose_generation() const noexcept { return pose_generation_; }
    std::uint64_t target_epoch() const noexcept { return target_epoch_; }
    ID3D11RenderTargetView *target() const noexcept { return active_view_.Get(); }
    ID3D11Texture2D *texture() const noexcept { return active_texture_.Get(); }
    bool pending() const noexcept { return pending_!=nullptr; }
    bool lost() const noexcept { return lost_; }
};
} // namespace ars::rendering
#endif
