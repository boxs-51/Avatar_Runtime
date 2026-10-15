#include "runtime_test_support.hpp"
#include <barrier>
#include <thread>
using ars::core::StageLease;
using ars::core::SnapshotLease;
using ars::core::ComponentFixture;
namespace {
constexpr unsigned iterations=1000;
constexpr std::uint64_t seed=0x4152534232330001ULL;
void require(bool result,const char *family,unsigned i) {
    if(!result) { std::printf("FAIL B3 family=%s iteration=%u seed=%llx\n",family,i,static_cast<unsigned long long>(seed)); std::exit(1); }
}
void expect(ars_status wanted,ars_status got,const char *family,unsigned i) {
    if(wanted!=got) { std::printf("FAIL B3 %s iteration=%u seed=%llx expected=%d actual=%d\n",family,i,static_cast<unsigned long long>(seed),wanted,got); std::exit(1); }
}
void submit(test::Authority &a,ars_ingress_cap cap,float value,unsigned i) {
    test::Frame frame; frame.channel.value[0]=value;
    expect(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"owned-input",i);
}
void publish(test::Authority &a,std::span<const ars_source_admin_h> sources,const ars::motion::Program &program,unsigned i) {
    StageLease stage; expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,sources,program,&stage),"prepare",i);
    expect(ARS_OK,a.router.publish_cpu(&stage),"publish",i);
}
struct Pair {
    test::Authority a; std::array<ars_source_admin_h,2> sources{}; std::array<ars_ingress_cap,2> caps{};
    Pair(unsigned i,std::uint64_t ttl=ars::security::max_ttl) {
        expect(ARS_OK,a.register_source(&sources[0],&caps[0],240,ttl),"register-A",i);
        expect(ARS_OK,a.register_source(&sources[1],&caps[1]),"register-B",i);
        submit(a,caps[0],1,i); submit(a,caps[1],2,i); publish(a,sources,test::blend_program(),i);
    }
};
void value(test::Authority &a,float wanted,std::uint64_t generation,unsigned i) {
    SnapshotLease snapshot; expect(ARS_OK,a.router.read_cpu(a.runtime,a.character,&snapshot),"read",i);
    require(snapshot.get()->outputs[0].scalar==wanted && snapshot.get()->generation==generation,"pose-value-and-generation",i);
    require(snapshot.get()->state.cells[2].scalar==wanted,"pose-and-persistent-state-same-revision",i);
}
void terminal_cut(unsigned mode) {
    for(unsigned i=0;i<iterations;++i) {
        Pair p(i,mode==1?10:ars::security::max_ttl); auto &a=p.a; StageLease stage;
        auto changed=test::retained_program(); changed.count=8;
        changed.nodes[1]={ars::motion::Op::scale,0,0,0,0,2};
        changed.nodes[2]={ars::motion::Op::store,1,0,0};
        changed.nodes[3]={ars::motion::Op::retained,0,0,1}; changed.nodes[4]={ars::motion::Op::add,1,3};
        changed.nodes[5]={ars::motion::Op::store,4,0,2}; changed.nodes[6]={ars::motion::Op::trigger,4}; changed.nodes[7]={ars::motion::Op::output,4,0,0};
        expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},changed,&stage),"stage-A+B",i);
        test::Gate gate; ars_status result=ARS_OK;
        std::thread worker([&]{gate.arrive_and_wait(); result=a.router.publish_cpu(&stage);}); gate.wait_entered();
        if(mode==0) expect(ARS_OK,a.router.revoke_source(a.runtime,p.sources[0]),"revoke-before-final",i);
        else if(mode==1) {
            a.time.now=110; test::Frame expired(2);
            expect(ARS_E_SOURCE_EXPIRED,a.router.submit_frame(a.runtime,p.caps[0],&expired.value),"V-ingress-release-tombstone",i);
        } else {
            ars_ingress_cap replacement{}; expect(ARS_OK,a.router.rotate_source(a.runtime,p.sources[0],&replacement),"rotate-before-final",i);
        }
        gate.release(); worker.join();
        expect(mode==1?ARS_E_SOURCE_EXPIRED:ARS_E_REVOKED,result,"candidate-and-staged-writes-rejected",i);
        value(a,3,1,i); // Historical display remains immutable; never a new seed.
        publish(a,{},test::retained_program(),i); value(a,2,2,i);
        SnapshotLease clean; expect(ARS_OK,a.router.read_cpu(a.runtime,a.character,&clean),"clean-state",i);
        require(clean.get()->state.trigger_count==0 && clean.get()->state.provenance.count==1,"V-purge-trigger-cache-and-A-retained-cell",i);
        clean.reset(); expect(ARS_OK,a.router.revoke_source(a.runtime,p.sources[1]),"revoke-B",i);
        publish(a,{},test::retained_program(),i); value(a,0,3,i);
    }
    std::printf("PASS B3 terminal-cut-%u: 1000 A/B/C/F/H/I/K/V epoch/state/trigger recomputations\n",mode);
}
void close_cut() {
    for(unsigned i=0;i<iterations;++i) {
        Pair p(i); auto &a=p.a; StageLease stage;
        expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},test::retained_program(),&stage),"M-old-S-pin",i);
        test::Gate gate; ars_status result=ARS_OK;
        std::thread worker([&]{gate.arrive_and_wait(); result=a.router.publish_cpu(&stage);}); gate.wait_entered();
        expect(ARS_OK,a.router.begin_close(a.runtime),"M-close-before-commit",i);
        expect(ARS_E_BUSY,a.router.finish_close(a.runtime),"M-no-early-free",i);
        gate.release(); worker.join(); expect(ARS_E_CLOSED,result,"M-no-post-quiescing-pose",i);
    }
    std::puts("PASS B3 M: 1000 retained S work/close/direct-unpin cuts");
}
struct ClockCut {
    std::uint64_t now=100; bool advance=false;
    static ars_status read(void *data,std::uint64_t *out) noexcept {
        auto &clock=*static_cast<ClockCut *>(data); *out=clock.now; if(clock.advance) clock.now=110; return ARS_OK;
    }
};
void time_cut() {
    for(unsigned i=0;i<iterations;++i) {
        for(bool after:{false,true}) {
            test::Authority a; ClockCut clock;
            ComponentFixture::clock(*a.context,ClockCut::read,&clock);
            ars_source_admin_h source{}; ars_ingress_cap cap{};
            expect(ARS_OK,a.register_source(&source,&cap,240,10),"O/P-register",i); submit(a,cap,1,i);
            auto program=test::blend_program(); program.nodes[1].source=0;
            StageLease stage; expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,std::span(&source,1),program,&stage),"O/P-prepare",i);
            if(after) clock.advance=true; else clock.now=110;
            expect(after?ARS_OK:ARS_E_SOURCE_EXPIRED,a.router.publish_cpu(&stage),"O/P-final-sample-time-contract",i);
            if(after) {
                SnapshotLease snapshot; expect(ARS_OK,a.router.read_cpu(a.runtime,a.character,&snapshot),"P-valid-historical-read",i);
                require(snapshot.get()->generation==1 && clock.now==110,"P-no-wall-clock-erasure-claim",i);
            }
        }
    }
    std::puts("PASS B3 O/P: 1000 before-sample expiry and after-valid-sample cases each");
}
void clock_cut() {
    for(unsigned i=0;i<iterations;++i) {
        Pair p(i); auto &a=p.a; StageLease stage;
        expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},test::retained_program(),&stage),"S-prepare",i);
        a.time.now=99; expect(ARS_E_CLOCK_REGRESSION,a.router.publish_cpu(&stage),"S-single-ingress-publish-clock-gate",i);
        a.time.now=101; expect(ARS_E_SECURITY_CLOCK_FAULT,a.router.prepare_cpu(a.runtime,a.character,{},test::retained_program(),&stage),"Q/S-sticky-publish-fault",i);
        test::Frame frame(2); expect(ARS_E_SECURITY_CLOCK_FAULT,a.router.submit_frame(a.runtime,p.caps[1],&frame.value),"Q/S-sticky-ingress-fault",i); value(a,3,1,i);
    }
    std::puts("PASS B3 Q/S: 1000 runtime-global clock poison/recovery-denial cuts");
}
void conflict_cut() {
    for(unsigned i=0;i<iterations;++i) {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{};
        expect(ARS_OK,a.register_source(&source,&cap),"ATOMIC-input-source",i); submit(a,cap,1,i);
        ars::motion::Program program; program.count=3; program.nodes[0]={ars::motion::Op::input};
        program.nodes[1]={ars::motion::Op::store,0}; program.nodes[2]={ars::motion::Op::output,0}; StageLease first,second;
        expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,std::span(&source,1),program,&first),"ATOMIC-first",i);
        expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,std::span(&source,1),program,&second),"ATOMIC-second",i);
        std::barrier start(3); std::array<ars_status,2> results{};
        std::thread one([&]{start.arrive_and_wait(); results[0]=a.router.publish_cpu(&first);});
        std::thread two([&]{start.arrive_and_wait(); results[1]=a.router.publish_cpu(&second);});
        start.arrive_and_wait(); one.join(); two.join();
        require((results[0]==ARS_OK && results[1]==ARS_E_BUSY)||(results[1]==ARS_OK && results[0]==ARS_E_BUSY),"ATOMIC-one-revision-winner",i);
        SnapshotLease snapshot; expect(ARS_OK,a.router.read_cpu(a.runtime,a.character,&snapshot),"ATOMIC-read",i);
        require(snapshot.get()->generation==1 && snapshot.get()->outputs[0].scalar==1 &&
          snapshot.get()->state.cells[0].scalar==1 && !ComponentFixture::queue(*a.context,source).size(),"ATOMIC-input-pose-state-one-commit",i);
    }
    std::puts("PASS B3 ATOMIC: 1000 concurrent optimistic revision conflicts");
}
void reader_cut() {
    test::Authority a; ars::motion::Program program; publish(a,{},program,0);
    for(unsigned i=0;i<iterations;++i) {
        StageLease stage; expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"SNAPSHOT-stage",i);
        std::barrier start(2); SnapshotLease snapshot; ars_status read=ARS_E_INTERNAL;
        std::thread reader([&]{start.arrive_and_wait(); read=a.router.read_cpu(a.runtime,a.character,&snapshot);});
        start.arrive_and_wait(); expect(ARS_OK,a.router.publish_cpu(&stage),"SNAPSHOT-publish-vs-reader",i); reader.join(); expect(ARS_OK,read,"SNAPSHOT-acquire-before-access",i);
        require(snapshot.get()->generation==2*i+1 || snapshot.get()->generation==2*i+2,"SNAPSHOT-consistent-old-or-new-revision",i);
        auto generation=snapshot.get()->generation; publish(a,{},program,i);
        require(snapshot.get()->generation==generation,"SNAPSHOT-old-memory-never-mutates",i);
    }
    std::puts("PASS B3 SNAPSHOT: 1000 immutable reader/publisher interleavings");
}
void order_cut() {
    for(unsigned i=0;i<iterations;++i) {
        Pair p(i); auto &a=p.a; StageLease stage;
        expect(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},test::retained_program(),&stage),"ORDER-prepare",i);
        std::barrier start(3); ars_status published=ARS_E_INTERNAL,rotated=ARS_E_INTERNAL;
        std::thread publisher([&]{start.arrive_and_wait(); published=a.router.publish_cpu(&stage);});
        std::thread rotate([&]{ars_ingress_cap next{}; start.arrive_and_wait(); rotated=a.router.rotate_source(a.runtime,p.sources[i%2],&next);});
        start.arrive_and_wait(); publisher.join(); rotate.join(); expect(ARS_OK,rotated,"ORDER-rotate",i);
        require(published==ARS_OK || published==ARS_E_REVOKED || published==ARS_E_BUSY,"ORDER-source-slot-order-no-inversion",i);
    }
    std::puts("PASS B3 ORDER: 1000 multi-source final lock/rotation orders");
}
}
int main(int argc,char **argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    if(argc>1 && std::strcmp(argv[1],"epoch")==0) { terminal_cut(2); return 0; }
    std::printf("B3 RACE seed=%llx iterations=%u/family; real components, no sleeps\n",static_cast<unsigned long long>(seed),iterations);
    if(argc>1) {
        if(std::strcmp(argv[1],"revoke")==0) terminal_cut(0);
        else if(std::strcmp(argv[1],"expiry")==0) terminal_cut(1);
        else if(std::strcmp(argv[1],"rotate")==0) terminal_cut(2);
        else if(std::strcmp(argv[1],"close")==0) close_cut();
        else if(std::strcmp(argv[1],"time")==0) time_cut();
        else if(std::strcmp(argv[1],"clock")==0) clock_cut();
        else if(std::strcmp(argv[1],"atomic")==0) conflict_cut();
        else if(std::strcmp(argv[1],"reader")==0) reader_cut();
        else if(std::strcmp(argv[1],"order")==0) order_cut();
        else return 2;
        return 0;
    }
    terminal_cut(0); terminal_cut(1); terminal_cut(2); close_cut(); time_cut(); clock_cut(); conflict_cut(); reader_cut(); order_cut();
    std::puts("PASS B3 production CPU publication races; all 87 whole-case verdicts remain HOLD");
}
