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
        RuntimeRouter router({1,1,2},std::numeric_limits<std::uint64_t>::max()); ars_runtime_h runtime{},out{};
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
    std::puts("PASS B1 production CPU lifecycle component suite; 87 whole security cases remain HOLD");
}
