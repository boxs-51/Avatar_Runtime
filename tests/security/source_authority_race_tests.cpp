#include "runtime_test_support.hpp"
#include <barrier>
#include <latch>
#include <thread>
using ars::core::ComponentFixture;
namespace {
constexpr std::uint64_t seed=0x4152534232330001ULL;
constexpr unsigned iterations=1000;
std::uint64_t schedule=seed;
bool order() { schedule^=schedule<<13; schedule^=schedule>>7; schedule^=schedule<<17; return (schedule&1)!=0; }
void require(bool result,const char *family,unsigned iteration) {
    if(!result) { std::printf("FAIL family=%s iteration=%u seed=%llx\n",family,iteration,static_cast<unsigned long long>(seed)); std::exit(1); }
}
void expect(ars_status expected,ars_status actual,const char *family,unsigned iteration) {
    if(expected!=actual) {
        std::printf("FAIL %s iteration=%u seed=%llx expected=%d actual=%d\n",family,iteration,static_cast<unsigned long long>(seed),expected,actual); std::exit(1);
    }
}
void registered(test::Authority &a,ars_source_admin_h &source,ars_ingress_cap &cap,unsigned i,std::uint64_t ttl=ars::security::max_ttl) {
    expect(ARS_OK,a.register_source(&source,&cap,240,ttl),"register",i);
}
void release(test::Authority &a,ars_source_admin_h source,unsigned i) { expect(ARS_OK,a.router.release_source(a.runtime,source),"release",i); }
void pop(test::Authority &a,ars_source_admin_h source,std::uint64_t sequence,unsigned i) {
    ars::core::FrameLease frame; expect(ARS_OK,a.router.dequeue_frame(a.runtime,source,&frame),"owned-queue-pop",i);
    require(frame.frame.sequence==sequence,"queue-sequence-order",i);
}
struct Capture {
    std::atomic<test::Gate *> gate{nullptr};
    static std::uint64_t ARS_CALL read(void *data) {
        if(auto *pause=static_cast<Capture *>(data)->gate.exchange(nullptr)) pause->arrive_and_wait();
        return 1234;
    }
};
void duplicate_queue() {
    test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{}; registered(a,source,cap,0);
    for(unsigned i=0;i<iterations;++i) {
        a.time.now=100+static_cast<std::uint64_t>(i)*1000000000ULL;
        const auto sequence=static_cast<std::uint64_t>(i)*3+1;
        test::Frame frame(sequence); std::barrier start(3); std::array<ars_status,2> result{}; const bool reverse=order();
        std::thread first([&]{start.arrive_and_wait(); result[reverse?1:0]=a.router.submit_frame(a.runtime,cap,&frame.value);});
        std::thread second([&]{start.arrive_and_wait(); result[reverse?0:1]=a.router.submit_frame(a.runtime,cap,&frame.value);});
        start.arrive_and_wait(); first.join(); second.join();
        require((result[0]==ARS_OK && result[1]==ARS_E_INVALID_FRAME)||(result[1]==ARS_OK && result[0]==ARS_E_INVALID_FRAME),"RACE-G-exactly-one-duplicate",i);
        auto &record=ComponentFixture::source(*a.context,source);
        require(record.credit==239*ars::security::credit_unit,"RACE-G-quota-once",i);
        test::Frame high(sequence+2),low(sequence+1); std::latch done(1);
        std::thread later([&]{done.wait(); result[0]=a.router.submit_frame(a.runtime,cap,&low.value);});
        std::thread higher([&]{result[1]=a.router.submit_frame(a.runtime,cap,&high.value); done.count_down();});
        higher.join(); later.join(); expect(ARS_OK,result[1],"SEQ-higher-wins",i); expect(ARS_E_INVALID_FRAME,result[0],"SEQ-no-reorder",i);
        pop(a,source,sequence,i); pop(a,source,sequence+2,i);
        test::Frame retry(sequence+3); const auto credit=record.credit;
        ComponentFixture::append_failure(*a.context,source,true);
        expect(ARS_E_OUT_OF_BUDGET,a.router.submit_frame(a.runtime,cap,&retry.value),"QUEUE-OOM-no-consume",i);
        require(record.sequence==sequence+2 && record.credit==credit,"QUEUE-transaction-no-charge",i);
        ComponentFixture::append_failure(*a.context,source,false); // Next iteration retries this sequence.
    }
    std::puts("PASS RACE-G/SEQ-order/QUEUE-rollback: 1000 seeded controlled iterations");
}
void rollover() {
    test::Authority a;
    for(unsigned i=0;i<iterations;++i) {
        ars_source_admin_h source{}; ars_ingress_cap cap{},next{}; registered(a,source,cap,i); test::Frame frame(UINT64_MAX);
        expect(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"RACE-E-last-sequence",i);
        frame.value.sequence=1; expect(ARS_E_INVALID_FRAME,a.router.submit_frame(a.runtime,cap,&frame.value),"RACE-E-no-wrap",i);
        expect(ARS_OK,a.router.rotate_source(a.runtime,source,&next),"RACE-E-rotate",i);
        expect(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"RACE-E-old-cap-denied",i);
        expect(ARS_OK,a.router.submit_frame(a.runtime,next,&frame.value),"RACE-E-new-sequence",i); release(a,source,i);
    }
    std::puts("PASS RACE-E: 1000 rollover/rotation/nonreuse iterations");
}
void revoke_during_copy() {
    Capture capture; test::Authority a(Capture::read,&capture);
    for(unsigned i=0;i<iterations;++i) {
        ars_source_admin_h source{}; ars_ingress_cap cap{}; registered(a,source,cap,i);
        test::Gate copy; capture.gate=&copy; test::Frame frame; frame.value.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL; ars_status result=ARS_OK;
        std::thread ingress([&]{result=a.router.submit_frame(a.runtime,cap,&frame.value);}); copy.wait_entered();
        expect(ARS_OK,a.router.revoke_source(a.runtime,source),"RACE-A/H-revoke-during-unlocked-copy",i);
        copy.release(); ingress.join(); expect(ARS_E_SCOPE_DENIED,result,"RACE-A/H-no-append-after-revoke",i);
        require(ComponentFixture::source(*a.context,source).sequence==0,"RACE-A/H-no-charge",i); release(a,source,i);
    }
    std::puts("PASS RACE-A/H authority cut: 1000 copy-vs-revoke iterations; pose oracle remains HOLD");
}
void rotation_terminal() {
    test::Authority a;
    for(unsigned i=0;i<iterations;++i) {
        ars_source_admin_h source{}; ars_ingress_cap cap{},next{}; registered(a,source,cap,i);
        test::Gate minted; a.random.gate=&minted; a.random.fail=(i%3==2); ars_status result=ARS_OK;
        std::thread rotation([&]{result=a.router.rotate_source(a.runtime,source,&next);}); minted.wait_entered();
        if(i%3==1) expect(ARS_OK,a.router.release_source(a.runtime,source),"RACE-K/L-admin-release-before-C",i);
        else expect(ARS_OK,a.router.revoke_source(a.runtime,source),"RACE-K/L-explicit-revoke-before-C",i);
        minted.release(); rotation.join(); a.random.fail=false;
        expect(ARS_E_REVOKED,result,"RACE-K/L-terminal-beats-mint-or-failure",i); require(!ars::security::nonzero(next),"RACE-K/L-zero-output",i);
        test::Frame frame; expect(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"RACE-K/L-old-cap-denied",i);
        if(i%3!=1) release(a,source,i);
    }
    std::puts("PASS RACE-K/L: 1000 unlocked-mint/revoke/admin-release/failure cuts");
}
void rotation_expiry() {
    test::Authority a;
    for(unsigned i=0;i<iterations;++i) {
        const auto now=100+static_cast<std::uint64_t>(i)*1000; a.time.now=now;
        ars_source_admin_h source{}; ars_ingress_cap cap{},next{}; registered(a,source,cap,i,100);
        test::Gate minted; a.random.gate=&minted; ars_status result=ARS_OK;
        std::thread rotation([&]{result=a.router.rotate_source(a.runtime,source,&next);}); minted.wait_entered();
        a.time.now=now+100; minted.release(); rotation.join();
        expect(ARS_E_SOURCE_EXPIRED,result,"RACE-U-final-expiry-sample",i); require(!ars::security::nonzero(next),"RACE-U-discard-token",i);
        expect(ARS_E_SOURCE_EXPIRED,a.router.rotate_source(a.runtime,source,&next),"RACE-U-no-recovery",i); release(a,source,i);
    }
    std::puts("PASS RACE-U: 1000 phase-B-expiry cuts");
}
void clock_poison() {
    for(unsigned i=0;i<iterations;++i) {
        test::Authority a; ars_source_admin_h source{},other{}; ars_ingress_cap cap{},another{},out{};
        registered(a,source,cap,i); registered(a,other,another,i); test::Frame frame; a.time.now=180;
        expect(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"RACE-N/S-first-global-clock",i); a.time.now=170;
        expect(ARS_E_CLOCK_REGRESSION,a.router.submit_frame(a.runtime,another,&frame.value),"RACE-N/S-cross-source-regression",i); a.time.now=190;
        expect(ARS_E_SECURITY_CLOCK_FAULT,a.router.rotate_source(a.runtime,source,&out),"RACE-Q/S-sticky-poison",i);
        require(ComponentFixture::source(*a.context,other).sequence==0,"RACE-N/Q/S-no-charge",i);
    }
    std::puts("PASS RACE-N/Q/S authority cut: 1000 global-clock poison iterations; publish remains HOLD");
}
void exhaustion() {
    test::Authority a;
    for(unsigned i=0;i<iterations;++i) {
        ars_source_admin_h source{}; ars_ingress_cap cap{},out{}; registered(a,source,cap,i);
        ComponentFixture::force_counter(*a.context,source,i%4); test::Frame frame;
        auto result=i%4==3?a.router.submit_frame(a.runtime,cap,&frame.value):a.router.rotate_source(a.runtime,source,&out);
        expect(ARS_E_HANDLE_EXHAUSTED,result,"RACE-W-terminal-overflow",i);
        expect(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"RACE-W-no-ABA",i);
        require(ComponentFixture::source(*a.context,source).tombstone.dirty.load(std::memory_order_acquire),"RACE-W-release-tombstone",i); release(a,source,i);
    }
    std::puts("PASS RACE-W: 1000 checked epoch/control/ticket/ordinal iterations");
}
void close_during_rotation() {
    for(unsigned i=0;i<iterations;++i) {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{},out{}; registered(a,source,cap,i);
        test::Gate minted; a.random.gate=&minted; ars_status result=ARS_OK;
        std::thread rotation([&]{result=a.router.rotate_source(a.runtime,source,&out);}); minted.wait_entered();
        expect(ARS_OK,a.router.begin_close(a.runtime),"RACE-M-close-before-C",i);
        expect(ARS_E_BUSY,a.router.finish_close(a.runtime),"RACE-M-keep-pinned-storage",i);
        minted.release(); rotation.join(); expect(ARS_E_CLOSED,result,"RACE-M-no-activation-after-close",i); require(!ars::security::nonzero(out),"RACE-M-zero-output",i);
    }
    std::puts("PASS RACE-M authority cut: 1000 close-vs-rotation iterations; pose publication remains HOLD");
}
void closing_flood() {
    for(unsigned i=0;i<iterations;++i) {
        test::Authority a; std::array<ars_source_admin_h,8> sources{}; std::array<ars_ingress_cap,8> caps{};
        for(unsigned j=0;j<8;++j) registered(a,sources[j],caps[j],i);
        test::Gate sample; a.time.gate=&sample; std::array<ars_status,8> accepted{};
        std::array<test::Frame,8> frames; std::array<std::thread,8> workers;
        for(unsigned j=0;j<8;++j) workers[j]=std::thread([&,j]{accepted[j]=a.router.submit_frame(a.runtime,caps[j],&frames[j].value);});
        sample.wait_entered(); ComponentFixture::wait_admissions(*a.context,8);
        std::thread controller([&]{
            ComponentFixture::wait_closing(*a.context); std::array<std::thread,8> denied;
            for(unsigned j=0;j<8;++j) denied[j]=std::thread([&,j]{test::Frame later(2);
                for(unsigned n=0;n<8;++n) expect(ARS_E_CLOSED,a.router.submit_frame(a.runtime,caps[j],&later.value),"RACE-T-denied-flood",i);});
            for(auto &thread:denied) thread.join();
            ars_close_state state{}; expect(ARS_OK,a.router.query_close(a.runtime,&state),"RACE-T-management-during-drain",i);
            require(state.outstanding_ingress_pins==8 && state.outstanding_close_management_pins==1 && state.state==ARS_FENCE_PENDING,"RACE-T-counters",i);
            expect(ARS_E_BUSY,a.router.finish_close(a.runtime),"RACE-T-no-early-finalization",i); sample.release();
        });
        expect(ARS_OK,a.router.begin_close(a.runtime),"RACE-R/T-sticky-intent-drains",i);
        for(auto &thread:workers) thread.join();
        controller.join();
        for(auto result:accepted) expect(ARS_OK,result,"RACE-R-previous-append-before-quiescing",i);
        require(ComponentFixture::admission(*a.context).active()==0,"RACE-T-balanced-RAII",i);
        a.retained.reset(); ars_close_state state{}; expect(ARS_OK,a.router.query_close(a.runtime,&state),"RACE-T-drained-query",i);
        require(state.state==ARS_FENCE_COMPLETE && state.outstanding_ingress_pins==0 && state.outstanding_pins==1,"RACE-T-only-query-pin",i);
    }
    std::puts("PASS RACE-R/T: 1000 eight-admitted/eight-denied CV close/flood iterations");
}
}
int main() {
    std::printf("B2 RACE SEED=%llx iterations=%u per family; production objects, no sleeps\n",static_cast<unsigned long long>(seed),iterations);
    duplicate_queue(); rollover(); revoke_during_copy(); rotation_terminal(); rotation_expiry();
    clock_poison(); exhaustion(); close_during_rotation(); closing_flood();
    std::puts("PASS B2 authority races; B3 publish and all 87 whole security cases remain HOLD");
}
