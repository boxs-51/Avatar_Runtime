#include "../../src/core/runtime_state.hpp"
#include "../../src/core/cpu_context.hpp"
#include "asset_test_support.hpp"
#include <chrono>
#include <thread>
namespace ars::core {
struct ComponentFixture {
    static CpuContext *context(ars_runtime_h id){auto &router=global_router();std::lock_guard lock(router.mutex_);Runtime *r=nullptr;assets::require(router.resolve_locked(id,&r)==ARS_OK);return r->cpu;}
};
}
int main(int argc,char **argv) {
    if(argc!=3)return 2;const bool orphan=std::string_view(argv[2])=="orphan";
    auto bytes=test::file(argv[1]);test::Host host;ars_runtime_h runtime{};
    ars_runtime_desc desc{};desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;desc.alloc=test::Host::allocate;desc.free_fn=test::Host::release;desc.allocator_user_data=&host;
    test::check(ars_runtime_create(&desc,&runtime)==ARS_OK,"custody runtime");
    ars::assets::hold_cpu_cleanup_for_test(true);
    ars_asset_source source{sizeof(source),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};ars_model_h model{{9,9,9}};
    test::check(ars_model_load(runtime,&source,&model)==ARS_E_BUSY && !model.id.runtime_uid,"unknown cleanup returns no model");
    auto *context=ars::core::ComponentFixture::context(runtime);test::check(context->quarantine && bool(context->quarantine_pin),"real OS custody retains runtime work pin");
    if(orphan){context->quarantine.reset();test::check(bool(context->quarantine_pin),"accidental custody drop keeps runtime pin and transfers OS state");}
    test::check(ars_runtime_begin_close(runtime)==ARS_OK,"close while cleanup unknown");ars_close_state state{};state.struct_size=sizeof(state);state.abi_version=ARS_ABI_VERSION;
    test::check(ars_runtime_query_close(runtime,&state)==ARS_OK && state.state==ARS_FENCE_PENDING && state.outstanding_pins>1,"close reports real pending work");
    test::check(ars_runtime_finish_close(runtime)==ARS_E_BUSY,"finish does not fabricate cleanup");
    ars_runtime_h second{};test::check(ars_runtime_create(&desc,&second)==ARS_OK,"other runtime");
    test::check(ars_model_load(second,&source,&model)==ARS_E_BUSY && !model.id.runtime_uid,"global gate prevents overlapping jobs");
    test::check(ars_runtime_begin_close(second)==ARS_OK,"close other runtime");
    // Global orphan recovery can make management conservative until cleanup is
    // known; releasing the fault below reaps actual OS state, not a fake report.
    ars::assets::hold_cpu_cleanup_for_test(false);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for(;;) {
        test::check(ars_runtime_query_close(runtime,&state)==ARS_OK,"query actual OS reap");
        if(state.state==ARS_FENCE_COMPLETE)break;
        test::check(state.state==ARS_FENCE_PENDING && state.outstanding_pins>1,"pending OS cleanup keeps actual work pin");
        test::check(std::chrono::steady_clock::now()<deadline,"actual OS cleanup completes within bounded observation");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    test::check(state.state==ARS_FENCE_COMPLETE,"actual OS reap releases retained custody");
    test::check(ars_runtime_finish_close(runtime)==ARS_OK,"finish after actual custody release");
    test::check(ars_runtime_finish_close(second)==ARS_OK,"finish other runtime");test::check(host.live==0,"custody allocator balanced");
    std::puts(orphan?"PASS orphan gate/OS custody recovery/close pin":"PASS runtime OS custody quarantine/close pin");
}
