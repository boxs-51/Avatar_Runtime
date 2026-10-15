#include "runtime_test_support.hpp"
#include "handle_registry.hpp"
#include <barrier>
#include <limits>
#include <thread>
#include <utility>
using namespace ars::core;

int main() {
    int first=42,second=7;
    HandleRegistry registry(11,1,2);
    ars_handle_id a{},b{};
    test::status(ARS_OK,registry.insert(ObjectType::character,&first,&a),"REG-insert-typed-slot");
    RegistryLease pin,extra,overflow;
    test::status(ARS_OK,registry.pin(a,ObjectType::character,&pin),"REG-pin-before-access");
    test::check(pin.payload()==&first,"REG-payload-through-pin");
    test::status(ARS_E_STALE_HANDLE,registry.pin(a,ObjectType::model,&extra),"REG-type-confusion");
    auto forged=a; forged.slot=UINT32_MAX;
    test::status(ARS_E_STALE_HANDLE,registry.pin(forged,ObjectType::character,&extra),"REG-slot-bounds");
    forged=a; forged.runtime_uid=12;
    test::status(ARS_E_STALE_HANDLE,registry.pin(forged,ObjectType::character,&extra),"REG-runtime-namespace");
    test::status(ARS_OK,registry.pin(a,ObjectType::character,&extra),"REG-second-pin");
    test::status(ARS_E_BUSY,registry.pin(a,ObjectType::character,&overflow),"REG-bounded-pins");
    test::status(ARS_OK,registry.invalidate(a,ObjectType::character),"REG-logical-invalidation");
    test::status(ARS_E_STALE_HANDLE,registry.check(a,ObjectType::character),"REG-stale-after-invalidate");
    test::check(*static_cast<int *>(pin.payload())==42,"REG-storage-stays-live-under-old-pin");
    test::status(ARS_E_OUT_OF_BUDGET,registry.insert(ObjectType::model,&second,&b),"REG-no-reuse-while-pinned");
    RegistryLease moved(std::move(pin));
    test::check(!pin && moved,"REG-move-ownership");
    extra.reset(); moved.reset();
    test::check(registry.outstanding_pins()==0,"REG-balanced-direct-unpin-after-invalidation");
    test::status(ARS_OK,registry.insert(ObjectType::model,&second,&b),"REG-reuse-with-new-generation");
    test::check(a.slot==b.slot && a.generation!=b.generation,"REG-no-ABA");
    test::status(ARS_E_STALE_HANDLE,registry.pin(a,ObjectType::character,&pin),"REG-old-generation-never-aliases");
    test::status(ARS_OK,registry.invalidate(b,ObjectType::model),"REG-second-invalidate");
    {
        HandleRegistry exhausted(15,1,2,std::numeric_limits<std::uint32_t>::max());
        test::status(ARS_OK,exhausted.insert(ObjectType::model,&first,&a),"REG-max-generation-issued-once");
        test::status(ARS_OK,exhausted.invalidate(a,ObjectType::model),"REG-terminal-generation-retirement");
        test::status(ARS_E_OUT_OF_BUDGET,exhausted.insert(ObjectType::model,&first,&b),"REG-retired-slot-not-recycled");
        test::check(b.runtime_uid==0,"REG-exhaustion-zero-output");
    }
    for(unsigned i=0;i<100000;++i) {
        RegistryLease lease;
        if(registry.insert(ObjectType::character,&first,&a)!=ARS_OK ||
           registry.pin(a,ObjectType::character,&lease)!=ARS_OK ||
           registry.invalidate(a,ObjectType::character)!=ARS_OK) return 1;
    }
    test::check(registry.active_slots()==0 && registry.outstanding_pins()==0,"REG-100000-seeded-slot-cycles-no-metadata-growth");
    {
        HandleRegistry concurrent(22,1,8);
        test::status(ARS_OK,concurrent.insert(ObjectType::character,&first,&a),"REG-race-setup");
        std::barrier pinned(9),invalidated(9);
        std::thread workers[8];
        for(auto &worker:workers) worker=std::thread([&]{
            RegistryLease lease;
            if(concurrent.pin(a,ObjectType::character,&lease)!=ARS_OK) std::exit(2);
            pinned.arrive_and_wait(); invalidated.arrive_and_wait();
            if(*static_cast<int *>(lease.payload())!=42) std::exit(3);
        });
        pinned.arrive_and_wait();
        test::status(ARS_OK,concurrent.invalidate(a,ObjectType::character),"REG-eight-pins-vs-invalidate");
        test::status(ARS_E_OUT_OF_BUDGET,concurrent.insert(ObjectType::model,&second,&b),"REG-race-no-reuse");
        invalidated.arrive_and_wait(); for(auto &worker:workers) worker.join();
        test::check(concurrent.outstanding_pins()==0,"REG-race-balanced-late-unpins");
    }
    std::puts("PASS B1 registry production-component tests; no public character/GPU claim");
}
