#pragma once
#include "ars/ars.h"
#include "runtime_lifecycle.hpp"
#include "../../src/security/source_authority.hpp"
#ifdef ARS_B3_NATIVE_TEST
#include "../../src/motion/publish_gate.hpp"
#endif
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <limits>

namespace ars::core {
// Defined only in test translation units, before any fixture handle/lease exists.
// There is no production counter override, reset entry point or test export.
struct ComponentFixture {
    static auto &stage(StageLease &lease) { return *lease.stage_; }
    static auto &publication(StageLease &lease) { return *lease.owner_; }
#ifdef ARS_B3_NATIVE_TEST
    static void snapshot_limit(motion::PublishGate &gate,std::uint32_t limit) { gate.snapshot_limit_=limit; }
    static void stage_limit(motion::PublishGate &gate,std::uint32_t limit) { gate.stage_limit_=limit; }
#endif
    static void pose_generation(security::SourceAuthority &authority,ars_character_h id,std::uint64_t generation) {
        for(auto &character:authority.characters_) if(security::same(character.id.id,id.id)) {
            std::lock_guard pose(character.pose); character.pose_generation=generation; return;
        }
        std::exit(26);
    }
    static void initial_uid(RuntimeRouter &router,std::uint64_t uid) {
        router.next_uid_=uid; router.uid_exhausted_=uid==0;
    }
    static void initial_generation(HandleRegistry &registry,std::uint32_t generation) {
        for(auto &slot:registry.slots_) {
            slot.generation=generation; slot.retired=generation==0;
        }
    }
    static security::SourceAuthority *authority(RuntimeRouter &router,ars_runtime_h handle,RuntimeLease *lease) {
        if(router.acquire(handle,LeaseKind::work,lease)!=ARS_OK) std::exit(20);
        return router.authority_for(*lease);
    }
    static void clock(security::SecurityClock &clock,ars_status(*read)(void *,std::uint64_t *) noexcept,void *user) {
        clock.read_=read; clock.user_=user; clock.init_=ARS_OK; clock.highwater_=0; clock.fault_=false;
    }
    static void clock(security::SourceAuthority &authority,ars_status(*read)(void *,std::uint64_t *) noexcept,void *user) {
        clock(authority.clock_,read,user);
    }
    static void entropy(security::SourceAuthority &authority,security::EntropyRead read,void *user) {
        authority.ledger_.entropy_=read; authority.ledger_.entropy_user_=user;
    }
    static void entropy(security::TokenLedger &ledger,security::EntropyRead read,void *user) {
        ledger.entropy_=read; ledger.entropy_user_=user;
    }
    static void chunks(security::OsEntropy &entropy,std::ptrdiff_t(*read)(void *,std::span<std::byte>,int *) noexcept,void *user) {
        entropy.read_=read; entropy.user_=user;
    }
    static auto &source(security::SourceAuthority &authority,ars_source_admin_h id) {
        for(auto &source:authority.sources_) if(security::same(source.id.id,id.id)) return source;
        std::exit(21);
    }
    static auto &queue(security::SourceAuthority &authority,ars_source_admin_h id) {
        auto &record=source(authority,id);
        return authority.queues_[record.data].queue;
    }
    static void append_failure(security::SourceAuthority &authority,ars_source_admin_h id,bool fail) {
        queue(authority,id).fail_append_=fail;
    }
    static auto &admission(security::SourceAuthority &authority) { return authority.admission_; }
    static void wait_admissions(security::SourceAuthority &authority,std::uint32_t count) {
        auto &gate=authority.admission_; std::unique_lock lock(gate.mutex_);
        gate.cv_.wait(lock,[&]{return gate.active_==count;});
    }
    static void wait_closing(security::SourceAuthority &authority) {
        auto &gate=authority.admission_; std::unique_lock lock(gate.mutex_);
        gate.cv_.wait(lock,[&]{return gate.closing_;});
    }
    static void ledger_limit(security::SourceAuthority &authority,std::uint32_t limit) { authority.ledger_.limit_=limit; }
    static std::uint32_t issued(security::SourceAuthority &authority) { return authority.ledger_.used_; }
    static void workspace_limit(security::SourceAuthority &authority,std::uint32_t limit) { authority.workspace_limit_=limit; }
    static void record_limit(security::SourceAuthority &authority,std::uint32_t limit) { authority.record_limit_=limit; }
    static void source_limit(security::SourceAuthority &authority,std::uint32_t limit) { authority.source_limit_=limit; }
    static void force_counter(security::SourceAuthority &authority,ars_source_admin_h id,unsigned counter) {
        auto &record=source(authority,id); std::lock_guard lock(record.mutex);
        auto maximum=std::numeric_limits<std::uint64_t>::max();
        if(counter==0) record.epoch=maximum;
        else if(counter==1) record.control=maximum;
        else if(counter==2) record.ticket=maximum;
        else record.ordinal=maximum;
    }
    static void supersede_ticket(security::SourceAuthority &authority,ars_source_admin_h id,bool active=false) {
        auto &record=source(authority,id); std::lock_guard lock(record.mutex); ++record.ticket;
        if(active) record.state=security::SourceState::active;
    }
};
}

namespace test {
inline void check(bool ok,const char *id) {
    std::printf("%s %s\n",ok?"PASS":"FAIL",id);
    if(!ok) std::exit(EXIT_FAILURE);
}
inline void status(ars_status expected,ars_status actual,const char *id) {
    std::printf("%s %s expected=%d actual=%d\n",expected==actual?"PASS":"FAIL",id,
                static_cast<int>(expected),static_cast<int>(actual));
    if(expected!=actual) std::exit(EXIT_FAILURE);
}
class Gate {
    std::mutex mutex_;
    std::condition_variable cv_;
    bool entered_=false,released_=false;
public:
    void arrive_and_wait() {
        std::unique_lock lock(mutex_); entered_=true; cv_.notify_all();
        cv_.wait(lock,[&]{return released_;});
    }
    void wait_entered() { std::unique_lock lock(mutex_); cv_.wait(lock,[&]{return entered_;}); }
    void release() { std::lock_guard lock(mutex_); released_=true; cv_.notify_all(); }
};
struct Clock {
    std::atomic<std::uint64_t> now{100};
    std::atomic<bool> fail{false};
    std::atomic<Gate *> gate{nullptr};
    static ars_status read(void *data,std::uint64_t *out) noexcept {
        auto &state=*static_cast<Clock *>(data);
        if(auto *pause=state.gate.exchange(nullptr)) pause->arrive_and_wait();
        if(state.fail.load()) return ARS_E_CLOCK_FAILURE;
        *out=state.now.load(); return ARS_OK;
    }
};
struct Entropy {
    std::atomic<std::uint64_t> next{1};
    std::atomic<bool> fail{false},constant{false},zero{false};
    std::atomic<unsigned> calls{0};
    std::atomic<Gate *> gate{nullptr};
    static ars_status read(void *data,std::span<std::byte> bytes) noexcept {
        auto &state=*static_cast<Entropy *>(data); ++state.calls;
        if(auto *pause=state.gate.exchange(nullptr)) pause->arrive_and_wait();
        if(state.fail.load()) return ARS_E_INTERNAL;
        ars_ingress_cap cap{state.constant.load()?state.next.load():state.next.fetch_add(1),0x4152534232ULL};
        if(state.zero.load()) cap={};
        std::memcpy(bytes.data(),&cap,bytes.size()); return ARS_OK;
    }
};
struct Frame {
    ars_motion_channel channel{{"mouth.open",10},ARS_VALUE_SCALAR,0,{0.5F,0,0,0}};
    ars_motion_frame value{};
    explicit Frame(std::uint64_t sequence=1) {
        value.struct_size=sizeof(value); value.abi_version=ARS_ABI_VERSION; value.sequence=sequence;
        value.timestamp_mode=ARS_TIMESTAMP_HOST_MONOTONIC; value.confidence=1; value.channel_count=1; value.channels=&channel;
    }
};
struct Authority {
    ars::core::RuntimeRouter router;
    ars_runtime_h runtime{};
    ars_character_h character{};
    ars::core::RuntimeLease retained;
    ars::security::SourceAuthority *context=nullptr;
    Clock time; Entropy random;
    explicit Authority(ars_simulation_now_fn callback=nullptr,void *user=nullptr,ars::core::RuntimeBudget budget={}):router(budget) {
        ars_runtime_desc desc{}; desc.struct_size=sizeof(desc); desc.abi_version=ARS_ABI_VERSION;
        desc.simulation_now_ns=callback; desc.clock_user_data=user;
        if(router.create(desc,&runtime)!=ARS_OK) std::exit(22);
        std::array<ars::security::SemanticChannel,2> catalog{};
        std::memcpy(catalog[0].id.data(),"mouth.open",11);
        std::memcpy(catalog[1].id.data(),"head.yaw_rad",13);
        if(router.bind_character(runtime,catalog,&character)!=ARS_OK) std::exit(23);
        context=ars::core::ComponentFixture::authority(router,runtime,&retained);
        ars::core::ComponentFixture::clock(*context,Clock::read,&time);
        ars::core::ComponentFixture::entropy(*context,Entropy::read,&random);
    }
    ~Authority() {
        retained.reset();
        auto status=router.begin_close(runtime);
        if(status!=ARS_OK && status!=ARS_E_CLOSED) std::exit(24);
        if(router.finish_close(runtime)!=ARS_OK) std::exit(25);
    }
    ars_status register_source(ars_source_admin_h *admin,ars_ingress_cap *cap,std::uint32_t rate=240,std::uint64_t ttl=ars::security::max_ttl) {
        ars_utf8_view allowed{"mouth.open",10};
        ars_source_desc desc{}; desc.struct_size=sizeof(desc); desc.abi_version=ARS_ABI_VERSION;
        desc.allowed_channel_ids=&allowed; desc.allowed_channel_count=1; desc.max_channels_per_frame=1;
        desc.max_frames_per_second=rate; desc.lease_ttl_ns=ttl;
        return router.register_source(runtime,character,&desc,admin,cap);
    }
};
#ifdef ARS_B3_NATIVE_TEST
inline void submit(Authority &a,ars_ingress_cap cap,std::uint64_t sequence,float value) {
    Frame frame(sequence); frame.channel.value[0]=value;
    status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"B3-owned-input");
}
inline ars::motion::Program blend_program() {
    using ars::motion::Op; ars::motion::Program program; program.count=8;
    program.nodes[0]={Op::input,0,0,0,0}; program.nodes[1]={Op::input,0,0,0,1};
    program.nodes[2]={Op::store,0,0,0}; program.nodes[3]={Op::store,1,0,1};
    program.nodes[4]={Op::add,0,1}; program.nodes[5]={Op::store,4,0,2};
    program.nodes[6]={Op::trigger,4,0,0,0,1,100}; program.nodes[7]={Op::output,4,0,0};
    return program;
}
inline ars::motion::Program retained_program() {
    using ars::motion::Op; ars::motion::Program program; program.count=5;
    program.nodes[0]={Op::retained,0,0,0}; program.nodes[1]={Op::retained,0,0,1};
    program.nodes[2]={Op::add,0,1}; program.nodes[3]={Op::output,2,0,0}; program.nodes[4]={Op::store,2,0,2};
    return program;
}
inline void publish(Authority &a,std::span<const ars_source_admin_h> sources,const ars::motion::Program &program) {
    ars::core::StageLease stage;
    status(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,sources,program,&stage),"B3-stage");
    status(ARS_OK,a.router.publish_cpu(&stage),"B3-publish");
}
#endif
}
