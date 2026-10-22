#include "../../src/core/runtime_state.hpp"
#include "../../src/core/cpu_context.hpp"
#include "../../src/core/render_frame.hpp"
#include "../../src/rendering/committed_scene.hpp"
#include "../../src/security/source_authority.hpp"
#include "../assets/asset_test_support.hpp"
#include <map>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cmath>
using namespace ars;
namespace ars::core {
struct ComponentFixture {
    static Runtime *runtime(ars_runtime_h id){auto &router=global_router();std::lock_guard lock(router.mutex_);Runtime *r=nullptr;assets::require(router.resolve_locked(id,&r)==ARS_OK);return r;}
    static ars_status prepared(security::SourceAuthority *a,const ars_motion_frame *f,security::PreparedFrame *out){return a->prepare(f,out);}
    static void generation(ars_runtime_h id,ars_character_h c,std::uint64_t value){for(auto &ch:runtime(id)->authority->characters_)if(security::same(ch.id.id,c.id)){std::lock_guard lock(ch.pose);ch.pose_generation=value;return;}test::check(false,"character fixture lookup");}
    static std::size_t allocated_sources(ars_runtime_h id){std::size_t count=0;for(auto &s:runtime(id)->authority->sources_)count+=s.allocated?1:0;return count;}
};
}
namespace {
void check(ars_status expected,ars_status actual,const char *name){if(expected!=actual){std::printf("FAIL %s expected=%d actual=%d\n",name,expected,actual);std::exit(1);}}
struct Host {
    std::map<void *,std::pair<std::uint64_t,std::uint32_t>> records;
    std::size_t calls=0,fail_at=0,probes=0;bool probe_all=false,probing=false;
    ars_runtime_h runtime{};const std::vector<std::uint8_t> *input=nullptr;
    void probe() {
        if(!probe_all || probing || !runtime.id.runtime_uid)return;probing=true;
        auto busy=[](ars_status s){test::check(s==ARS_E_BUSY || s==ARS_E_STALE_HANDLE,"allocator reentry rejected without mutation");};
        ars_asset_source src{sizeof(src),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,input->data(),input->size()};ars_model_h m{};
        busy(ars_model_load(runtime,&src,&m));test::check(!m.id.runtime_uid,"recursive load zeros output");
        ars_character_desc d{sizeof(d),ARS_ABI_VERSION,ARS_MODE_AUTO,0};ars_character_h c{};busy(ars_character_create(runtime,m,&d,&c));
        busy(ars_character_destroy(runtime,c));busy(ars_runtime_update(runtime,1));busy(ars_runtime_begin_close(runtime));busy(ars_runtime_finish_close(runtime));
        ars_close_state state{};state.struct_size=sizeof(state);state.abi_version=ARS_ABI_VERSION;const auto s=ars_runtime_query_close(runtime,&state);test::check(s==ARS_OK || s==ARS_E_STALE_HANDLE,"management query may reenter safely");
        ++probes;probing=false;
    }
    static void *ARS_CALL allocate(void *p,std::uint64_t n,std::uint32_t a) {auto &h=*static_cast<Host *>(p);h.probe();if(++h.calls==h.fail_at)return nullptr;auto *v=_aligned_malloc(n?static_cast<std::size_t>(n):1,std::max<std::size_t>(a,sizeof(void *)));if(v)h.records.emplace(v,std::pair{n,a});return v;}
    static void ARS_CALL release(void *p,void *v,std::uint64_t n,std::uint32_t a){auto &h=*static_cast<Host *>(p);h.probe();auto found=h.records.find(v);test::check(found!=h.records.end() && found->second==std::pair{n,a},"exact allocation/free identity");h.records.erase(found);_aligned_free(v);}
};
struct Fixture {
    Host host;ars_runtime_h runtime{};ars_model_h model{};const std::vector<std::uint8_t> &bytes;
    explicit Fixture(const std::vector<std::uint8_t> &input,decltype(ars_runtime_desc::simulation_now_ns) clock=nullptr,void *clock_data=nullptr):bytes(input){host.input=&bytes;ars_runtime_desc d{};d.struct_size=sizeof(d);d.abi_version=ARS_ABI_VERSION;d.alloc=Host::allocate;d.free_fn=Host::release;d.allocator_user_data=&host;d.simulation_now_ns=clock;d.clock_user_data=clock_data;check(ARS_OK,ars_runtime_create(&d,&runtime),"runtime create");host.runtime=runtime;}
    ars_status load(){ars_asset_source s{sizeof(s),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};return ars_model_load(runtime,&s,&model);}
    ars_character_h character(){ars_character_desc d{sizeof(d),ARS_ABI_VERSION,ARS_MODE_AUTO,0};ars_character_h c{};check(ARS_OK,ars_character_create(runtime,model,&d,&c),"character create");return c;}
    std::pair<ars_source_admin_h,ars_ingress_cap> source(ars_character_h c){ars_utf8_view id{"mouth.open",10};ars_source_desc d{};d.struct_size=sizeof(d);d.abi_version=ARS_ABI_VERSION;d.allowed_channel_ids=&id;d.allowed_channel_count=1;d.max_channels_per_frame=1;d.max_frames_per_second=240;d.lease_ttl_ns=security::max_ttl;ars_source_admin_h a{};ars_ingress_cap cap{};check(ARS_OK,ars_source_register(runtime,c,&d,&a,&cap),"source register");return {a,cap};}
    void submit(ars_ingress_cap cap,std::uint64_t sequence,float value){ars_motion_channel ch{{"mouth.open",10},ARS_VALUE_SCALAR,0,{value,0,0,0}};ars_motion_frame frame{};frame.struct_size=sizeof(frame);frame.abi_version=ARS_ABI_VERSION;frame.timestamp_mode=ARS_TIMESTAMP_HOST_MONOTONIC;frame.sequence=sequence;frame.channels=&ch;frame.channel_count=1;frame.confidence=1;check(ARS_OK,ars_motion_submit(runtime,cap,&frame),"public ingress");}
    core::CpuContext &context(){return *core::ComponentFixture::runtime(runtime)->cpu;}
    ~Fixture(){if(runtime.id.runtime_uid){auto s=ars_runtime_begin_close(runtime);test::check(s==ARS_OK || s==ARS_E_CLOSED,"begin cleanup");ars_status finish=ARS_E_BUSY;for(unsigned i=0;i<5000 && finish==ARS_E_BUSY;++i){finish=ars_runtime_finish_close(runtime);if(finish==ARS_E_BUSY)std::this_thread::sleep_for(std::chrono::milliseconds(1));}check(ARS_OK,finish,"finish cleanup after actual OS reap");}test::check(host.records.empty(),"all runtime/model allocations retired");}
};
void lifecycle(const std::vector<std::uint8_t> &bytes) {
    Fixture f(bytes);f.host.probe_all=true;check(ARS_OK,f.load(),"model load with every callback probe");auto c=f.character();auto [a,cap]=f.source(c);
    auto weak=std::weak_ptr<const assets::ValidatedCpuModel>(f.context().characters[0].model);check(ARS_OK,ars_model_release(f.runtime,f.model),"user model release");
    f.submit(cap,1,0.7f);check(ARS_OK,ars_runtime_update(f.runtime,1),"character retains released model");
    core::SnapshotLease old;check(ARS_OK,core::global_router().read_cpu(f.runtime,c,&old),"historical model pin");
    check(ARS_OK,ars_character_destroy(f.runtime,c),"destroy retains old snapshot");check(ARS_OK,ars_source_admin_release(f.runtime,a),"closed admin still releasable");test::check(!weak.expired(),"old snapshot keeps model generation alive");
    check(ARS_OK,ars_runtime_begin_close(f.runtime),"begin with old snapshot");check(ARS_E_BUSY,ars_runtime_finish_close(f.runtime),"finish waits for historical pin");old.reset();
    weak.reset(); // Private weak control blocks must not outlive their Resource.
    check(ARS_OK,ars_runtime_finish_close(f.runtime),"finish after final pin");f.runtime={};test::check(f.host.records.empty() && f.host.probes>0,"model and callbacks fully retired");
}
void render_capture(const std::vector<std::uint8_t> &bytes) {
    Fixture f(bytes);check(ARS_OK,f.load(),"render capture model");auto c=f.character();auto [admin,cap]=f.source(c);
    auto first=std::make_unique<core::RenderFrameLease>();
    check(ARS_E_BUSY,core::global_router().capture_render(f.runtime,c,first.get()),"render requires committed pose");
    f.submit(cap,1,0.7f);check(ARS_OK,ars_runtime_update(f.runtime,1),"render producer update");
    check(ARS_OK,core::global_router().capture_render(f.runtime,c,first.get()),"capture actual runtime snapshot");
    const auto &semantics=first->model()->semantics();
    const auto mouth=std::find_if(semantics.begin(),semantics.end(),[](const auto &s){return s.id=="mouth.open";});
    test::check(mouth!=semantics.end(),"mouth semantic exists");const auto mouth_index=mouth-semantics.begin();
    test::check(first->generation()==1 && first->configuration()==1 && std::abs(first->pose()->outputs[mouth_index].scalar-0.7f)<1e-6f,
        "actual committed pose and config identity");
    check(ARS_E_INVALID_ARGUMENT,core::global_router().capture_render(f.runtime,c,first.get()),"immutable capture cannot be overwritten");
    check(ARS_OK,ars_character_set_mode(f.runtime,c,ARS_MODE_2D),"select2D for projection");
    core::RenderFrameLease second;check(ARS_OK,core::global_router().capture_render(f.runtime,c,&second),"capture2D");
    rendering::PreparedScene2D scene;std::vector<rendering::LayerControl> controls;
    for(auto l:second.layers())controls.push_back({l.visible,l.opacity,l.z});
    check(ARS_OK,rendering::prepare_committed_2d(*second.model(),
        std::span(second.pose()->outputs.data(),second.pose()->channel_count),controls,scene),"project actual2D snapshot");
    test::check(!scene.draws.empty(),"nonempty actual2D geometry");
    check(ARS_OK,ars_model_release(f.runtime,f.model),"captured frame keeps released model");
    check(ARS_OK,ars_character_destroy(f.runtime,c),"destroy captured character");
    test::check(first->generation()==1 && std::abs(first->pose()->outputs[mouth_index].scalar-0.7f)<1e-6f,"historical frame unchanged after destroy");
    check(ARS_OK,ars_source_admin_release(f.runtime,admin),"render source admin cleanup");
    // second and first destruct before Fixture, retaining allocator custody.
    std::puts("PASS committed render capture/immutable config/actual2D/model custody");
}
void recycling(const std::vector<std::uint8_t> &bytes) {
    Fixture f(bytes);check(ARS_OK,f.load(),"recycle model");
    for(unsigned i=0;i<96;++i){auto c=f.character();auto [admin,cap]=f.source(c);(void)cap;check(ARS_OK,ars_character_destroy(f.runtime,c),"cycle destroy");check(ARS_OK,ars_source_admin_release(f.runtime,admin),"cycle admin release");}
    test::check(core::ComponentFixture::allocated_sources(f.runtime)==0,"96 cycles reclaim all64 source slots");
    ars_character_desc d{sizeof(d),ARS_ABI_VERSION,ARS_MODE_AUTO,0};ars_character_h out{{9,9,9}};
    check(ARS_E_STALE_HANDLE,ars_character_create(f.runtime,ars_model_h{f.runtime.id},&d,&out),"wrong object type");test::check(!out.id.runtime_uid,"type failure zero handle");
    Fixture other(bytes);check(ARS_E_STALE_HANDLE,ars_character_create(other.runtime,f.model,&d,&out),"cross-runtime model");test::check(!out.id.runtime_uid,"runtime failure zero handle");
}
void atomic_update(const std::vector<std::uint8_t> &bytes) {
    Fixture f(bytes);check(ARS_OK,f.load(),"batch model");auto first=f.character(),second=f.character();auto [a,cap]=f.source(first);auto [b,cap2]=f.source(second);(void)a;(void)b;
    f.submit(cap,1,0.25f);f.submit(cap2,1,0.75f);core::ComponentFixture::generation(f.runtime,second,UINT64_MAX);
    check(ARS_E_HANDLE_EXHAUSTED,ars_runtime_update(f.runtime,10),"second character failure rejects public whole update");
    test::check(!f.context().simulation_bound && !f.context().has_time && f.context().sources[0].cache.frame.count==0,"failed first update neither binds S/time nor commits first cache");
    core::SnapshotLease absent;check(ARS_E_BUSY,core::global_router().read_cpu(f.runtime,first,&absent),"no earlier partial snapshot");
    core::ComponentFixture::generation(f.runtime,second,0);ars_status result=ARS_E_INTERNAL;std::thread::id winner;
    std::thread s([&]{winner=std::this_thread::get_id();result=ars_runtime_update(f.runtime,10);});s.join();check(ARS_OK,result,"first successful S can differ from failed caller");
    test::check(f.context().simulation_bound && f.context().simulation==winner && f.context().simulation_time==10,"S/time committed with whole snapshot batch");check(ARS_E_WRONG_THREAD,ars_runtime_update(f.runtime,11),"later wrong S rejected");
    core::SnapshotLease one,two;check(ARS_OK,core::global_router().read_cpu(f.runtime,first,&one),"first accepted");check(ARS_OK,core::global_router().read_cpu(f.runtime,second,&two),"second accepted");
    const auto i=static_cast<std::size_t>(std::find_if(f.context().characters[0].model->semantics().begin(),f.context().characters[0].model->semantics().end(),[](const auto &v){return v.id=="mouth.open";})-f.context().characters[0].model->semantics().begin());
    test::check(one.get()->generation==1 && two.get()->generation==1 && one.get()->outputs[i].scalar==0.25f && two.get()->outputs[i].scalar==0.75f,"whole public batch consumes original queued inputs once");
}
void typed(const std::vector<std::uint8_t> &bytes) {
    Fixture f(bytes);check(ARS_OK,f.load(),"typed asset");auto character=f.character();
    const std::array<const char *,5> names{"extra.bool","extra.vec2","extra.vec3","extra.quat","extra.trigger"};
    const std::array<unsigned,5> kinds{ARS_VALUE_BOOL,ARS_VALUE_VEC2,ARS_VALUE_VEC3,ARS_VALUE_QUAT_XYZW,ARS_VALUE_TRIGGER};
    std::array<ars_utf8_view,5> ids{};for(std::size_t i=0;i<ids.size();++i)ids[i]={names[i],static_cast<std::uint32_t>(std::strlen(names[i]))};
    ars_source_desc d{};d.struct_size=sizeof(d);d.abi_version=ARS_ABI_VERSION;d.allowed_channel_ids=ids.data();d.allowed_channel_count=5;d.max_channels_per_frame=5;d.max_frames_per_second=240;d.lease_ttl_ns=security::max_ttl;
    std::array<ars_source_admin_h,2> admin{};std::array<ars_ingress_cap,2> caps{};for(unsigned i=0;i<2;++i)check(ARS_OK,ars_source_register(f.runtime,character,&d,&admin[i],&caps[i]),"all-type public source");
    auto submit=[&](unsigned source,std::uint64_t seq,bool pulse_only){
        std::array<ars_motion_channel,5> channels{};
        for(unsigned i=0;i<5;++i){channels[i].semantic_id=ids[i];channels[i].kind=kinds[i];}
        channels[0].value[0]=source?0.f:1.f;channels[1].value[0]=source?0.f:2.f;channels[1].value[1]=source?0.f:2.f;
        channels[2].value[0]=source?3.f:1.f;channels[2].value[1]=source?4.f:2.f;channels[2].value[2]=source?5.f:3.f;
        channels[3].value[3]=source?-1.f:1.f;channels[4].value[0]=1;
        ars_motion_frame frame{};frame.struct_size=sizeof(frame);frame.abi_version=ARS_ABI_VERSION;frame.sequence=seq;frame.timestamp_mode=ARS_TIMESTAMP_HOST_MONOTONIC;frame.confidence=1;frame.channel_count=pulse_only?1:5;frame.channels=channels.data()+(pulse_only?4:0);
        return ars_motion_submit(f.runtime,caps[source],&frame);
    };
    for(unsigned i=1;i<=16;++i)check(ARS_OK,submit(0,i,i!=1),"fill trigger FIFO");check(ARS_E_QUEUE_FULL,submit(0,17,true),"FIFO saturation rejects without consuming sequence");check(ARS_OK,submit(1,1,false),"opposite quaternion/source");
    auto index=[&](const char *name){const auto &sem=f.context().characters[0].model->semantics();for(std::size_t i=0;i<sem.size();++i)if(sem[i].id==name)return i;test::check(false,"typed semantic lookup");return std::size_t(0);};
    for(unsigned tick=1;tick<=16;++tick){check(ARS_OK,ars_runtime_update(f.runtime,tick),"one FIFO event per update");core::SnapshotLease pin;check(ARS_OK,core::global_router().read_cpu(f.runtime,character,&pin),"read typed public result");const auto &out=pin.get()->outputs;
        test::check(out[index(names[4])].scalar==1 && out[index(names[0])].scalar==1 && out[index(names[1])].scalar==1 && out[index(names[3])].tail[2]==1,"public bool tie/vector clamp/quaternion q-minus-q/one-shot trigger");
        test::check(out[index(names[2])].scalar==(tick==1?2.f:1.f),"retained channel fallback after revoke");
        if(tick==1)check(ARS_OK,ars_source_revoke(f.runtime,admin[1]),"revoke cached typed contributor");
    }
    check(ARS_OK,ars_runtime_update(f.runtime,17),"no stale trigger replay");core::SnapshotLease idle;check(ARS_OK,core::global_router().read_cpu(f.runtime,character,&idle),"idle typed snapshot");test::check(idle.get()->outputs[index(names[4])].scalar==0,"trigger absent after FIFO drained");
    check(ARS_OK,submit(0,17,true),"rejected sequence remains reusable");check(ARS_OK,ars_runtime_update(f.runtime,18),"retried trigger publishes once");
    std::puts("PASS public all-types/cache/revoke/quaternion/FIFO16/no-replay");
}
void bounds_controls(const std::vector<std::uint8_t> &bytes) {
    Fixture f(bytes);std::array<ars_model_h,64> models{};
    for(auto &m:models){check(ARS_OK,f.load(),"fill64 public models");m=f.model;}
    f.model={{9,9,9}};check(ARS_E_OUT_OF_BUDGET,f.load(),"65th model bounded");test::check(!f.model.id.runtime_uid,"model exhaustion zero output");f.model=models[0];
    std::array<ars_character_h,16> chars{};for(auto &c:chars)c=f.character();
    ars_character_desc d{sizeof(d),ARS_ABI_VERSION,ARS_MODE_AUTO,0};ars_character_h extra{{9,9,9}};
    check(ARS_E_OUT_OF_BUDGET,ars_character_create(f.runtime,f.model,&d,&extra),"17th character bounded");test::check(!extra.id.runtime_uid,"character exhaustion zero output");
    auto &record=f.context().characters[0];const auto original=record.mode;
    check(ARS_E_INVALID_ARGUMENT,ars_character_set_mode(f.runtime,chars[0],0),"invalid mode");test::check(record.mode==original,"invalid mode retains previous mode");
    check(ARS_OK,ars_character_set_mode(f.runtime,chars[0],ARS_MODE_3D),"dual mode3D");
    const auto &layer=record.model->layers()[0];ars_layer_change change{sizeof(change),ARS_ABI_VERSION,{layer.id.data(),static_cast<std::uint32_t>(layer.id.size())},0,0.25f,123,0};
    check(ARS_E_UNSUPPORTED,ars_character_set_visual_layer(f.runtime,chars[0],&change),"layer2D only");
    check(ARS_OK,ars_character_set_mode(f.runtime,chars[0],ARS_MODE_2D),"dual mode2D");check(ARS_OK,ars_character_set_visual_layer(f.runtime,chars[0],&change),"local layer control");
    test::check(!record.layers[0].visible && record.layers[0].opacity==0.25f && record.layers[0].z==123,"exact local layer state");change.opacity=2;
    check(ARS_E_INVALID_ARGUMENT,ars_character_set_visual_layer(f.runtime,chars[0],&change),"invalid layer rollback");test::check(record.layers[0].opacity==0.25f,"rejected layer retains state");
    for(auto m:models)check(ARS_OK,ars_model_release(f.runtime,m),"release64 users while16 characters retain model");
    check(ARS_OK,ars_runtime_update(f.runtime,1),"public16 character atomic update after user release");
    for(auto c:chars){core::SnapshotLease pin;check(ARS_OK,core::global_router().read_cpu(f.runtime,c,&pin),"all16 snapshots published");test::check(pin.get()->generation==1,"all16 generation1");}
    for(auto c:chars)check(ARS_OK,ars_character_destroy(f.runtime,c),"destroy16");
    check(ARS_OK,f.load(),"model slot reused after full retirement");auto recycled=f.character();(void)recycled;
    std::puts("PASS public models64/characters16/exhaustion/reuse/mode/layer/atomic16");
}
struct Clock {
    std::atomic<std::uint64_t> value{100};
    std::atomic<bool> block{false},entered{false},release{false};
    std::atomic<unsigned> calls{0},inflight{0},maximum{0};
    static std::uint64_t ARS_CALL now(void *data) {
        auto &c=*static_cast<Clock *>(data);++c.calls;
        const auto n=++c.inflight;auto m=c.maximum.load();while(m<n && !c.maximum.compare_exchange_weak(m,n)){}
        c.entered=true;while(n==1 && c.block && !c.release)std::this_thread::yield();
        const auto result=c.value.load();--c.inflight;return result;
    }
};
void clocks(const std::vector<std::uint8_t> &bytes) {
    Clock clock;Fixture f(bytes,Clock::now,&clock);check(ARS_OK,f.load(),"clock model");auto c=f.character();auto [admin,cap]=f.source(c);(void)admin;
    ars_motion_channel ch{{"mouth.open",10},ARS_VALUE_SCALAR,0,{0.8f,0,0,0}};
    ars_motion_frame frame{};frame.struct_size=sizeof(frame);frame.abi_version=ARS_ABI_VERSION;frame.sequence=1;frame.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;frame.confidence=1;frame.channels=&ch;frame.channel_count=1;
    check(ARS_OK,ars_motion_submit(f.runtime,cap,&frame),"ingress clock100");clock.value=90;
    check(ARS_E_CLOCK_REGRESSION,ars_runtime_update(f.runtime,0),"first update respects ingress highwater");
    test::check(!f.context().simulation_bound && !f.context().has_time && !f.context().sources[0].cache.frame.count,"clock rejection preserves S/time/cache");
    core::SnapshotLease absent;check(ARS_E_BUSY,core::global_router().read_cpu(f.runtime,c,&absent),"clock rejection has no snapshot");
    clock.value=110;check(ARS_OK,ars_runtime_update(f.runtime,0),"clock recovery consumes original frame");
    clock.value=105;frame.sequence=2;check(ARS_E_CLOCK_REGRESSION,ars_motion_submit(f.runtime,cap,&frame),"ingress respects update highwater");
    // Hold a real ingress callback open. S must neither invoke it concurrently nor publish.
    clock.value=120;clock.block=true;clock.entered=false;ars_status ingress=ARS_E_INTERNAL;
    std::thread i([&]{ingress=ars_motion_submit(f.runtime,cap,&frame);});
    while(!clock.entered)std::this_thread::yield();const auto calls=clock.calls.load();
    check(ARS_E_BUSY,ars_runtime_update(f.runtime,0),"S excluded while ingress clock active");
    test::check(clock.calls==calls && f.context().simulation_time==110,"busy sampling leaves time and callback untouched");
    clock.release=true;i.join();check(ARS_OK,ingress,"failed sequence reusable after clock rejection");
    check(ARS_OK,ars_runtime_update(f.runtime,0),"update after callback exclusion");
    test::check(clock.maximum==1,"host clock never overlaps");
    // Model the exact already-admitted ingress window: resume its private prepare
    // after S entered the host callback, bypassing only the earlier API admission.
    Clock other_clock;Fixture other(bytes,Clock::now,&other_clock);check(ARS_OK,other.load(),"overlap model");auto other_character=other.character();(void)other_character;
    auto *authority=core::ComponentFixture::runtime(other.runtime)->authority;
    other_clock.block=true;ars_status update=ARS_E_INTERNAL;
    std::thread simulation([&]{update=ars_runtime_update(other.runtime,0);});
    while(!other_clock.entered)std::this_thread::yield();security::PreparedFrame prepared;
    const auto already_admitted=core::ComponentFixture::prepared(authority,&frame,&prepared);
    other_clock.release=true;simulation.join();
    check(ARS_E_BUSY,already_admitted,"already-admitted ingress cannot overlap S clock");check(ARS_OK,update,"S callback resumes");
    test::check(other_clock.calls==1 && other_clock.maximum==1,"exclusive shared guard covers preadmitted ingress window");
    std::puts("PASS public shared clock exclusion/highwater/rollback/retry");
}
void oom(const std::vector<std::uint8_t> &bytes) {
    std::size_t allocations=0;
    {Fixture f(bytes);const auto before=f.host.calls;check(ARS_OK,f.load(),"OOM baseline");allocations=f.host.calls-before;}
    for(std::size_t i=1;i<=allocations;++i){Fixture f(bytes);f.host.fail_at=f.host.calls+i;check(ARS_E_OUT_OF_BUDGET,f.load(),"load OOM at each observed allocation");test::check(!f.model.id.runtime_uid,"failed model output zero");f.host.fail_at=0;}
    {Fixture f(bytes);check(ARS_OK,f.load(),"create OOM model");f.host.fail_at=f.host.calls+1;ars_character_desc d{sizeof(d),ARS_ABI_VERSION,ARS_MODE_AUTO,0};ars_character_h c{{9,9,9}};check(ARS_E_OUT_OF_BUDGET,ars_character_create(f.runtime,f.model,&d,&c),"authority allocation OOM");test::check(!c.id.runtime_uid,"failed character output zero");f.host.fail_at=0;c=f.character();auto [a,cap]=f.source(c);(void)a;f.submit(cap,1,0.5f);f.host.fail_at=f.host.calls+1;check(ARS_E_OUT_OF_BUDGET,ars_runtime_update(f.runtime,1),"publication allocation OOM");test::check(!f.context().simulation_bound,"failed allocation does not bind S");f.host.fail_at=0;check(ARS_OK,ars_runtime_update(f.runtime,1),"publication retry retains queued input");}
    std::printf("PASS public OOM all%zu model allocations +character/publication allocations\n",allocations);
}
}
int main(int argc,char **argv){if(argc!=2 && argc!=3)return 2;auto bytes=test::file(argv[1]);if(argc==3){typed(bytes);return 0;}render_capture(bytes);lifecycle(bytes);recycling(bytes);atomic_update(bytes);clocks(bytes);bounds_controls(bytes);oom(bytes);std::puts("PASS public CPU lifetime/admin96/type/runtime/S/atomic/OOM/reentry");}
