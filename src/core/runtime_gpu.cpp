#include "runtime_state.hpp"
#include "../rendering/public_d3d11.hpp"
namespace ars::core {
ars_status RuntimeRouter::gpu_render(ars_runtime_h handle,const ars_render_target *target) {
    if(!target)return ARS_E_INVALID_ARGUMENT;
    auto pin=std::make_shared<RuntimeLease>();auto status=acquire(handle,LeaseKind::work,pin.get());
    if(status!=ARS_OK)return status;
    std::shared_ptr<rendering::PublicD3D11Host> gpu;
    auto candidate=std::make_shared<rendering::PublicD3D11Host>();
    {
        std::lock_guard lock(mutex_);auto *r=pin->runtime_;
        if(r->closing)return ARS_E_CLOSED;
        if(!r->gpu)r->gpu=std::move(candidate);gpu=r->gpu;
    }
    if(!gpu->on_owner())return ARS_E_WRONG_THREAD;
    return gpu->render(*this,handle,*target,pin);
}
ars_status RuntimeRouter::gpu_drain(ars_runtime_h handle) {
    RuntimeLease lease;auto status=acquire(handle,LeaseKind::management,&lease);if(status!=ARS_OK)return status;
    std::shared_ptr<rendering::PublicD3D11Host> gpu;bool closing=false;
    {std::lock_guard lock(mutex_);gpu=lease.runtime_->gpu;closing=lease.runtime_->closing;}
    if(!gpu)return ARS_OK;
    if(!gpu->on_owner())return ARS_E_WRONG_THREAD;
    // Management custody outlives the final work-pin release inside R drain.
    status=gpu->drain(closing);
    if(status==ARS_OK&&closing){std::lock_guard lock(mutex_);lease.runtime_->gpu.reset();}
    return status;
}
}
