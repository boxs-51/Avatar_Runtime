// Public ABI mutations on a validated model; native-only observers/scheduler seams.
#include "../../src/core/runtime_state.hpp"
#include "../../src/core/cpu_context.hpp"
#include "../../src/security/source_authority.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <barrier>
#include <condition_variable>
#include <fstream>
#include <iterator>
#include <latch>
#include <limits>
#include <map>
#include <malloc.h>
#include <thread>
#include <vector>
#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL line=%d %s\n",__LINE__,#x);std::exit(1);}} while(0)
namespace ars::core {
struct ComponentFixture {
    static Runtime *runtime(ars_runtime_h h){auto &r=global_router();std::lock_guard lock(r.mutex_);Runtime *p=nullptr;REQUIRE(r.resolve_locked(h,&p)==ARS_OK);return p;}
    static auto &source(security::SourceAuthority &a,ars_source_admin_h h){for(auto &s:a.sources_)if(security::same(s.id.id,h.id))return s;std::exit(2);}
    static auto &queue(security::SourceAuthority &a,ars_source_admin_h h){return a.queues_[source(a,h).data].queue;}
    static void clock(security::SourceAuthority &a,ars_status(*read)(void*,uint64_t*)noexcept,void *user){a.clock_.read_=read;a.clock_.user_=user;a.clock_.init_=ARS_OK;a.clock_.highwater_=0;a.clock_.fault_=false;}
    static void append_failure(security::SourceAuthority &a,ars_source_admin_h h,bool v){queue(a,h).fail_append_=v;}
    static uint32_t issued(security::SourceAuthority &a){return a.ledger_.used_;}
    static void admitted(security::SourceAuthority &a,unsigned n){auto &g=a.admission_;std::unique_lock lock(g.mutex_);REQUIRE(g.cv_.wait_for(lock,std::chrono::seconds(10),[&]{return g.active_==n;}));}
    static void closing(security::SourceAuthority &a){auto &g=a.admission_;std::unique_lock lock(g.mutex_);REQUIRE(g.cv_.wait_for(lock,std::chrono::seconds(10),[&]{return g.closing_;}));}
};
}
namespace {
using Probe=ars::core::ComponentFixture;
struct Gate {
    std::mutex mutex;std::condition_variable cv;bool entered=false,released=false;
    void pause(){std::unique_lock lock(mutex);entered=true;cv.notify_all();REQUIRE(cv.wait_for(lock,std::chrono::seconds(10),[&]{return released;}));}
    void wait(){std::unique_lock lock(mutex);REQUIRE(cv.wait_for(lock,std::chrono::seconds(10),[&]{return entered;}));}
    void release(){std::lock_guard lock(mutex);released=true;cv.notify_all();}
};
struct Clock {
    std::atomic<Gate*> gate{nullptr};
    static ars_status read(void *p,uint64_t *out)noexcept{auto &c=*static_cast<Clock*>(p);if(auto *g=c.gate.exchange(nullptr))g->pause();*out=100;return ARS_OK;}
};
struct Allocator {
    std::mutex mutex;std::map<void*,std::pair<uint64_t,uint32_t>> live;
    static void *ARS_CALL allocate(void *p,uint64_t n,uint32_t a){auto &s=*static_cast<Allocator*>(p);auto *v=_aligned_malloc(static_cast<size_t>(n?n:1),std::max<size_t>(a,sizeof(void*)));if(v){std::lock_guard lock(s.mutex);REQUIRE(s.live.emplace(v,std::pair{n,a}).second);}return v;}
    static void ARS_CALL release(void *p,void *v,uint64_t n,uint32_t a){auto &s=*static_cast<Allocator*>(p);std::lock_guard lock(s.mutex);auto it=s.live.find(v);REQUIRE(it!=s.live.end() && it->second==std::make_pair(n,a));s.live.erase(it);_aligned_free(v);}
};
struct Binding {ars_source_admin_h admin{};ars_ingress_cap cap{};};
struct Frame {
    ars_motion_channel channel{{"mouth.open",10},ARS_VALUE_SCALAR,0,{.75f,0,0,0}};ars_motion_frame frame{};
    explicit Frame(uint64_t seq=1){frame.struct_size=sizeof frame;frame.abi_version=ARS_ABI_VERSION;frame.sequence=seq;frame.timestamp_mode=ARS_TIMESTAMP_HOST_MONOTONIC;frame.timestamp_ns=100;frame.confidence=1;frame.channel_count=1;frame.channels=&channel;}
};
struct Pose {uint64_t generation;float mouth,yaw,quat_w;};
struct Fixture {
    Allocator allocator;Clock clock;ars_runtime_h runtime{};ars_model_h model{};ars_character_h character{};
    ars::core::RuntimeLease keepalive;ars::security::SourceAuthority *authority=nullptr;bool begun=false;uint64_t tick=0;
    explicit Fixture(const std::vector<uint8_t> &bytes){
        ars_runtime_desc d{};d.struct_size=sizeof d;d.abi_version=ARS_ABI_VERSION;d.alloc=Allocator::allocate;d.free_fn=Allocator::release;d.allocator_user_data=&allocator;
        REQUIRE(ars_runtime_create(&d,&runtime)==ARS_OK);
        ars_asset_source s{sizeof(s),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};REQUIRE(ars_model_load(runtime,&s,&model)==ARS_OK);
        character=create();REQUIRE(ars::core::global_router().acquire(runtime,ars::core::LeaseKind::work,&keepalive)==ARS_OK);
        authority=Probe::runtime(runtime)->authority;REQUIRE(authority);Probe::clock(*authority,Clock::read,&clock);
    }
    ars_character_h create(){ars_character_desc d{sizeof(d),ARS_ABI_VERSION,ARS_MODE_AUTO,0};ars_character_h c{};REQUIRE(ars_character_create(runtime,model,&d,&c)==ARS_OK);return c;}
    Binding bind(ars_character_h c,ars_utf8_view id={"mouth.open",10}){ars_source_desc d{};d.struct_size=sizeof d;d.abi_version=ARS_ABI_VERSION;d.allowed_channel_ids=&id;d.allowed_channel_count=1;d.max_channels_per_frame=1;d.max_frames_per_second=240;d.lease_ttl_ns=ars::security::max_ttl;d.diagnostic_label={"same-label",10};Binding b;REQUIRE(ars_source_register(runtime,c,&d,&b.admin,&b.cap)==ARS_OK);return b;}
    ars_status submit(ars_ingress_cap cap,Frame &f){return ars_motion_submit(runtime,cap,&f.frame);}
    void update(){REQUIRE(ars_runtime_update(runtime,++tick)==ARS_OK);}
    Pose pose(ars_character_h c){
        ars::core::SnapshotLease pin;REQUIRE(ars::core::global_router().read_cpu(runtime,c,&pin)==ARS_OK);
        const auto *snapshot=pin.get();Pose out{snapshot->generation,0,0,0};bool mouth=false,yaw=false;
        for(const auto &record:Probe::runtime(runtime)->cpu->characters)if(record.live && ars::security::same(record.id.id,c.id)){
            const auto &sem=record.model->semantics();REQUIRE(sem.size()==snapshot->channel_count);
            for(size_t i=0;i<sem.size();++i){if(sem[i].id=="mouth.open"){out.mouth=snapshot->outputs[i].scalar;mouth=true;}if(sem[i].id=="head.yaw_rad"){out.yaw=snapshot->outputs[i].scalar;yaw=true;}if(sem[i].id=="extra.quat")out.quat_w=snapshot->outputs[i].tail[2];}
        }REQUIRE(mouth && yaw);return out;
    }
    ~Fixture(){
        keepalive.reset();if(!begun){REQUIRE(ars_model_release(runtime,model)==ARS_OK);REQUIRE(ars_runtime_begin_close(runtime)==ARS_OK);}
        ars_status result=ARS_E_BUSY;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(result==ARS_E_BUSY && std::chrono::steady_clock::now()<end){result=ars_runtime_finish_close(runtime);if(result==ARS_E_BUSY)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        REQUIRE(result==ARS_OK);REQUIRE(allocator.live.empty());
    }
};
void cases(const std::string &id,const std::vector<uint8_t> &bytes){
    Fixture f(bytes);auto b=f.bind(f.character);auto &s=Probe::source(*f.authority,b.admin);
    auto &queue=Probe::queue(*f.authority,b.admin);
    if(id=="SRC-001" || id=="SRC-005" || id=="SRC-008"){
        Frame valid(10);REQUIRE(f.submit(b.cap,valid)==ARS_OK);f.update();const auto old=f.pose(f.character);REQUIRE(old.mouth==.75f);
        const auto credit=s.credit,ordinal=s.ordinal;
        if(id=="SRC-001"){Frame bad(11);bad.channel.value[0]=.1f;REQUIRE(f.submit({},bad)==ARS_E_SCOPE_DENIED);}
        if(id=="SRC-005"){Frame bad(11);bad.channel.semantic_id={"head.yaw_rad",12};REQUIRE(f.submit(b.cap,bad)==ARS_E_SCOPE_DENIED);}
        if(id=="SRC-008")for(auto seq:{uint64_t(10),uint64_t(9)}){Frame bad(seq);bad.channel.value[0]=.1f;REQUIRE(f.submit(b.cap,bad)==ARS_E_INVALID_FRAME);}
        REQUIRE(queue.size()==0 && s.sequence==10 && s.credit==credit && s.ordinal==ordinal);
        auto unchanged=f.pose(f.character);REQUIRE(unchanged.generation==old.generation && unchanged.mouth==old.mouth && unchanged.yaw==old.yaw);
        f.update();auto next=f.pose(f.character);REQUIRE(next.generation>old.generation && next.mouth==old.mouth && next.yaw==old.yaw);
        std::puts("TRACE rejected input leaves queue/sequence/quota and committed nonneutral pose unchanged; next generation retains authorized pose");
    }else if(id=="SRC-009" || id=="SRC-010"){
        Frame last(id=="SRC-009"?UINT64_MAX:1);REQUIRE(f.submit(b.cap,last)==ARS_OK);f.update();
        if(id=="SRC-009"){Frame wrap(0);REQUIRE(f.submit(b.cap,wrap)==ARS_E_INVALID_FRAME);wrap.frame.sequence=1;REQUIRE(f.submit(b.cap,wrap)==ARS_E_INVALID_FRAME);REQUIRE(s.sequence==UINT64_MAX && queue.size()==0);}
        const auto old_epoch=s.epoch;const auto label=s.label;ars_ingress_cap fresh{};REQUIRE(ars_source_rotate(f.runtime,b.admin,&fresh)==ARS_OK);
        REQUIRE(s.epoch>old_epoch && s.label==label && (fresh.lo!=b.cap.lo || fresh.hi!=b.cap.hi));
        Frame restart;restart.channel.value[0]=.25f;REQUIRE(f.submit(b.cap,restart)==ARS_E_SCOPE_DENIED);REQUIRE(f.submit(fresh,restart)==ARS_OK);f.update();REQUIRE(f.pose(f.character).mouth==.25f);
        std::puts("TRACE rotation advances epoch, preserves diagnostic label, rejects old cap and accepts new sequence1; sequence wrap never admitted");
    }else if(id=="SRC-011"){
        Frame old;REQUIRE(f.submit(b.cap,old)==ARS_OK);f.update();REQUIRE(ars_source_revoke(f.runtime,b.admin)==ARS_OK);
        auto next=f.bind(f.character);old.frame.sequence=100;old.frame.timestamp_ns=101;
        REQUIRE(f.submit(b.cap,old)==ARS_E_SCOPE_DENIED);REQUIRE(Probe::queue(*f.authority,next.admin).size()==0);
        f.update();REQUIRE(f.pose(f.character).mouth==0);Frame fresh;fresh.channel.value[0]=.25f;
        REQUIRE(f.submit(next.cap,fresh)==ARS_OK);f.update();REQUIRE(f.pose(f.character).mouth==.25f);
        std::puts("TRACE revoked token replay at plausible new timestamp denied after reconnect; neutral then fresh authorized pose commits");
    }else if(id=="SRC-013"){
        const auto credit=s.credit;
        for(float v:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){Frame bad;bad.channel.value[0]=v;REQUIRE(f.submit(b.cap,bad)==ARS_E_INVALID_FRAME);}
        const unsigned char utf8[]={0xc0,0xaf};Frame text;text.channel.semantic_id={reinterpret_cast<const char*>(utf8),2};REQUIRE(f.submit(b.cap,text)==ARS_E_INVALID_FRAME);
        REQUIRE(queue.size()==0 && s.sequence==0 && s.ordinal==0 && s.credit==credit);
        auto q=f.bind(f.character,{"extra.quat",10});Frame identity;identity.channel.semantic_id={"extra.quat",10};identity.channel.kind=ARS_VALUE_QUAT_XYZW;for(auto &v:identity.channel.value)v=0;identity.channel.value[3]=1;
        REQUIRE(f.submit(q.cap,identity)==ARS_OK);f.update();REQUIRE(f.pose(f.character).quat_w==1);
        auto &qs=Probe::source(*f.authority,q.admin);const auto qcredit=qs.credit;
        Frame zero(2);zero.channel.semantic_id={"extra.quat",10};zero.channel.kind=ARS_VALUE_QUAT_XYZW;for(auto &v:zero.channel.value)v=0;
        REQUIRE(f.submit(q.cap,zero)==ARS_E_INVALID_FRAME);REQUIRE(Probe::queue(*f.authority,q.admin).size()==0 && qs.sequence==1 && qs.credit==qcredit);
        identity.frame.sequence=2;REQUIRE(f.submit(q.cap,identity)==ARS_OK);
        Frame good;REQUIRE(f.submit(b.cap,good)==ARS_OK);f.update();REQUIRE(f.pose(f.character).mouth==.75f && f.pose(f.character).quat_w==1);
        std::puts("TRACE NaN/INF/zero quaternion/invalid UTF8 rejected before queue without sequence/quota charge; valid same sequence publishes");
    }else if(id=="SRC-016"){
        const auto old=f.character;REQUIRE(ars_character_destroy(f.runtime,old)==ARS_OK);f.character=f.create();auto next=f.bind(f.character);
        REQUIRE(old.id.generation!=f.character.id.generation);REQUIRE(next.cap.lo!=b.cap.lo || next.cap.hi!=b.cap.hi);
        Frame frame;REQUIRE(f.submit(b.cap,frame)==ARS_E_SCOPE_DENIED);f.update();REQUIRE(f.pose(f.character).mouth==0);
        REQUIRE(f.submit(next.cap,frame)==ARS_OK);f.update();REQUIRE(f.pose(f.character).mouth==.75f);
        std::puts("TRACE same diagnostic label/new character generation grants no inherited authority; old cap denied/new cap controls only new instance");
    }else if(id=="SRC-018"){
        REQUIRE(ars_character_set_mode(f.runtime,f.character,ARS_MODE_2D)==ARS_OK);
        REQUIRE(ars_character_set_mode(f.runtime,f.character,ARS_MODE_3D)==ARS_OK);
        Frame denied;denied.channel.semantic_id={"head.yaw_rad",12};REQUIRE(f.submit(b.cap,denied)==ARS_E_SCOPE_DENIED);
        denied.channel.semantic_id={"bone.head.yaw",13};REQUIRE(f.submit(b.cap,denied)==ARS_E_SCOPE_DENIED);
        Frame good;REQUIRE(f.submit(b.cap,good)==ARS_OK);f.update();auto pose=f.pose(f.character);REQUIRE(pose.mouth==.75f && pose.yaw==0);
        std::puts("TRACE 2D-to-3D mode retains semantic grant; unspecified head/native bone denied, mouth accepted");
    }else if(id=="SRC-021"){
        const auto issued=Probe::issued(*f.authority);
        for(const std::string name:{std::string("Mouth.Open"),std::string("mouth.\xc3\xb6pen")}){
            ars_utf8_view allowed{name.data(),static_cast<uint32_t>(name.size())};ars_source_desc d{};d.struct_size=sizeof d;d.abi_version=ARS_ABI_VERSION;d.allowed_channel_ids=&allowed;d.allowed_channel_count=1;d.max_channels_per_frame=1;d.max_frames_per_second=240;d.lease_ttl_ns=ars::security::max_ttl;
            ars_source_admin_h out{{9,9,9}};ars_ingress_cap cap{9,9};REQUIRE(ars_source_register(f.runtime,f.character,&d,&out,&cap)==ARS_E_INVALID_ARGUMENT);
            REQUIRE(!out.id.runtime_uid && !out.id.slot && !out.id.generation && !cap.lo && !cap.hi);
        }REQUIRE(Probe::issued(*f.authority)==issued);Frame good;REQUIRE(f.submit(b.cap,good)==ARS_OK);f.update();REQUIRE(f.pose(f.character).mouth==.75f);
        std::puts("TRACE mixed-case/Unicode grant aliases rejected; zero outputs/no issuance; canonical source remains valid");
    }else if(id=="SRC-027"){
        for(unsigned i=0;i<1000;++i){
            if(i)b=f.bind(f.character);auto &record=Probe::source(*f.authority,b.admin);auto &q=Probe::queue(*f.authority,b.admin);
            Frame base(99);REQUIRE(f.submit(b.cap,base)==ARS_OK);f.update();const auto credit=record.credit,ordinal=record.ordinal;
            std::barrier start(3);std::array<ars_status,2> result{};Frame first(100),second(100);first.channel.value[0]=.25f;second.channel.value[0]=.5f;
            std::thread a([&]{start.arrive_and_wait();result[0]=f.submit(b.cap,first);});std::thread c([&]{start.arrive_and_wait();result[1]=f.submit(b.cap,second);});start.arrive_and_wait();a.join();c.join();
            REQUIRE((result[0]==ARS_OK && result[1]==ARS_E_INVALID_FRAME)||(result[1]==ARS_OK && result[0]==ARS_E_INVALID_FRAME));
            REQUIRE(q.size()==1 && record.sequence==100 && record.ordinal==ordinal+1 && record.credit==credit-ars::security::credit_unit);
            f.update();REQUIRE(f.pose(f.character).mouth==(result[0]==ARS_OK?.25f:.5f));
            REQUIRE(ars_source_admin_release(f.runtime,b.admin)==ARS_OK);
            std::printf("TRACE iteration=%u duplicate100 results=%d,%d queue1 ordinal+1 quota_once\n",i,result[0],result[1]);
        }
    }else if(id=="SRC-028"){
        for(uint64_t i=86;i<=101;++i){Frame frame(i);REQUIRE(f.submit(b.cap,frame)==ARS_OK);}REQUIRE(queue.size()==16);
        const auto credit=s.credit;Frame retry(102);REQUIRE(f.submit(b.cap,retry)==ARS_E_QUEUE_FULL);REQUIRE(s.sequence==101 && s.credit==credit);
        f.update();REQUIRE(queue.size()==15);Probe::append_failure(*f.authority,b.admin,true);
        REQUIRE(f.submit(b.cap,retry)==ARS_E_OUT_OF_BUDGET);REQUIRE(s.sequence==101 && s.credit==credit && queue.size()==15);
        Probe::append_failure(*f.authority,b.admin,false);REQUIRE(f.submit(b.cap,retry)==ARS_OK);REQUIRE(s.sequence==102 && s.credit==credit-ars::security::credit_unit && queue.size()==16);
        std::puts("TRACE seq102 full then injected append OOM consumes no highwater/quota; restored capacity accepts same102 once");
    }else if(id=="SRC-029"){
        std::latch higher_done(1);ars_status high=ARS_E_INTERNAL,low=ARS_E_INTERNAL;Frame a(101),c(100);c.channel.value[0]=.25f;
        std::thread first([&]{high=f.submit(b.cap,a);higher_done.count_down();});std::thread second([&]{higher_done.wait();low=f.submit(b.cap,c);});first.join();second.join();
        REQUIRE(high==ARS_OK && low==ARS_E_INVALID_FRAME && s.sequence==101 && queue.size()==1);f.update();REQUIRE(f.pose(f.character).mouth==.75f);
        std::puts("TRACE deterministic thread101-before100; lower denied, single queue ordinal and committed winner preserved");
    }else if(id=="SRC-039" || id=="SRC-045"){
        const unsigned width=id=="SRC-045"?8:1;std::array<Binding,8> bindings{};bindings[0]=b;
        for(unsigned i=1;i<width;++i)bindings[i]=f.bind(f.character);
        Gate pause;f.clock.gate=&pause;std::barrier start(static_cast<ptrdiff_t>(width+1));std::array<ars_status,8> results{};std::vector<std::thread> workers;
        for(unsigned i=0;i<width;++i)workers.emplace_back([&,i]{Frame frame;start.arrive_and_wait();results[i]=f.submit(bindings[i].cap,frame);});
        start.arrive_and_wait();pause.wait();Probe::admitted(*f.authority,width);
        std::thread coordinator([&]{Probe::closing(*f.authority);Frame later(2);for(unsigned i=0;i<8;++i)REQUIRE(f.submit(b.cap,later)==ARS_E_CLOSED);pause.release();});
        REQUIRE(ars_runtime_begin_close(f.runtime)==ARS_OK);f.begun=true;coordinator.join();for(auto &thread:workers)thread.join();for(unsigned i=0;i<width;++i)REQUIRE(results[i]==ARS_OK);
        Frame late(3);REQUIRE(f.submit(b.cap,late)==ARS_E_CLOSED);REQUIRE(ars_runtime_update(f.runtime,1)==ARS_E_CLOSED);
        std::printf("TRACE admitted=%u at controlled clock cut; writer intent rejects8 later attempts; old readers drain; begin_close completes; no post-quiesce admission\n",width);
    }else REQUIRE(false);
}
}
int main(int argc,char **argv){
    if(argc!=3)return 2;const std::string id=argv[1];
    const std::array<std::string,15> ids{"SRC-001","SRC-005","SRC-008","SRC-009","SRC-010","SRC-011","SRC-013","SRC-016","SRC-018","SRC-021","SRC-027","SRC-028","SRC-029","SRC-039","SRC-045"};
    if(std::find(ids.begin(),ids.end(),id)==ids.end())return 2;
    std::ifstream input(argv[2],std::ios::binary);REQUIRE(input.good());std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};REQUIRE(!bytes.empty());
    cases(id,bytes);std::printf("SECURITY_CASE %s PASS seed=42 live=0\n",id.c_str());return 0;
}
