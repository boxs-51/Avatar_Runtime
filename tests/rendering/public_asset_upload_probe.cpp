// Native test-only attachment of the existing renderer observation seam.
// No runtime behavior, public exports, failure injection or device proxy is added.
#include "../../src/core/runtime_state.hpp"
#include "../../src/rendering/public_d3d11.hpp"
#include "../../src/rendering/d3d11_scene3d.hpp"
namespace ars::core {
struct ComponentFixture {
    static Runtime *runtime(ars_runtime_h handle){
        auto &router=global_router();std::lock_guard lock(router.mutex_);
        Runtime *result=nullptr;
        return router.resolve_locked(handle,&result)==ARS_OK?result:nullptr;
    }
};
}
namespace {
ars::rendering::GpuCreateProbe creates;
ars::rendering::PublicD3D11Hooks hooks;
}
extern "C" int qualification_bind_gpu(ars_runtime_h handle){
    auto *runtime=ars::core::ComponentFixture::runtime(handle);
    if(!runtime||runtime->gpu)return 0;
    hooks.creates=&creates;
    runtime->gpu=std::make_shared<ars::rendering::PublicD3D11Host>(&hooks);
    return 1;
}
extern "C" std::uint64_t qualification_gpu_attempts(){return creates.attempts;}
