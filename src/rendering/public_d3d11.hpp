#pragma once
#ifdef _WIN32
#include "../core/render_frame.hpp"
#include "scene3d.hpp"
#include <memory>
#include <thread>
#include <atomic>
namespace ars::graphics::d3d11 {struct QueryOps;}
namespace ars::rendering {
struct GpuCreateProbe;
// Internal native test seams. C ABI construction always supplies nullptr.
// Observers run on R outside runtime locks and must not throw or issue GPU work.
struct PublicD3D11Hooks {
    enum class Fence {draw_ready,copy_ready};
    graphics::d3d11::QueryOps *queries=nullptr;
    GpuCreateProbe *creates=nullptr;
    void (*observe)(void *,Fence) noexcept=nullptr;
    void *user=nullptr;
};
struct AcceptedRender {
    struct Character {ars_character_h character{};ars_model_h model{};std::uint64_t pose=0,configuration=0;std::uint32_t mode=0;};
    std::array<Character,16> characters{};
    std::size_t count=0;
    Camera3D camera;
    std::uint64_t target_generation=0,target_epoch=0;
};
// CPU envelope may die on H only after drain has destroyed Impl on R.
class PublicD3D11Host final {
    struct Impl;
    std::unique_ptr<Impl> impl_;
    const std::thread::id owner_=std::this_thread::get_id();
    std::atomic<bool> active_{false};
    PublicD3D11Hooks *hooks_=nullptr;
public:
    explicit PublicD3D11Host(PublicD3D11Hooks *hooks=nullptr);
    ~PublicD3D11Host();
    ars_status render(core::RuntimeRouter &,ars_runtime_h,const ars_render_target &,
        std::shared_ptr<const void> runtime_pin);
    ars_status drain(bool closing);
    bool on_owner() const noexcept{return owner_==std::this_thread::get_id();}
    bool active() const noexcept{return active_.load(std::memory_order_acquire);}
    const AcceptedRender *accepted() const noexcept;
};
}
#endif
