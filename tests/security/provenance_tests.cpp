#include "runtime_test_support.hpp"
#include <thread>
#include <stdexcept>
using ars::core::ComponentFixture;
using ars::motion::Op;
struct Capture {
    test::Authority *owner=nullptr,*other=nullptr;
    ars_source_admin_h source{}; ars_ingress_cap cap{},other_cap{};
    std::array<ars_status,9> nested{};
    ars_status independent=ARS_E_INTERNAL;
    std::uint64_t now=200;
    bool mutate=false,throw_error=false;
    test::Gate *gate=nullptr;
    std::atomic<unsigned> calls{0};
    ars::core::StageLease *stage=nullptr;
    static std::uint64_t ARS_CALL read(void *data) {
        auto &c=*static_cast<Capture *>(data); ++c.calls;
        if(c.gate) c.gate->arrive_and_wait();
        if(c.mutate) {
            auto &a=*c.owner; ars_ingress_cap cap{}; ars_source_admin_h source{}; ars_character_h character{};
            std::array<ars::security::SemanticChannel,1> catalog{}; std::memcpy(catalog[0].id.data(),"mouth.open",11);
            c.nested[0]=a.router.rotate_source(a.runtime,c.source,&cap);
            c.nested[1]=a.router.revoke_source(a.runtime,c.source);
            c.nested[2]=a.router.release_source(a.runtime,c.source);
            c.nested[3]=a.router.destroy_character(a.runtime,a.character);
            c.nested[4]=a.router.bind_character(a.runtime,catalog,&character);
            c.nested[5]=a.router.begin_close(a.runtime);
            c.nested[6]=a.router.finish_close(a.runtime);
            c.nested[7]=a.register_source(&source,&cap);
            if(c.stage) c.nested[8]=a.router.publish_cpu(c.stage);
            if(c.other) {
                test::Frame frame; frame.value.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;
                c.independent=c.other->router.submit_frame(c.other->runtime,c.other_cap,&frame.value);
            }
        }
        if(c.throw_error) throw std::runtime_error("capture");
        return c.now;
    }
};
void callbacks() {
    Capture capture,independent; test::Authority a(Capture::read,&capture),b(Capture::read,&independent);
    capture.owner=&a; capture.other=&b;
    test::status(ARS_OK,a.register_source(&capture.source,&capture.cap),"P2-register");
    ars_source_admin_h other{}; test::status(ARS_OK,b.register_source(&other,&capture.other_cap),"P2-other-register");
    ars::motion::Program program; ars::core::StageLease stage;
    test::status(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"P2-stage-before-callback");
    capture.stage=&stage; capture.mutate=true;
    test::Frame frame; frame.value.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;
    test::status(ARS_OK,a.router.submit_frame(a.runtime,capture.cap,&frame.value),"P2-authority-unchanged-during-callback");
    for(auto status:capture.nested) test::status(ARS_E_BUSY,status,"P2-all-mutating-entrypoints");
    test::status(ARS_OK,capture.independent,"P2-unrelated-runtime-callback-allowed");
    capture.mutate=false; capture.stage=nullptr; capture.now=100; frame.value.sequence=2;
    auto &source=ComponentFixture::source(*a.context,capture.source); auto credit=source.credit;
    test::status(ARS_E_CLOCK_REGRESSION,a.router.submit_frame(a.runtime,capture.cap,&frame.value),"P2-capture-regression-rejected");
    test::check(source.sequence==1 && source.credit==credit && ComponentFixture::queue(*a.context,capture.source).size()==1,"P2-regression-no-sequence-quota-queue-mutation");
    capture.now=200;
    test::status(ARS_OK,a.router.submit_frame(a.runtime,capture.cap,&frame.value),"P2-equal-time-retry-same-sequence");
    capture.throw_error=true; frame.value.sequence=3;
    test::status(ARS_E_INTERNAL,a.router.submit_frame(a.runtime,capture.cap,&frame.value),"P2-exception-contained");
    capture.throw_error=false; capture.now=201;
    test::status(ARS_OK,a.router.submit_frame(a.runtime,capture.cap,&frame.value),"P2-guard-restored-after-exception");
    test::Gate gate; capture.gate=&gate; frame.value.sequence=4; ars_status first=ARS_E_INTERNAL;
    std::thread worker([&]{first=a.router.submit_frame(a.runtime,capture.cap,&frame.value);}); gate.wait_entered();
    auto calls=capture.calls.load(); test::Frame second(5); second.value.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;
    test::status(ARS_E_BUSY,a.router.submit_frame(a.runtime,capture.cap,&second.value),"P2-one-callback-per-runtime");
    test::status(ARS_E_BUSY,a.router.revoke_source(a.runtime,capture.source),"P2-other-thread-mutation-denied");
    test::check(capture.calls==calls,"P2-concurrent-callback-not-invoked");
    ars_close_state state{}; test::status(ARS_OK,a.router.query_close(a.runtime,&state),"P2-management-query-remains-available");
    gate.release(); worker.join(); capture.gate=nullptr; test::status(ARS_OK,first,"P2-paused-callback-completes");
}
void lineage() {
    ars::motion::Provenance provenance;
    for(unsigned i=0;i<16;++i) {
        std::uint16_t mask=0;
        test::status(ARS_OK,provenance.include({{{1,1,1}},{{1,i+2,1}},1},&mask),"CLOSURE-sixteen-full-identity-epochs");
        test::check(mask==(1U<<i),"CLOSURE-exact-mask");
    }
    std::uint16_t mask=0; test::status(ARS_E_OUT_OF_BUDGET,provenance.include({{{1,1,1}},{{1,99,1}},1},&mask),"CLOSURE-never-truncate-union");
    test::check(provenance.count==16,"CLOSURE-overflow-preserves-set");
    test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{};
    test::status(ARS_OK,a.register_source(&source,&cap),"CLOSURE-register"); test::submit(a,cap,1,0.75F);
    ars::motion::Program program; program.count=7;
    program.nodes[0]={Op::input,0,0,0,0}; program.nodes[1]={Op::scale,0,0,0,0,-1};
    program.nodes[2]={Op::add,0,1}; program.nodes[3]={Op::scale,0,0,0,0,0};
    program.nodes[4]={Op::store,3,0,3}; program.nodes[5]={Op::trigger,2,0,0,0,1,500}; program.nodes[6]={Op::output,2,0,0};
    test::publish(a,std::span(&source,1),program); ars::core::SnapshotLease snapshot;
    test::status(ARS_OK,a.router.read_cpu(a.runtime,a.character,&snapshot),"CLOSURE-read");
    const auto *view=snapshot.get();
    test::check(view->outputs[0].scalar==0 && view->outputs[0].dependencies==1 && view->state.cells[3].dependencies==1 &&
       view->state.triggers[0].value.dependencies==1 && view->state.provenance.count==1,"CLOSURE-cancel-zero-weight-delay-trigger-retain-authority");
    snapshot.reset(); test::status(ARS_OK,a.router.revoke_source(a.runtime,source),"CLOSURE-revoke");
    ars::motion::Program retained; retained.count=2; retained.nodes[0]={Op::retained,0,0,3}; retained.nodes[1]={Op::output,0,0,0};
    test::publish(a,{},retained);
    test::status(ARS_OK,a.router.read_cpu(a.runtime,a.character,&snapshot),"CLOSURE-read-clean-baseline");
    test::check(snapshot.get()->state.provenance.count==0 && snapshot.get()->state.trigger_count==0 && snapshot.get()->outputs[0].scalar==0,
        "CLOSURE-purges-all-future-effects-no-old-pose-seed");
}
int main() { callbacks(); lineage(); std::puts("PASS B3 provenance and both P2 fixes; public APIs remain disabled"); }
