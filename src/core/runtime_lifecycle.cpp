#include "runtime_lifecycle.hpp"
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace ars::core {
struct Runtime {
    const ars_runtime_desc descriptor;
    const std::thread::id control=std::this_thread::get_id();
    HandleRegistry registry;
    ars_runtime_h handle{};
    std::uint32_t work=0,management=0;
    bool closing=false,finalizer=false;
    Runtime(const ars_runtime_desc &desc,std::uint64_t uid,const RuntimeBudget &budget)
        :descriptor(desc),registry(uid,budget.slots,budget.pins) {}
};
namespace {
ars_status free_storage(void *storage,const ars_runtime_desc &desc) noexcept {
    if(!storage) return ARS_OK;
    try {
        if(desc.free_fn) desc.free_fn(desc.allocator_user_data,storage,sizeof(Runtime),alignof(Runtime));
        else ::operator delete(storage,std::align_val_t{alignof(Runtime)});
        return ARS_OK;
    } catch(...) { return ARS_E_INTERNAL; }
}
}
RuntimeLease::RuntimeLease(RuntimeLease &&other) noexcept { *this=std::move(other); }
RuntimeLease &RuntimeLease::operator=(RuntimeLease &&other) noexcept {
    if(this!=&other) {
        reset(); owner_=std::exchange(other.owner_,nullptr);
        runtime_=std::exchange(other.runtime_,nullptr); kind_=other.kind_; root_=std::move(other.root_);
    }
    return *this;
}
RuntimeLease::~RuntimeLease() { reset(); }
void RuntimeLease::reset() noexcept {
    auto *owner=std::exchange(owner_,nullptr); auto *runtime=std::exchange(runtime_,nullptr);
    // No lock nesting from registry back to router. Router pin protects the
    // control block until root unpin is complete, including after close intent.
    root_.reset(); if(owner) owner->release(runtime,kind_);
}
RuntimeRouter::RuntimeRouter(RuntimeBudget budget,std::uint64_t initial_uid)
    :budget_(budget),next_uid_(initial_uid) {
    if(!budget_.runtimes || !budget_.slots || !budget_.pins ||
       budget_.runtimes>max_runtime_count || budget_.slots>max_slot_count || budget_.pins>max_pin_count)
        budget_={0,0,0};
    if(initial_uid==0) uid_exhausted_=true;
}
void RuntimeRouter::cancel_reservation(std::uint32_t index) noexcept {
    std::lock_guard lock(mutex_); entries_[index]={};
}
ars_status RuntimeRouter::create(const ars_runtime_desc &desc,ars_runtime_h *out) {
    if(out) *out={};
    if(!out || (desc.alloc==nullptr)!=(desc.free_fn==nullptr)) return ARS_E_INVALID_ARGUMENT;
    std::uint32_t index=max_runtime_count;
    std::uint64_t uid=0;
    {
        std::lock_guard lock(mutex_);
        if(uid_exhausted_) return ARS_E_HANDLE_EXHAUSTED;
        for(std::uint32_t i=0;i<budget_.runtimes;++i) if(!entries_[i].reserved) { index=i; break; }
        if(index==max_runtime_count) return ARS_E_OUT_OF_BUDGET;
        uid=next_uid_;
        if(next_uid_==std::numeric_limits<std::uint64_t>::max()) uid_exhausted_=true;
        else ++next_uid_;
        entries_[index]={uid,nullptr,true};
    }
    void *storage=nullptr; Runtime *runtime=nullptr;
    ars_status status=ARS_E_OUT_OF_BUDGET;
    try {
        storage=desc.alloc ? desc.alloc(desc.allocator_user_data,sizeof(Runtime),alignof(Runtime))
                           : ::operator new(sizeof(Runtime),std::align_val_t{alignof(Runtime)},std::nothrow);
        if(storage) {
            if(reinterpret_cast<std::uintptr_t>(storage)%alignof(Runtime)!=0) status=ARS_E_INVALID_ARGUMENT;
            else {
                runtime=new(storage) Runtime(desc,uid,budget_);
                status=runtime->registry.insert(ObjectType::runtime,runtime,&runtime->handle.id);
                if(status==ARS_OK) {
                    std::lock_guard lock(mutex_); entries_[index].runtime=runtime;
                    *out=runtime->handle; return ARS_OK;
                }
            }
        }
    } catch(const std::bad_alloc &) { status=ARS_E_OUT_OF_BUDGET; }
      catch(...) { status=ARS_E_INTERNAL; }
    cancel_reservation(index);
    if(runtime) runtime->~Runtime();
    if(free_storage(storage,desc)!=ARS_OK) return ARS_E_INTERNAL;
    return status;
}
ars_status RuntimeRouter::resolve_locked(ars_runtime_h handle,Runtime **out) const {
    if(handle.id.runtime_uid==0) return ARS_E_INVALID_ARGUMENT;
    for(std::uint32_t i=0;i<budget_.runtimes;++i) {
        const auto &entry=entries_[i];
        if(entry.reserved && entry.uid==handle.id.runtime_uid && entry.runtime) {
            auto status=entry.runtime->registry.check(handle.id,ObjectType::runtime);
            if(status==ARS_OK) *out=entry.runtime;
            return status;
        }
    }
    return ARS_E_STALE_HANDLE;
}
ars_status RuntimeRouter::acquire(ars_runtime_h handle,LeaseKind kind,RuntimeLease *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    out->reset();
    std::lock_guard lock(mutex_); Runtime *runtime=nullptr;
    auto status=resolve_locked(handle,&runtime); if(status!=ARS_OK) return status;
    if(runtime->finalizer) return ARS_E_BUSY;
    if(kind==LeaseKind::work && runtime->closing) return ARS_E_CLOSED;
    if(runtime->work+runtime->management>=budget_.pins) return ARS_E_BUSY;
    status=runtime->registry.pin(handle.id,ObjectType::runtime,&out->root_);
    if(status!=ARS_OK) return status;
    if(kind==LeaseKind::work) ++runtime->work; else ++runtime->management;
    out->owner_=this; out->runtime_=runtime; out->kind_=kind;
    return ARS_OK;
}
void RuntimeRouter::release(Runtime *runtime,LeaseKind kind) noexcept {
    std::lock_guard lock(mutex_);
    if(kind==LeaseKind::work) --runtime->work; else --runtime->management;
}
ars_status RuntimeRouter::begin_close(ars_runtime_h handle) {
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::work,&lease);
    if(status!=ARS_OK) return status;
    if(lease.runtime_->control!=std::this_thread::get_id()) return ARS_E_WRONG_THREAD;
    std::lock_guard lock(mutex_);
    if(lease.runtime_->closing) return ARS_E_CLOSED;
    lease.runtime_->closing=true; return ARS_OK;
}
ars_status RuntimeRouter::query_close(ars_runtime_h handle,ars_close_state *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::management,&lease);
    if(status!=ARS_OK) return status;
    std::lock_guard lock(mutex_); auto *runtime=lease.runtime_;
    std::memset(out,0,sizeof(*out)); out->struct_size=sizeof(*out); out->abi_version=ARS_ABI_VERSION;
    out->outstanding_pins=runtime->work+runtime->management;
    out->outstanding_close_management_pins=runtime->management;
    out->state=runtime->closing && runtime->work==0 ? ARS_FENCE_COMPLETE:ARS_FENCE_PENDING;
    return ARS_OK;
}
ars_status RuntimeRouter::finish_close(ars_runtime_h handle) {
    Runtime *runtime=nullptr;
    {
        std::lock_guard lock(mutex_);
        auto status=resolve_locked(handle,&runtime); if(status!=ARS_OK) return status;
        if(runtime->finalizer || !runtime->closing) return ARS_E_BUSY;
        runtime->finalizer=true;
        if(runtime->work || runtime->management) { runtime->finalizer=false; return ARS_E_BUSY; }
        status=runtime->registry.invalidate(handle.id,ObjectType::runtime);
        if(status!=ARS_OK) { runtime->finalizer=false; return status; }
        for(auto &entry:entries_) if(entry.runtime==runtime) { entry={}; break; }
    }
    // CPU-only: unlink first, then destroy/free outside every ARS lock. No GPU
    // resources exist in B1; this is not an emulated GPU completion mechanism.
    const auto descriptor=runtime->descriptor;
    runtime->~Runtime(); return free_storage(runtime,descriptor);
}
std::uint32_t RuntimeRouter::occupied() const {
    std::lock_guard lock(mutex_); std::uint32_t count=0;
    for(const auto &entry:entries_) if(entry.reserved) ++count;
    return count;
}
RuntimeRouter &global_router() { static RuntimeRouter router; return router; }
}
