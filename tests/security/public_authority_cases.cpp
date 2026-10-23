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
    static void entropy(security::SourceAuthority &a,security::EntropyRead read,void *user){a.ledger_.entropy_=read;a.ledger_.entropy_user_=user;}
    static void ledger_limit(security::SourceAuthority &a,uint32_t limit){a.ledger_.limit_=limit;}
    static unsigned allocated(security::SourceAuthority &a){unsigned n=0;for(auto &s:a.sources_)n+=s.allocated?1:0;return n;}
    static uint32_t next_generation(ars_runtime_h h,uint32_t generation){auto &r=runtime(h)->registry;std::lock_guard lock(r.mutex_);for(uint32_t i=0;i<r.capacity_;++i){auto &s=r.slots_[i];if(!s.active && !s.retired && !s.pins){s.generation=generation;return i;}}std::exit(3);}
    static bool retired(ars_runtime_h h,uint32_t slot){auto &r=runtime(h)->registry;std::lock_guard lock(r.mutex_);return r.slots_[slot].retired;}
    static void admitted(security::SourceAuthority &a,unsigned n){auto &g=a.admission_;std::unique_lock lock(g.mutex_);REQUIRE(g.cv_.wait_for(lock,std::chrono::seconds(10),[&]{return g.active_==n;}));}
    static void closing(security::SourceAuthority &a){auto &g=a.admission_;std::unique_lock lock(g.mutex_);REQUIRE(g.cv_.wait_for(lock,std::chrono::seconds(10),[&]{return g.closing_;}));}
    static unsigned active(security::SourceAuthority &a){return a.admission_.active();}
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
    std::atomic<Gate*> final_gate{nullptr};ars::motion::PublicBatch *batch=nullptr;
    std::atomic<uint64_t> now{100};std::atomic<bool> fail{false},sample_before_pause{false};std::atomic<unsigned> final_samples{0};
    static ars_status read(void *p,uint64_t *out)noexcept{
        auto &c=*static_cast<Clock*>(p);if(auto *g=c.gate.exchange(nullptr))g->pause();
        // Armed only for a single S-thread update, with coordinator changing only atomics.
        if(c.final_gate.load() && c.batch && c.batch->count){if(auto *g=c.final_gate.exchange(nullptr)){
            ++c.final_samples;const auto before=c.now.load();g->pause();*out=c.sample_before_pause?before:c.now.load();return ARS_OK;}}
        if(c.fail)return ARS_E_CLOCK_FAILURE;*out=c.now;return ARS_OK;
    }
};
struct Entropy {
    bool fail=false;ars_ingress_cap retired{};bool collision_once=false;uint64_t serial=1;unsigned calls=0;Gate *gate=nullptr;
    static ars_status read(void *p,std::span<std::byte> bytes)noexcept{
        auto &e=*static_cast<Entropy*>(p);++e.calls;if(e.gate)e.gate->pause();if(e.fail)return ARS_E_INTERNAL;
        auto token=e.collision_once && e.calls==1?e.retired:ars_ingress_cap{e.serial++,UINT64_C(0x41343253334e4154)};
        REQUIRE(bytes.size()==sizeof token);std::memcpy(bytes.data(),&token,sizeof token);return ARS_OK;
    }
};
struct Simulation {
    uint64_t now=100;static uint64_t ARS_CALL read(void *p){return static_cast<Simulation*>(p)->now;}
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
struct Pose {uint64_t generation;float mouth,yaw,quat_w;uint32_t dependencies;};
struct Fixture {
    Allocator allocator;Clock clock;ars_runtime_h runtime{};ars_model_h model{};ars_character_h character{};
    ars::core::RuntimeLease keepalive;ars::security::SourceAuthority *authority=nullptr;bool begun=false;uint64_t tick=0;
    explicit Fixture(const std::vector<uint8_t> &bytes,ars_simulation_now_fn simulation=nullptr,void *clock_data=nullptr){
        ars_runtime_desc d{};d.struct_size=sizeof d;d.abi_version=ARS_ABI_VERSION;d.alloc=Allocator::allocate;d.free_fn=Allocator::release;d.allocator_user_data=&allocator;
        d.simulation_now_ns=simulation;d.clock_user_data=clock_data;
        REQUIRE(ars_runtime_create(&d,&runtime)==ARS_OK);
        ars_asset_source s{sizeof(s),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};REQUIRE(ars_model_load(runtime,&s,&model)==ARS_OK);
        character=create();REQUIRE(ars::core::global_router().acquire(runtime,ars::core::LeaseKind::work,&keepalive)==ARS_OK);
        authority=Probe::runtime(runtime)->authority;REQUIRE(authority);Probe::clock(*authority,Clock::read,&clock);clock.batch=&Probe::runtime(runtime)->cpu->batch;
    }
    ars_character_h create(){ars_character_desc d{sizeof(d),ARS_ABI_VERSION,ARS_MODE_AUTO,0};ars_character_h c{};REQUIRE(ars_character_create(runtime,model,&d,&c)==ARS_OK);return c;}
    Binding bind(ars_character_h c,ars_utf8_view id={"mouth.open",10},uint64_t ttl=ars::security::max_ttl,ars_status expected=ARS_OK,uint32_t rate=240){ars_source_desc d{};d.struct_size=sizeof d;d.abi_version=ARS_ABI_VERSION;d.allowed_channel_ids=&id;d.allowed_channel_count=1;d.max_channels_per_frame=1;d.max_frames_per_second=rate;d.lease_ttl_ns=ttl;d.diagnostic_label={"same-label",10};Binding b{{{9,9,9}},{9,9}};REQUIRE(ars_source_register(runtime,c,&d,&b.admin,&b.cap)==expected);return b;}
    ars_status submit(ars_ingress_cap cap,Frame &f){return ars_motion_submit(runtime,cap,&f.frame);}
    void update(){REQUIRE(ars_runtime_update(runtime,++tick)==ARS_OK);}
    Pose pose(ars_character_h c){
        ars::core::SnapshotLease pin;REQUIRE(ars::core::global_router().read_cpu(runtime,c,&pin)==ARS_OK);
        const auto *snapshot=pin.get();Pose out{snapshot->generation,0,0,0,snapshot->state.provenance.count};bool mouth=false,yaw=false;
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
void retirement_diagnostic(const std::string &id,const std::vector<uint8_t> &bytes){
    unsigned live=0;ars_status after=ARS_E_INTERNAL;
    {Fixture trial(bytes);const auto slot=Probe::next_generation(trial.runtime,UINT32_MAX);
        if(id=="DIAG-MODEL"){
            ars_asset_source source{sizeof(source),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};ars_model_h model{};
            REQUIRE(ars_model_load(trial.runtime,&source,&model)==ARS_OK);REQUIRE(model.id.slot==slot && model.id.generation==UINT32_MAX);
            REQUIRE(ars_model_release(trial.runtime,model)==ARS_E_HANDLE_EXHAUSTED);REQUIRE(ars_model_release(trial.runtime,model)==ARS_E_STALE_HANDLE);
            for(const auto &record:Probe::runtime(trial.runtime)->cpu->models)live+=record.live?1:0;
            after=ars_runtime_update(trial.runtime,1);std::printf("TRACE model terminal release: live_models=%u expected1; update=%d\n",live,after);
        }else{
            auto b=trial.bind(trial.character);REQUIRE(b.admin.id.slot==slot && b.admin.id.generation==UINT32_MAX);Frame frame;
            REQUIRE(trial.submit(b.cap,frame)==ARS_OK);trial.update();REQUIRE(ars_source_admin_release(trial.runtime,b.admin)==ARS_E_HANDLE_EXHAUSTED);
            REQUIRE(ars_source_admin_release(trial.runtime,b.admin)==ARS_E_STALE_HANDLE);REQUIRE(trial.submit(b.cap,frame)==ARS_E_SCOPE_DENIED);
            for(const auto &record:Probe::runtime(trial.runtime)->cpu->sources)live+=record.live?1:0;
            after=ars_runtime_update(trial.runtime,++trial.tick);REQUIRE(after==ARS_OK && trial.pose(trial.character).mouth==0);
            std::printf("TRACE admin terminal release: live_sources=%u expected0; neutral update=%d\n",live,after);
        }REQUIRE(Probe::retired(trial.runtime,slot));
    }
    REQUIRE(after==ARS_OK && live==(id=="DIAG-MODEL"?1u:0u));
}
void pressure_diagnostic(const std::vector<uint8_t> &bytes){
    Fixture f(bytes);std::array<Binding,16> bindings{};
    for(auto &b:bindings)b=f.bind(f.character,{"mouth.open",10},ars::security::max_ttl,ARS_OK,1);
    uint64_t accepted=0,limited=0;
    for(unsigned round=0;round<8;++round){
        f.clock.now=100+uint64_t(round)*ars::security::credit_unit;
        for(unsigned i=0;i<bindings.size();++i){
            auto &record=Probe::source(*f.authority,bindings[i].admin);auto &q=Probe::queue(*f.authority,bindings[i].admin);
            const auto before=record.ordinal;
            for(unsigned attempt=1;attempt<=100;++attempt){
                Frame frame(uint64_t(round)*100+attempt);const auto result=f.submit(bindings[i].cap,frame);
                REQUIRE(result==(attempt==1?ARS_OK:ARS_E_RATE_LIMIT));
                accepted+=result==ARS_OK;limited+=result==ARS_E_RATE_LIMIT;
                REQUIRE(q.size()==1 && record.ordinal==before+1 && record.sequence==uint64_t(round)*100+1 && record.credit==0);
            }
        }
        f.update();REQUIRE(f.pose(f.character).mouth==.75f);
        for(const auto &b:bindings)REQUIRE(Probe::queue(*f.authority,b.admin).size()==0);
    }
    REQUIRE(accepted==128 && limited==12672);
    std::printf("TRACE pressure rate1 16 sources 8 refill rounds 100x attempts: accepted=%llu rate_denied=%llu; exact per-source charge/sequence, queue1 then drain0; totals are harness-owned, runtime drop counters unavailable\n",static_cast<unsigned long long>(accepted),static_cast<unsigned long long>(limited));
    {Fixture full(bytes);std::array<Binding,16> sources{};unsigned queued=0,dropped=0;
        for(auto &b:sources)b=full.bind(full.character);
        for(const auto &b:sources){for(unsigned attempt=1;attempt<=100;++attempt){Frame frame(attempt);const auto result=full.submit(b.cap,frame);
            REQUIRE(result==(attempt<=16?ARS_OK:ARS_E_QUEUE_FULL));queued+=result==ARS_OK;dropped+=result==ARS_E_QUEUE_FULL;}
            auto &record=Probe::source(*full.authority,b.admin);REQUIRE(Probe::queue(*full.authority,b.admin).size()==16 && record.sequence==16 && record.ordinal==16 && record.credit==224*ars::security::credit_unit);
        }REQUIRE(queued==256 && dropped==1344);full.update();
        for(const auto &b:sources)REQUIRE(Probe::queue(*full.authority,b.admin).size()==15);
        std::puts("TRACE pressure queue saturation16x16: accepted256 queue_full1344; full denial preserves highwater/credit, update drains one each");
    }
}
void close_flood(const std::vector<uint8_t> &bytes){
    Fixture f(bytes);std::array<Binding,16> bindings{};for(auto &b:bindings)b=f.bind(f.character);
    // Queue storage is fixed and pinned by keepalive; terminal() invalidates source.data.
    std::array<ars::security::IngressQueue*,16> queues{};for(unsigned i=0;i<bindings.size();++i)queues[i]=&Probe::queue(*f.authority,bindings[i].admin);
    std::array<std::atomic<unsigned>,8> rounds{};std::array<uint64_t,8> accepted{},full{},closed{};
    std::barrier start(9);std::vector<std::thread> workers;Gate pause;
    for(unsigned i=0;i<8;++i)workers.emplace_back([&,i]{
        start.arrive_and_wait();uint64_t sequence=0;
        for(;;){Frame frame(++sequence);const auto result=f.submit(bindings[2*i+(sequence%2)].cap,frame);
            if(result==ARS_OK)++accepted[i];else if(result==ARS_E_QUEUE_FULL)++full[i];else if(result==ARS_E_CLOSED){++closed[i];break;}else REQUIRE(false);
            rounds[i].fetch_add(1);
        }
    });
    start.arrive_and_wait();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(!std::all_of(rounds.begin(),rounds.end(),[](const auto &n){return n.load()>=100;})){
        REQUIRE(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();
    }
    // All16 queues saturated under ongoing calls; park8 actual admitted readers.
    f.clock.gate=&pause;pause.wait();Probe::admitted(*f.authority,8);
    std::thread coordinator([&]{Probe::closing(*f.authority);
        for(unsigned round=0;round<100;++round)for(const auto &b:bindings){Frame frame(UINT64_C(1000000000)+round);REQUIRE(f.submit(b.cap,frame)==ARS_E_CLOSED);}
        REQUIRE(Probe::active(*f.authority)==8);pause.release();
    });
    const auto begin=std::chrono::steady_clock::now();REQUIRE(ars_runtime_begin_close(f.runtime)==ARS_OK);f.begun=true;
    coordinator.join();for(auto &worker:workers)worker.join();
    REQUIRE(std::chrono::steady_clock::now()-begin<std::chrono::seconds(10) && Probe::active(*f.authority)==0);
    uint64_t total=0,denied=0;for(unsigned i=0;i<8;++i){REQUIRE(accepted[i]==32 && full[i]>=68 && closed[i]==1);total+=accepted[i];denied+=full[i];}
    for(unsigned i=0;i<bindings.size();++i){const auto &b=bindings[i];auto &record=Probe::source(*f.authority,b.admin);REQUIRE(record.state==ars::security::SourceState::closed && record.data==ars::security::max_sources && queues[i]->size()==0);Frame late;REQUIRE(f.submit(b.cap,late)==ARS_E_CLOSED);}
    REQUIRE(total==256 && denied>=544 && ars_runtime_update(f.runtime,1)==ARS_E_CLOSED);
    std::printf("TRACE sustained flood 8 workers/16 sources: accepted=%llu queue_full=%llu; 8 real readers admitted at writer intent, 1600 later attempts CLOSED, all8 readers drain, queues0/admissions0, begin_close completes under10sec with workers still issuing until CLOSED\n",static_cast<unsigned long long>(total),static_cast<unsigned long long>(denied));
}
void cases(const std::string &id,const std::vector<uint8_t> &bytes){
    if(id=="SRC-041"){close_flood(bytes);return;}
    if(id=="HDL-005"){
        ars_status after=ARS_E_INTERNAL;unsigned live=0;
        {Fixture trial(bytes);auto source=trial.bind(trial.character);Frame frame;REQUIRE(trial.submit(source.cap,frame)==ARS_OK);trial.update();
            const auto slot=Probe::next_generation(trial.runtime,UINT32_MAX);auto terminal=trial.create();REQUIRE(terminal.id.slot==slot && terminal.id.generation==UINT32_MAX);
            trial.update();REQUIRE(trial.pose(trial.character).mouth==.75f);
            REQUIRE(ars_character_destroy(trial.runtime,terminal)==ARS_E_HANDLE_EXHAUSTED);REQUIRE(Probe::retired(trial.runtime,slot));
            REQUIRE(ars_character_destroy(trial.runtime,terminal)==ARS_E_STALE_HANDLE);REQUIRE(ars_character_set_mode(trial.runtime,terminal,ARS_MODE_2D)==ARS_E_STALE_HANDLE);
            auto next=trial.create();REQUIRE(next.id.slot!=slot);
            for(const auto &c:Probe::runtime(trial.runtime)->cpu->characters)live+=c.live?1:0;
            after=ars_runtime_update(trial.runtime,++trial.tick);
            std::printf("TRACE max-generation destroy retires slot%u, stale old handle denied, newslot=%u; live_cpu=%u expected2; update_after=%d expected0\n",slot,next.id.slot,live,after);
        } // Actual final close/allocator-zero is checked before reporting a regression.
        REQUIRE(live==2 && after==ARS_OK);return;
    }
    const bool short_ttl=id=="SRC-012" || id=="SRC-025" || id=="SRC-036" || id=="SRC-037" || id=="SRC-038" || id=="SRC-042";
    Fixture f(bytes);auto b=f.bind(f.character,{"mouth.open",10},short_ttl?100:ars::security::max_ttl);auto &s=Probe::source(*f.authority,b.admin);
    auto &queue=Probe::queue(*f.authority,b.admin);
    if(id=="SRC-004"){
        Entropy error;error.fail=true;Probe::entropy(*f.authority,Entropy::read,&error);
        const auto allocated=Probe::allocated(*f.authority),issued=Probe::issued(*f.authority);
        auto failed=f.bind(f.character,{"mouth.open",10},ars::security::max_ttl,ARS_E_INTERNAL);
        REQUIRE(!failed.admin.id.runtime_uid && !failed.admin.id.slot && !failed.admin.id.generation && !failed.cap.lo && !failed.cap.hi);
        REQUIRE(Probe::allocated(*f.authority)==allocated && Probe::issued(*f.authority)==issued && error.calls==1);
        Frame old;REQUIRE(f.submit(b.cap,old)==ARS_OK);f.update();REQUIRE(f.pose(f.character).mouth==.75f);
        ars_ingress_cap replacement{9,9};REQUIRE(ars_source_rotate(f.runtime,b.admin,&replacement)==ARS_E_INTERNAL);
        REQUIRE(!replacement.lo && !replacement.hi && error.calls==2);REQUIRE(f.submit(b.cap,old)==ARS_E_SCOPE_DENIED);f.update();REQUIRE(f.pose(f.character).mouth==0);
        std::puts("TRACE injected RNG register/rotate failure: typed INTERNAL, zero outputs/no source activation/no weak fallback; old authority denied and neutral fallback");
    }else if(id=="SRC-012"){
        for(unsigned i=1;i<=3;++i){Frame frame(i);REQUIRE(f.submit(b.cap,frame)==ARS_OK);}f.update();REQUIRE(queue.size()==2 && f.pose(f.character).mouth==.75f);
        f.clock.now=s.expiry;f.update();auto pose=f.pose(f.character);REQUIRE(pose.mouth==0 && pose.dependencies==0 && queue.size()==0);
        Frame late(4);REQUIRE(f.submit(b.cap,late)==ARS_E_SCOPE_DENIED);REQUIRE(s.state==ars::security::SourceState::expired);
        std::puts("TRACE TTL expires with two queued frames; queue cleared, terminal expired, next committed pose neutral with zero provenance");
    }else if(id=="SRC-020"){
        Frame frame;REQUIRE(f.submit(b.cap,frame)==ARS_OK);f.update();const auto before=f.pose(f.character);f.clock.fail=true;frame.frame.sequence=2;
        REQUIRE(f.submit(b.cap,frame)==ARS_E_CLOCK_FAILURE);REQUIRE(ars_runtime_update(f.runtime,2)==ARS_E_SECURITY_CLOCK_FAULT);f.clock.fail=false;
        REQUIRE(f.submit(b.cap,frame)==ARS_E_SECURITY_CLOCK_FAULT);REQUIRE(f.pose(f.character).generation==before.generation);
        {Fixture other(bytes);auto source=other.bind(other.character);other.clock.now=99;Frame next;
         REQUIRE(other.submit(source.cap,next)==ARS_E_CLOCK_REGRESSION);other.clock.now=101;REQUIRE(other.submit(source.cap,next)==ARS_E_SECURITY_CLOCK_FAULT);}
        {Simulation time;Fixture sim(bytes,Simulation::read,&time);auto source=sim.bind(sim.character);Frame input;REQUIRE(sim.submit(source.cap,input)==ARS_OK);
         REQUIRE(ars_runtime_update(sim.runtime,0)==ARS_OK);auto accepted=sim.pose(sim.character);const auto expiry=Probe::source(*sim.authority,source.admin).expiry;
         time.now=99;REQUIRE(ars_runtime_update(sim.runtime,0)==ARS_E_CLOCK_REGRESSION);REQUIRE(sim.pose(sim.character).generation==accepted.generation && Probe::source(*sim.authority,source.admin).expiry==expiry);}
        std::puts("TRACE security unavailable/regression is sticky and blocks new publication; simulation callback regression rejects update without pose/lease renewal");
    }else if(id=="SRC-022"){
        REQUIRE(ars_source_revoke(f.runtime,b.admin)==ARS_OK);Entropy entropy;entropy.retired=b.cap;entropy.collision_once=true;Probe::entropy(*f.authority,Entropy::read,&entropy);
        const auto issued=Probe::issued(*f.authority);auto fresh=f.bind(f.character);REQUIRE(entropy.calls==2 && Probe::issued(*f.authority)==issued+1);
        REQUIRE(fresh.cap.lo!=b.cap.lo || fresh.cap.hi!=b.cap.hi);Frame frame;REQUIRE(f.submit(b.cap,frame)==ARS_E_SCOPE_DENIED);REQUIRE(f.submit(fresh.cap,frame)==ARS_OK);f.update();
        Probe::ledger_limit(*f.authority,Probe::issued(*f.authority));ars_ingress_cap out{9,9};REQUIRE(ars_source_rotate(f.runtime,fresh.admin,&out)==ARS_E_HANDLE_EXHAUSTED);
        REQUIRE(!out.lo && !out.hi && entropy.calls==2);REQUIRE(f.submit(fresh.cap,frame)==ARS_E_SCOPE_DENIED);REQUIRE(f.submit(b.cap,frame)==ARS_E_SCOPE_DENIED);
        std::puts("TRACE retired-token collision retried to fresh token; injected issuance limit fails closed and never resurrects either retired token");
    }else if(id=="SRC-025" || id=="SRC-036" || id=="SRC-037"){
        Frame first;REQUIRE(f.submit(b.cap,first)==ARS_OK);f.update();const auto before=f.pose(f.character);
        f.clock.now=199;Frame next(2);next.channel.value[0]=.25f;REQUIRE(f.submit(b.cap,next)==ARS_OK);
        Gate cut;f.clock.final_gate=&cut;f.clock.sample_before_pause=id=="SRC-037";
        std::thread coordinator([&]{cut.wait();f.clock.now=200;cut.release();});
        const auto result=ars_runtime_update(f.runtime,++f.tick);coordinator.join();REQUIRE(f.clock.final_samples==1);
        auto after=f.pose(f.character);
        if(id=="SRC-037"){REQUIRE(result==ARS_OK && after.generation==before.generation+1 && after.mouth==.25f);}
        else REQUIRE(result==ARS_E_SOURCE_EXPIRED && after.generation==before.generation && after.mouth==before.mouth);
        f.update();after=f.pose(f.character);REQUIRE(after.mouth==0 && after.dependencies==0 && s.state==ars::security::SourceState::expired);
        std::printf("TRACE final-public-batch security sample cut; sample_before_pause=%d result=%d; next sample expires source and commits neutral/no lineage\n",id=="SRC-037",result);
    }else if(id=="SRC-026"){
        for(unsigned mode=0;mode<2;++mode){Fixture trial(bytes);auto source=trial.bind(trial.character);Frame frame;REQUIRE(trial.submit(source.cap,frame)==ARS_OK);trial.update();
            Entropy failure;failure.fail=true;if(mode==0)Probe::entropy(*trial.authority,Entropy::read,&failure);else Probe::ledger_limit(*trial.authority,Probe::issued(*trial.authority));
            ars_ingress_cap out{9,9};REQUIRE(ars_source_rotate(trial.runtime,source.admin,&out)==(mode==0?ARS_E_INTERNAL:ARS_E_HANDLE_EXHAUSTED));
            REQUIRE(!out.lo && !out.hi);REQUIRE(trial.submit(source.cap,frame)==ARS_E_SCOPE_DENIED);trial.update();REQUIRE(trial.pose(trial.character).mouth==0);
        }std::puts("TRACE rotation RNG/issuance exhaustion after old-token retirement: zero cap, old token permanently denied, no rollback, neutral fresh pose");
    }else if(id=="SRC-035" || id=="SRC-040"){
        Frame first;REQUIRE(f.submit(b.cap,first)==ARS_OK);f.update();const auto before=f.pose(f.character);const auto credit=s.credit;
        if(id=="SRC-035"){f.clock.now=99;Frame next(2);REQUIRE(f.submit(b.cap,next)==ARS_E_CLOCK_REGRESSION);REQUIRE(s.credit==credit && s.sequence==1 && queue.size()==0);}
        else{f.clock.now=180;Frame next(2);REQUIRE(f.submit(b.cap,next)==ARS_OK);f.clock.now=170;REQUIRE(ars_runtime_update(f.runtime,2)==ARS_E_CLOCK_REGRESSION);}
        f.clock.now=200;Frame retry(3);REQUIRE(f.submit(b.cap,retry)==ARS_E_SECURITY_CLOCK_FAULT);REQUIRE(ars_runtime_update(f.runtime,3)==ARS_E_SECURITY_CLOCK_FAULT);REQUIRE(f.pose(f.character).generation==before.generation);
        std::puts("TRACE backwards ingress/publisher security sample faults global clock permanently, no unauthorized quota/sequence/publish advance");
    }else if(id=="SRC-038"){
        f.clock.now=150;Frame accepted;REQUIRE(f.submit(b.cap,accepted)==ARS_OK);f.update();const auto before=f.pose(f.character);const auto epoch=s.epoch;
        f.clock.now=200;Frame late(2);REQUIRE(f.submit(b.cap,late)==ARS_E_SOURCE_EXPIRED);REQUIRE(s.state==ars::security::SourceState::expired && s.epoch>epoch);
        f.clock.now=120;REQUIRE(ars_runtime_update(f.runtime,2)==ARS_E_CLOCK_REGRESSION);f.clock.now=201;REQUIRE(ars_runtime_update(f.runtime,3)==ARS_E_SECURITY_CLOCK_FAULT);
        REQUIRE(f.submit(b.cap,late)==ARS_E_SCOPE_DENIED);REQUIRE(f.pose(f.character).generation==before.generation);
        std::puts("TRACE accepted150 expires200 then regresses120: terminal EXPIRED/epoch invalidation, sticky clock fault, no old-token renewal or fresh pose");
    }else if(id=="SRC-042"){
        Frame accepted;REQUIRE(f.submit(b.cap,accepted)==ARS_OK);f.update();Entropy entropy;Gate minted;entropy.gate=&minted;Probe::entropy(*f.authority,Entropy::read,&entropy);
        std::thread coordinator([&]{minted.wait();f.clock.now=200;minted.release();});ars_ingress_cap out{9,9};const auto result=ars_source_rotate(f.runtime,b.admin,&out);coordinator.join();
        REQUIRE(result==ARS_E_SOURCE_EXPIRED && !out.lo && !out.hi && s.state==ars::security::SourceState::expired);
        REQUIRE(ars_source_rotate(f.runtime,b.admin,&out)==ARS_E_SOURCE_EXPIRED);REQUIRE(entropy.calls==1);REQUIRE(f.submit(b.cap,accepted)==ARS_E_SCOPE_DENIED);f.update();REQUIRE(f.pose(f.character).mouth==0);
        std::puts("TRACE rotation paused at mint phase B, TTL passes; phase C terminally EXPIRED, zero output, subsequent rotate denied, no lease renewal");
    }else if(id=="SRC-044"){
        for(unsigned mode=0;mode<3;++mode){Fixture trial(bytes);auto source=trial.bind(trial.character,{"mouth.open",10},100);auto &record=Probe::source(*trial.authority,source.admin);Frame frame;REQUIRE(trial.submit(source.cap,frame)==ARS_OK);trial.update();
            ars_ingress_cap out{9,9};ars_status result;
            if(mode==0){record.epoch=UINT64_MAX;result=ars_source_revoke(trial.runtime,source.admin);REQUIRE(record.epoch==UINT64_MAX);}
            else if(mode==1){record.control=UINT64_MAX;trial.clock.now=200;frame.frame.sequence=2;result=trial.submit(source.cap,frame);REQUIRE(record.control==UINT64_MAX);}
            else{record.ticket=UINT64_MAX;result=ars_source_rotate(trial.runtime,source.admin,&out);REQUIRE(record.ticket==UINT64_MAX && !out.lo && !out.hi);}
            REQUIRE(result==ARS_E_HANDLE_EXHAUSTED && record.state==ars::security::SourceState::exhausted);REQUIRE(trial.submit(source.cap,frame)==ARS_E_SCOPE_DENIED);trial.update();REQUIRE(trial.pose(trial.character).mouth==0);
        }std::puts("TRACE forced epoch/control/ticket UINT64_MAX: terminal EXHAUSTED, no wrap/token ABA, no assertion crash, fresh neutral pose");
    }else if(id=="SRC-001" || id=="SRC-005" || id=="SRC-008"){
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
    if(id=="DIAG-PRESSURE"){
        std::ifstream input(argv[2],std::ios::binary);REQUIRE(input.good());std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};REQUIRE(!bytes.empty());
        pressure_diagnostic(bytes);std::puts("PRESSURE_DIAGNOSTIC PASS live=0 SRC014_COUNTER_ORACLE_UNVERIFIED");return 0;
    }
    if(id=="DIAG-MODEL" || id=="DIAG-ADMIN"){
        std::ifstream input(argv[2],std::ios::binary);REQUIRE(input.good());std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};REQUIRE(!bytes.empty());
        retirement_diagnostic(id,bytes);std::printf("RETIREMENT_DIAGNOSTIC %s PASS live=0\n",id.c_str());return 0;
    }
    const std::array<std::string,30> ids{"SRC-001","SRC-005","SRC-008","SRC-009","SRC-010","SRC-011","SRC-013","SRC-016","SRC-018","SRC-021","SRC-027","SRC-028","SRC-029","SRC-039","SRC-045","SRC-004","SRC-012","SRC-020","SRC-022","SRC-025","SRC-026","SRC-035","SRC-036","SRC-037","SRC-038","SRC-040","SRC-042","SRC-044","HDL-005","SRC-041"};
    if(std::find(ids.begin(),ids.end(),id)==ids.end())return 2;
    std::ifstream input(argv[2],std::ios::binary);REQUIRE(input.good());std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};REQUIRE(!bytes.empty());
    cases(id,bytes);std::printf("SECURITY_CASE %s PASS seed=42 live=0\n",id.c_str());return 0;
}
