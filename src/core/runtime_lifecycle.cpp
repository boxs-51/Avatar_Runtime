#include "runtime_lifecycle.hpp"
#include "../security/source_authority.hpp"
#include "../motion/publish_gate.hpp"
#include "../abi/api_validation.hpp"
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <thread>
#include <memory>
#include <utility>

namespace ars::core {
struct Runtime {
    const ars_runtime_desc descriptor;
    const std::thread::id control=std::this_thread::get_id();
    HandleRegistry registry;
    ars_runtime_h handle{};
    std::uint32_t work=0,management=0,ingress=0,snapshots=0;
    bool closing=false,quiescing=false,finalizer=false;
    security::SourceAuthority *authority=nullptr;
    motion::PublishGate *publication=nullptr;
    bool publication_allocating=false;
    security::CallbackState callback;
    const std::uint64_t authority_budget;
    const std::uint64_t publication_budget;
    Runtime(const ars_runtime_desc &desc,std::uint64_t uid,const RuntimeBudget &budget)
        :descriptor(desc),registry(uid,budget.slots,budget.pins),authority_budget(budget.authority_bytes),publication_budget(budget.publication_bytes) {}
};
namespace {
ars_status release_block(void *storage,std::uint64_t bytes,std::uint32_t alignment,const ars_runtime_desc &desc) noexcept {
    if(!storage) return ARS_OK;
    try {
        if(desc.free_fn) desc.free_fn(desc.allocator_user_data,storage,bytes,alignment);
        else ::operator delete(storage,std::align_val_t{alignment});
        return ARS_OK;
    } catch(...) { return ARS_E_INTERNAL; }
}
ars_status free_authority(security::SourceAuthority *authority,const ars_runtime_desc &desc) noexcept {
    if(!authority) return ARS_OK;
    authority->~SourceAuthority();
    return release_block(authority,sizeof(security::SourceAuthority),alignof(security::SourceAuthority),desc);
}
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
StageLease::~StageLease() { reset(); }
void StageLease::reset() noexcept {
    if(owner_) owner_->discard(stage_);
    owner_=nullptr; stage_=nullptr; runtime_.reset();
}
SnapshotLease::~SnapshotLease() { reset(); }
void SnapshotLease::reset() noexcept { motion::Snapshot::release(snapshot_); snapshot_=nullptr; runtime_.reset(); }
void RuntimeLease::reset() noexcept {
    auto *owner=std::exchange(owner_,nullptr); auto *runtime=std::exchange(runtime_,nullptr);
    // No lock nesting from registry back to router. Router pin protects the
    // control block until root unpin is complete, including after close intent.
    root_.reset(); if(owner) owner->release(runtime,kind_);
}
RuntimeRouter::RuntimeRouter(RuntimeBudget budget)
    :budget_(budget),next_uid_(1) {
    if(!budget_.runtimes || !budget_.slots || !budget_.pins ||
       budget_.runtimes>max_runtime_count || budget_.slots>max_slot_count || budget_.pins>max_pin_count)
        budget_={0,0,0};
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
    if(kind!=LeaseKind::management && runtime->callback.active()) return ARS_E_BUSY;
    if(kind!=LeaseKind::management && runtime->closing) return ARS_E_CLOSED;
    if(runtime->work+runtime->management>=budget_.pins) return ARS_E_BUSY;
    status=runtime->registry.pin(handle.id,ObjectType::runtime,&out->root_);
    if(status!=ARS_OK) return status;
    if(kind==LeaseKind::management) ++runtime->management;
    else { ++runtime->work; if(kind==LeaseKind::ingress) ++runtime->ingress; if(kind==LeaseKind::snapshot) ++runtime->snapshots; }
    out->owner_=this; out->runtime_=runtime; out->kind_=kind;
    return ARS_OK;
}
void RuntimeRouter::release(Runtime *runtime,LeaseKind kind) noexcept {
    std::lock_guard lock(mutex_);
    if(kind==LeaseKind::management) --runtime->management;
    else { --runtime->work; if(kind==LeaseKind::ingress) --runtime->ingress; if(kind==LeaseKind::snapshot) --runtime->snapshots; }
}
ars_status RuntimeRouter::begin_close(ars_runtime_h handle) {
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::work,&lease);
    if(status!=ARS_OK) return status;
    if(lease.runtime_->control!=std::this_thread::get_id()) return ARS_E_WRONG_THREAD;
    auto *runtime=lease.runtime_;
    security::MutationLease mutation(runtime->callback); if(!mutation) return ARS_E_BUSY;
    auto *authority=authority_for(lease);
    if(!authority) {
        std::lock_guard lock(mutex_); runtime->closing=runtime->quiescing=true; return ARS_OK;
    }
    std::array<motion::Snapshot *,security::max_characters> retired{};
    {
        std::lock_guard commit(authority->commit);
        authority->request_close();
        { std::lock_guard lock(mutex_); runtime->closing=true; }
        authority->drain(); // CV wait holds neither router nor source/pose/clock locks.
        authority->close_all(&retired);
        { std::lock_guard lock(mutex_); runtime->quiescing=true; }
    }
    for(auto *snapshot:retired) motion::Snapshot::release(snapshot);
    return ARS_OK;
}
ars_status RuntimeRouter::query_close(ars_runtime_h handle,ars_close_state *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::management,&lease);
    if(status!=ARS_OK) return status;
    std::lock_guard lock(mutex_); auto *runtime=lease.runtime_;
    std::memset(out,0,sizeof(*out)); out->struct_size=sizeof(*out); out->abi_version=ARS_ABI_VERSION;
    out->outstanding_pins=runtime->work+runtime->management;
    out->outstanding_close_management_pins=runtime->management;
    out->outstanding_ingress_pins=runtime->ingress;
    out->outstanding_render_snapshot_pins=runtime->snapshots; // CPU immutable snapshot leases, no GPU work.
    out->state=runtime->closing && runtime->quiescing && runtime->work==0 ? ARS_FENCE_COMPLETE:ARS_FENCE_PENDING;
    return ARS_OK;
}
ars_status RuntimeRouter::finish_close(ars_runtime_h handle) {
    Runtime *runtime=nullptr;
    {
        std::lock_guard lock(mutex_);
        auto status=resolve_locked(handle,&runtime); if(status!=ARS_OK) return status;
        if(runtime->callback.active()) return ARS_E_BUSY;
        if(runtime->finalizer || !runtime->quiescing) return ARS_E_BUSY;
        runtime->finalizer=true;
        if(runtime->work || runtime->management) { runtime->finalizer=false; return ARS_E_BUSY; }
        status=runtime->registry.invalidate(handle.id,ObjectType::runtime);
        if(status!=ARS_OK) { runtime->finalizer=false; return status; }
        for(auto &entry:entries_) if(entry.runtime==runtime) { entry={}; break; }
    }
    // CPU-only: unlink first, then destroy/free outside every ARS lock. No GPU
    // resources exist in B1; this is not an emulated GPU completion mechanism.
    const auto descriptor=runtime->descriptor;
    ars_status publication_status=ARS_OK;
    if(runtime->publication) {
        runtime->publication->~PublishGate();
        publication_status=release_block(runtime->publication,sizeof(motion::PublishGate),alignof(motion::PublishGate),descriptor);
    }
    const auto authority_status=free_authority(runtime->authority,descriptor);
    runtime->~Runtime(); const auto status=free_storage(runtime,descriptor);
    return publication_status!=ARS_OK?publication_status:(authority_status!=ARS_OK?authority_status:status);
}
std::uint32_t RuntimeRouter::occupied() const {
    std::lock_guard lock(mutex_); std::uint32_t count=0;
    for(const auto &entry:entries_) if(entry.reserved) ++count;
    return count;
}
security::SourceAuthority *RuntimeRouter::authority_for(const RuntimeLease &lease) const {
    std::lock_guard lock(mutex_);
    return lease.runtime_?lease.runtime_->authority:nullptr;
}
ars_status RuntimeRouter::ensure_authority(RuntimeLease &lease,security::SourceAuthority **out) {
    auto *runtime=lease.runtime_;
    if(auto *existing=authority_for(lease)) { *out=existing; return ARS_OK; }
    static_assert(sizeof(security::SourceAuthority)+sizeof(Runtime)<=max_authority_bytes);
    if(runtime->authority_budget<sizeof(security::SourceAuthority)+sizeof(Runtime)) return ARS_E_OUT_OF_BUDGET;
    auto descriptor=runtime->descriptor; void *storage=nullptr;
    security::SourceAuthority *authority=nullptr;
    ars_status failure=ARS_E_INTERNAL;
    try {
        storage=descriptor.alloc?descriptor.alloc(descriptor.allocator_user_data,sizeof(security::SourceAuthority),alignof(security::SourceAuthority)):
            ::operator new(sizeof(security::SourceAuthority),std::align_val_t{alignof(security::SourceAuthority)},std::nothrow);
        if(!storage) return ARS_E_OUT_OF_BUDGET;
        if(reinterpret_cast<std::uintptr_t>(storage)%alignof(security::SourceAuthority)!=0) {
            auto status=release_block(storage,sizeof(security::SourceAuthority),alignof(security::SourceAuthority),descriptor);
            return status==ARS_OK?ARS_E_INVALID_ARGUMENT:status;
        }
        authority=new(storage) security::SourceAuthority(runtime->registry,descriptor,runtime->handle.id.runtime_uid,runtime->callback);
    } catch(const std::bad_alloc &) { failure=ARS_E_OUT_OF_BUDGET; }
      catch(...) { failure=ARS_E_INTERNAL; }
    if(!authority) {
        auto status=release_block(storage,sizeof(security::SourceAuthority),alignof(security::SourceAuthority),descriptor);
        return status==ARS_OK?failure:status;
    }
    {
        std::lock_guard lock(mutex_);
        if(!runtime->closing && !runtime->authority) { runtime->authority=authority; *out=authority; return ARS_OK; }
    }
    auto status=free_authority(authority,descriptor);
    return status==ARS_OK?ARS_E_CLOSED:status;
}
ars_status RuntimeRouter::bind_character(ars_runtime_h handle,std::span<const security::SemanticChannel> catalog,ars_character_h *out) {
    if(out) *out={};
    if(!out) return ARS_E_INVALID_ARGUMENT;
    auto status=validate_catalog(catalog); if(status!=ARS_OK) return status;
    RuntimeLease lease; status=acquire(handle,LeaseKind::work,&lease); if(status!=ARS_OK) return status;
    security::MutationLease mutation(lease.runtime_->callback); if(!mutation) return ARS_E_BUSY;
    if(lease.runtime_->control!=std::this_thread::get_id()) return ARS_E_WRONG_THREAD;
    security::SourceAuthority *authority=nullptr;
    status=ensure_authority(lease,&authority); if(status!=ARS_OK) return status;
    return authority->bind(catalog,out);
}
ars_status RuntimeRouter::destroy_character(ars_runtime_h handle,ars_character_h character) {
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::work,&lease); if(status!=ARS_OK) return status;
    security::MutationLease mutation(lease.runtime_->callback); if(!mutation) return ARS_E_BUSY;
    if(lease.runtime_->control!=std::this_thread::get_id()) return ARS_E_WRONG_THREAD;
    auto *authority=authority_for(lease); motion::Snapshot *retired=nullptr;
    status=authority?authority->destroy(character,&retired):ARS_E_STALE_HANDLE;
    motion::Snapshot::release(retired); return status;
}
ars_status RuntimeRouter::register_source(ars_runtime_h handle,ars_character_h character,const ars_source_desc *desc,ars_source_admin_h *out,ars_ingress_cap *cap) {
    if(out) *out={};
    if(cap) *cap={};
    if(!out || !cap || !ars::abi::valid(desc)) return ARS_E_INVALID_ARGUMENT;
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::work,&lease); if(status!=ARS_OK) return status;
    security::MutationLease mutation(lease.runtime_->callback); if(!mutation) return ARS_E_BUSY;
    auto *authority=authority_for(lease); return authority?authority->register_source(character,desc,out,cap):ARS_E_STALE_HANDLE;
}
ars_status RuntimeRouter::rotate_source(ars_runtime_h handle,ars_source_admin_h source,ars_ingress_cap *out) {
    if(out) *out={};
    if(!out) return ARS_E_INVALID_ARGUMENT;
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::work,&lease); if(status!=ARS_OK) return status;
    security::MutationLease mutation(lease.runtime_->callback); if(!mutation) return ARS_E_BUSY;
    auto *authority=authority_for(lease); return authority?authority->rotate(source,out):ARS_E_STALE_HANDLE;
}
ars_status RuntimeRouter::revoke_source(ars_runtime_h handle,ars_source_admin_h source) {
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::work,&lease); if(status!=ARS_OK) return status;
    security::MutationLease mutation(lease.runtime_->callback); if(!mutation) return ARS_E_BUSY;
    auto *authority=authority_for(lease); return authority?authority->revoke(source):ARS_E_STALE_HANDLE;
}
ars_status RuntimeRouter::release_source(ars_runtime_h handle,ars_source_admin_h source) {
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::work,&lease); if(status!=ARS_OK) return status;
    security::MutationLease mutation(lease.runtime_->callback); if(!mutation) return ARS_E_BUSY;
    auto *authority=authority_for(lease); return authority?authority->release(source):ARS_E_STALE_HANDLE;
}
ars_status RuntimeRouter::submit_frame(ars_runtime_h handle,ars_ingress_cap cap,const ars_motion_frame *frame) {
    if(!ars::abi::valid(frame)) return ARS_E_INVALID_ARGUMENT;
    RuntimeLease lease; auto status=acquire(handle,LeaseKind::ingress,&lease); if(status!=ARS_OK) return status;
    auto *authority=authority_for(lease); return authority?authority->submit(cap,frame):ARS_E_SCOPE_DENIED;
}
ars_status RuntimeRouter::dequeue_frame(ars_runtime_h handle,ars_source_admin_h source,FrameLease *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    out->reset(); RuntimeLease runtime; auto status=acquire(handle,LeaseKind::work,&runtime); if(status!=ARS_OK) return status;
    security::MutationLease mutation(runtime.runtime_->callback); if(!mutation) return ARS_E_BUSY;
    auto *authority=authority_for(runtime);
    status=authority?authority->dequeue(source,&out->frame,&out->source_,&out->character_):ARS_E_STALE_HANDLE;
    if(status!=ARS_OK) out->reset(); else out->runtime_=std::move(runtime);
    return status;
}
ars_status RuntimeRouter::ensure_publication(RuntimeLease &lease,motion::PublishGate **out) {
    static_assert(sizeof(motion::PublishGate)<=max_publication_bytes);
    auto *runtime=lease.runtime_; auto *authority=authority_for(lease);
    if(!authority) return ARS_E_STALE_HANDLE;
    {
        std::lock_guard lock(mutex_);
        if(runtime->publication) { *out=runtime->publication; return ARS_OK; }
        if(runtime->publication_allocating) return ARS_E_BUSY;
        runtime->publication_allocating=true;
    }
    auto cancel=[this](Runtime *reserved) { std::lock_guard lock(mutex_); reserved->publication_allocating=false; };
    std::unique_ptr<Runtime,decltype(cancel)> reservation(runtime,cancel);
    if(runtime->publication_budget<sizeof(motion::PublishGate)) return ARS_E_OUT_OF_BUDGET;
    void *storage=nullptr; motion::PublishGate *publication=nullptr;
    ars_status failure=ARS_E_INTERNAL; const auto descriptor=runtime->descriptor;
    try {
        storage=descriptor.alloc?descriptor.alloc(descriptor.allocator_user_data,sizeof(motion::PublishGate),alignof(motion::PublishGate)):
            ::operator new(sizeof(motion::PublishGate),std::align_val_t{alignof(motion::PublishGate)},std::nothrow);
        if(!storage) return ARS_E_OUT_OF_BUDGET;
        if(reinterpret_cast<std::uintptr_t>(storage)%alignof(motion::PublishGate)!=0) {
            auto status=release_block(storage,sizeof(motion::PublishGate),alignof(motion::PublishGate),descriptor);
            return status==ARS_OK?ARS_E_INVALID_ARGUMENT:status;
        }
        publication=new(storage) motion::PublishGate(*authority);
    } catch(const std::bad_alloc &) { failure=ARS_E_OUT_OF_BUDGET; } catch(...) { failure=ARS_E_INTERNAL; }
    if(!publication) {
        const auto status=release_block(storage,sizeof(motion::PublishGate),alignof(motion::PublishGate),descriptor);
        return status==ARS_OK?failure:status;
    }
    {
        std::lock_guard lock(mutex_);
        if(!runtime->closing && !runtime->publication) { runtime->publication=publication; *out=publication; return ARS_OK; }
        if(!runtime->closing) *out=runtime->publication;
    }
    publication->~PublishGate();
    const auto status=release_block(storage,sizeof(motion::PublishGate),alignof(motion::PublishGate),descriptor);
    return status!=ARS_OK?status:(*out?ARS_OK:ARS_E_CLOSED);
}
ars_status RuntimeRouter::prepare_cpu(ars_runtime_h handle,ars_character_h character,std::span<const ars_source_admin_h> sources,const motion::Program &program,StageLease *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    out->reset(); RuntimeLease runtime; auto status=acquire(handle,LeaseKind::work,&runtime); if(status!=ARS_OK) return status;
    security::MutationLease mutation(runtime.runtime_->callback); if(!mutation) return ARS_E_BUSY;
    status=ensure_publication(runtime,&out->owner_);
    if(status==ARS_OK) status=out->owner_->prepare(character,sources,program,&out->stage_);
    if(status!=ARS_OK) out->reset(); else out->runtime_=std::move(runtime);
    return status;
}
ars_status RuntimeRouter::publish_cpu(StageLease *lease) {
    if(!lease || lease->runtime_.owner_!=this || !lease->owner_ || !lease->stage_) return ARS_E_INVALID_ARGUMENT;
    RuntimeLease runtime=std::move(lease->runtime_);
    security::MutationLease mutation(runtime.runtime_->callback);
    const auto status=mutation?lease->owner_->publish(*lease->stage_):ARS_E_BUSY;
    lease->reset(); return status;
}
ars_status RuntimeRouter::read_cpu(ars_runtime_h handle,ars_character_h character,SnapshotLease *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    out->reset(); auto status=acquire(handle,LeaseKind::snapshot,&out->runtime_); if(status!=ARS_OK) return status;
    motion::PublishGate *publication=nullptr;
    { std::lock_guard lock(mutex_); publication=out->runtime_.runtime_->publication; }
    status=publication?publication->read(character,&out->snapshot_):ARS_E_BUSY;
    if(status!=ARS_OK) out->reset();
    return status;
}
RuntimeRouter &global_router() { static RuntimeRouter router; return router; }
}
