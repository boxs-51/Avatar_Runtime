#include "runtime_test_support.hpp"
#include "runtime_lifecycle.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <limits>
#include <new>
#include <thread>
#include <utility>
using namespace ars::core;

namespace {
ars_runtime_desc descriptor() {
    ars_runtime_desc desc{}; desc.struct_size=sizeof(desc); desc.abi_version=ARS_ABI_VERSION; return desc;
}
struct Allocation {
    RuntimeRouter *router=nullptr;
    std::atomic<unsigned> allocations{0},frees{0},reentered{0};
    std::uint64_t bytes=0; std::uint32_t alignment=0;
    bool fail=false,throw_error=false,misalign=false;
    void *raw=nullptr;
    test::Gate *allocation_gate=nullptr,*free_gate=nullptr;
};
void *ARS_CALL allocate(void *data,uint64_t bytes,uint32_t alignment) {
    auto &state=*static_cast<Allocation *>(data);
    ++state.allocations; (void)state.router->occupied(); ++state.reentered;
    state.bytes=bytes; state.alignment=alignment;
    if(state.allocation_gate) state.allocation_gate->arrive_and_wait();
    if(state.throw_error) throw 17;
    if(state.fail) return nullptr;
    state.raw=::operator new(static_cast<std::size_t>(bytes)+alignment,std::align_val_t{alignment},std::nothrow);
    if(state.misalign && state.raw) return static_cast<unsigned char *>(state.raw)+1;
    return state.raw;
}
void ARS_CALL release(void *data,void *ptr,uint64_t bytes,uint32_t alignment) {
    auto &state=*static_cast<Allocation *>(data);
    (void)state.router->occupied(); ++state.reentered;
    if(bytes!=state.bytes || alignment!=state.alignment) std::exit(7);
    if(ptr!=(state.misalign?static_cast<void *>(static_cast<unsigned char *>(state.raw)+1):state.raw)) std::exit(8);
    if(state.free_gate) state.free_gate->arrive_and_wait();
    ::operator delete(state.raw,std::align_val_t{alignment}); ++state.frees;
}
ars_runtime_desc custom(Allocation &state) {
    auto desc=descriptor(); desc.alloc=allocate; desc.free_fn=release; desc.allocator_user_data=&state; return desc;
}
struct AuthorityAllocation {
    struct Block { void *raw=nullptr,*returned=nullptr; std::uint64_t bytes=0; std::uint32_t alignment=0; };
    std::array<Block,4> blocks{};
    RuntimeRouter *router=nullptr;
    ars_runtime_h runtime{};
    unsigned calls=0,frees=0,reentries=0,mode=0;
};
void *ARS_CALL allocate_authority(void *data,std::uint64_t bytes,std::uint32_t alignment) {
    auto &state=*static_cast<AuthorityAllocation *>(data); ++state.calls;
    (void)state.router->occupied(); ++state.reentries;
    if(state.calls==2 && state.mode==1) return nullptr;
    if(state.calls==2 && state.mode==2) throw 17;
    if(state.calls==2 && state.mode==4 && state.router->begin_close(state.runtime)!=ARS_OK) std::exit(30);
    for(auto &block:state.blocks) if(!block.raw) {
        block.raw=::operator new(static_cast<std::size_t>(bytes)+alignment,std::align_val_t{alignment},std::nothrow);
        if(!block.raw) return nullptr;
        block.returned=state.calls==2 && state.mode==3?static_cast<unsigned char *>(block.raw)+1:block.raw;
        block.bytes=bytes; block.alignment=alignment; return block.returned;
    }
    return nullptr;
}
void ARS_CALL free_authority_allocation(void *data,void *pointer,std::uint64_t bytes,std::uint32_t alignment) {
    auto &state=*static_cast<AuthorityAllocation *>(data); (void)state.router->occupied(); ++state.reentries;
    for(auto &block:state.blocks) if(block.returned==pointer) {
        if(block.bytes!=bytes || block.alignment!=alignment) std::exit(31);
        ::operator delete(block.raw,std::align_val_t{alignment}); block={}; ++state.frees; return;
    }
    std::exit(32);
}
}

int main() {
    const auto desc=descriptor();
    {
        RuntimeRouter router({2,2,4}); ars_runtime_h runtime{};
        test::status(ARS_OK,router.create(desc,&runtime),"LIFE-create-real-core");
        RuntimeLease work,management;
        test::status(ARS_OK,router.acquire(runtime,LeaseKind::work,&work),"LIFE-old-work-lease");
        test::status(ARS_OK,router.begin_close(runtime),"LIFE-close-does-not-wait-for-old-CPU-pin");
        test::status(ARS_E_CLOSED,router.acquire(runtime,LeaseKind::work,&management),"LIFE-close-blocks-new-work");
        test::status(ARS_E_BUSY,router.finish_close(runtime),"LIFE-finish-busy-on-work");
        work.reset();
        test::status(ARS_OK,router.acquire(runtime,LeaseKind::management,&management),"LIFE-management-exception");
        ars_close_state snapshot{};
        test::status(ARS_OK,router.query_close(runtime,&snapshot),"LIFE-query-during-close");
        test::check(snapshot.outstanding_pins==2 && snapshot.outstanding_close_management_pins==2,
                    "LIFE-query-counts-own-and-other-management-leases");
        std::array<ars_status,2> result{}; std::barrier start(3);
        std::thread a([&]{start.arrive_and_wait(); result[0]=router.finish_close(runtime);});
        std::thread b([&]{start.arrive_and_wait(); result[1]=router.finish_close(runtime);});
        start.arrive_and_wait(); a.join(); b.join();
        test::check(result[0]==ARS_E_BUSY && result[1]==ARS_E_BUSY,"LIFE-two-finalizers-denied-by-live-query-pin");
        test::status(ARS_OK,router.query_close(runtime,&snapshot),"LIFE-unsuccessful-finalizer-releases-claim");
        RuntimeLease moved(std::move(management)); management=std::move(moved); management.reset();
        test::status(ARS_OK,router.finish_close(runtime),"LIFE-direct-unpin-enables-finalize");
        test::status(ARS_E_STALE_HANDLE,router.query_close(runtime,&snapshot),"LIFE-unlinked-runtime-stale");
        ars_runtime_h next{};
        test::status(ARS_OK,router.create(desc,&next),"LIFE-create-after-finalize");
        test::check(next.id.runtime_uid!=runtime.id.runtime_uid,"LIFE-no-runtime-UID-reuse");
        test::status(ARS_OK,router.begin_close(next),"LIFE-next-close");
        test::status(ARS_OK,router.finish_close(next),"LIFE-next-finalize");
    }
    {
        RuntimeRouter router({1,1,1}); ars_runtime_h runtime{},overflow{};
        test::status(ARS_OK,router.create(desc,&runtime),"LIMIT-one-runtime");
        test::status(ARS_E_OUT_OF_BUDGET,router.create(desc,&overflow),"LIMIT-runtime-capacity");
        test::check(!overflow.id.runtime_uid,"LIMIT-capacity-zero-output");
        RuntimeLease work,other;
        test::status(ARS_OK,router.acquire(runtime,LeaseKind::work,&work),"LIMIT-one-pin");
        test::status(ARS_E_BUSY,router.acquire(runtime,LeaseKind::work,&other),"LIMIT-pin-contention");
        work.reset(); test::status(ARS_OK,router.begin_close(runtime),"LIMIT-release-and-close");
        test::status(ARS_OK,router.finish_close(runtime),"LIMIT-cleanup");
    }
    {
        // Isolated real router instance, never a reset of global authority.
        RuntimeRouter router({1,1,2}); ars_runtime_h runtime{},out{};
        ComponentFixture::initial_uid(router,std::numeric_limits<std::uint64_t>::max());
        test::status(ARS_OK,router.create(desc,&runtime),"UID-last-value");
        test::status(ARS_OK,router.begin_close(runtime),"UID-close-last");
        test::status(ARS_OK,router.finish_close(runtime),"UID-finish-last");
        test::status(ARS_E_HANDLE_EXHAUSTED,router.create(desc,&out),"UID-terminal-exhaustion");
        test::check(out.id.runtime_uid==0,"UID-exhaustion-zero-handle");
    }
    {
        RuntimeRouter router({1,2,4}); Allocation state; state.router=&router;
        ars_runtime_h runtime{};
        state.fail=true;
        test::status(ARS_E_OUT_OF_BUDGET,router.create(custom(state),&runtime),"OOM-null-allocator-result");
        test::check(!runtime.id.runtime_uid && router.occupied()==0,"OOM-no-partial-entry");
        state.fail=false; state.throw_error=true;
        test::status(ARS_E_INTERNAL,router.create(custom(state),&runtime),"OOM-allocator-exception-contained");
        test::check(router.occupied()==0,"OOM-throw-reservation-rollback");
        state.throw_error=false; state.misalign=true;
        test::status(ARS_E_INVALID_ARGUMENT,router.create(custom(state),&runtime),"OOM-bad-alignment-rejected-before-construction");
        test::check(state.frees==1 && router.occupied()==0,"OOM-bad-alignment-freed-outside-locks");
        state.misalign=false;
        test::status(ARS_OK,router.create(custom(state),&runtime),"ALLOC-recover-after-failed-creations");
        test::check(runtime.id.runtime_uid>=4,"ALLOC-failed-UIDs-never-reused");
        test::status(ARS_OK,router.begin_close(runtime),"ALLOC-close");
        test::status(ARS_OK,router.finish_close(runtime),"ALLOC-finalize");
        test::check(state.frees==2 && state.reentered==state.allocations+state.frees,"ALLOC-paired-sizes-alignment-and-reentrancy");
    }
    {
        RuntimeRouter router({1,2,4}); Allocation state; state.router=&router;
        test::Gate gate; state.allocation_gate=&gate; ars_runtime_h runtime{},second{};
        std::thread creator([&]{test::status(ARS_OK,router.create(custom(state),&runtime),"ALLOC-reserved-create-event");
            // begin_close is control-thread only; the creator owns this object.
            test::status(ARS_OK,router.begin_close(runtime),"ALLOC-reserved-owner-close");});
        gate.wait_entered();
        test::status(ARS_E_OUT_OF_BUDGET,router.create(desc,&second),"ALLOC-reservation-bounds-concurrent-create");
        gate.release(); creator.join();
        test::status(ARS_OK,router.finish_close(runtime),"ALLOC-management-finalizer-other-thread");
    }
    {
        RuntimeRouter router; Allocation state; state.router=&router;
        test::Gate gate; state.free_gate=&gate; ars_runtime_h runtime{};
        test::status(ARS_OK,router.create(custom(state),&runtime),"FINAL-free-setup");
        test::status(ARS_OK,router.begin_close(runtime),"FINAL-free-begin");
        std::thread finalizer([&]{test::status(ARS_OK,router.finish_close(runtime),"FINAL-free-complete");});
        gate.wait_entered(); ars_close_state snapshot{};
        test::status(ARS_E_STALE_HANDLE,router.query_close(runtime,&snapshot),"FINAL-unlink-before-host-free");
        test::status(ARS_E_STALE_HANDLE,router.finish_close(runtime),"FINAL-second-caller-cannot-double-free");
        gate.release(); finalizer.join(); test::check(state.frees==1,"FINAL-exactly-one-free");
    }
    test::check(global_router().occupied()==0,"GLOBAL-component-fixtures-do-not-reset-global-router");
    {
        RuntimeRouter limited({1,8,8,1}); ars_runtime_h runtime{}; ars_character_h character{};
        test::status(ARS_OK,limited.create(desc,&runtime),"B2-BUDGET-bare-runtime-still-small");
        std::array<ars::security::SemanticChannel,1> catalog{}; std::memcpy(catalog[0].id.data(),"mouth.open",11);
        test::status(ARS_E_OUT_OF_BUDGET,limited.bind_character(runtime,catalog,&character),"B2-BUDGET-authority-byte-reservation");
        test::check(!character.id.runtime_uid,"B2-BUDGET-no-partial-character");
        test::status(ARS_OK,limited.begin_close(runtime),"B2-BUDGET-runtime-remains-closeable");
        test::status(ARS_OK,limited.finish_close(runtime),"B2-BUDGET-cleanup");
    }
    {
        test::Authority authority; ars_source_admin_h source{}; ars_ingress_cap cap{};
        test::status(ARS_OK,authority.register_source(&source,&cap),"B2-PIN-register");
        test::Frame input; test::status(ARS_OK,authority.router.submit_frame(authority.runtime,cap,&input.value),"B2-PIN-admit");
        FrameLease retained_frame;
        test::status(ARS_OK,authority.router.dequeue_frame(authority.runtime,source,&retained_frame),"B2-PIN-owned-frame-root-character-source");
        test::status(ARS_OK,authority.router.destroy_character(authority.runtime,authority.character),"B2-PIN-logical-character-destroy");
        test::status(ARS_OK,authority.router.release_source(authority.runtime,source),"B2-PIN-admin-release-after-character-destroy");
        test::status(ARS_OK,authority.router.begin_close(authority.runtime),"B2-PIN-runtime-close");
        authority.retained.reset();
        test::status(ARS_E_BUSY,authority.router.finish_close(authority.runtime),"B2-PIN-frame-blocks-reclaim");
        test::check(retained_frame.frame.channels[0].value[0]==0.5F,"B2-PIN-owned-frame-data-after-logical-invalidation");
        retained_frame.reset(); ars_close_state close{};
        test::status(ARS_OK,authority.router.query_close(authority.runtime,&close),"B2-PIN-direct-unpin-after-stale-handle");
        test::check(close.state==ARS_FENCE_COMPLETE && close.outstanding_pins==1,"B2-PIN-no-hidden-object-root-reference");
    }
    for(unsigned mode=0;mode<=4;++mode) {
        RuntimeRouter host; AuthorityAllocation state; state.router=&host; state.mode=mode;
        auto host_desc=descriptor(); host_desc.alloc=allocate_authority; host_desc.free_fn=free_authority_allocation; host_desc.allocator_user_data=&state;
        test::status(ARS_OK,host.create(host_desc,&state.runtime),"B2-ALLOC-root-host-pair");
        std::array<ars::security::SemanticChannel,1> catalog{}; std::memcpy(catalog[0].id.data(),"mouth.open",11);
        ars_character_h character{}; const std::array<ars_status,5> expected{ARS_OK,ARS_E_OUT_OF_BUDGET,ARS_E_INTERNAL,ARS_E_INVALID_ARGUMENT,ARS_E_CLOSED};
        test::status(expected[mode],host.bind_character(state.runtime,catalog,&character),"B2-ALLOC-lazy-context-failure-and-reentry");
        if(mode!=0) test::check(!character.id.runtime_uid,"B2-ALLOC-no-published-partial-binding");
        if(mode!=4) {
            if(mode!=0) test::status(ARS_OK,host.bind_character(state.runtime,catalog,&character),"B2-ALLOC-clean-context-retry");
            test::status(ARS_OK,host.begin_close(state.runtime),"B2-ALLOC-close");
        }
        test::status(ARS_OK,host.finish_close(state.runtime),"B2-ALLOC-finalize-all-ownership");
        test::check(std::all_of(state.blocks.begin(),state.blocks.end(),[](auto &block){return block.raw==nullptr;}) &&
                    state.reentries==state.calls+state.frees,"B2-ALLOC-paired-bytes-alignment-and-outside-locks");
    }
    {
        test::Authority authority; ars::core::RuntimeLease snapshot;
        test::status(ARS_OK,authority.router.acquire(authority.runtime,ars::core::LeaseKind::snapshot,&snapshot),"B3-CPU-snapshot-root-category");
        authority.retained.reset();
        test::status(ARS_OK,authority.router.begin_close(authority.runtime),"B3-CPU-close-with-snapshot-root");
        ars_close_state state{}; test::status(ARS_OK,authority.router.query_close(authority.runtime,&state),"B3-CPU-snapshot-counter-query");
        test::check(state.outstanding_pins==2 && state.outstanding_render_snapshot_pins==1 && state.outstanding_gpu_retires==0,
                    "B3-CPU-snapshot-does-not-hide-root-or-invent-GPU-work");
        test::status(ARS_E_BUSY,authority.router.finish_close(authority.runtime),"B3-CPU-finalizer-keeps-snapshot-storage");
        snapshot.reset();
    }
    std::puts("PASS B1 production CPU lifecycle component suite; 87 whole security cases remain HOLD");
}
