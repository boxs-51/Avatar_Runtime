#include "runtime_test_support.hpp"
#include "../../src/motion/publish_gate.hpp"
int main() {
    {
        test::Authority a; ars::motion::Program program; ars::core::StageLease stage;
        test::status(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"B3-real-runtime-prepare-neutral-candidate");
        test::status(ARS_OK,a.router.publish_cpu(&stage),"SNAPSHOT-publish-initial");
        std::array<ars::core::SnapshotLease,64> readers;
        for(unsigned i=0;i<64;++i) {
            test::status(ARS_OK,a.router.read_cpu(a.runtime,a.character,&readers[i]),"SNAPSHOT-retain-historical-reader");
            test::check(readers[i].get()->generation==i+1,"SNAPSHOT-immutable-generation");
            if(i!=63) test::publish(a,{},program);
        }
        test::status(ARS_E_OUT_OF_BUDGET,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"SNAPSHOT-no-eviction-at-64-slots");
        readers[0].reset(); test::publish(a,{},program);
        test::check(readers[1].get()->generation==2,"SNAPSHOT-old-reader-survives-slot-recycle");
        test::status(ARS_OK,a.router.destroy_character(a.runtime,a.character),"SNAPSHOT-logical-character-destroy");
        test::check(readers[63].get()->generation==64,"SNAPSHOT-direct-reader-survives-invalidation");
        a.retained.reset(); test::status(ARS_OK,a.router.begin_close(a.runtime),"SNAPSHOT-begin-close");
        ars_close_state state{}; test::status(ARS_OK,a.router.query_close(a.runtime,&state),"SNAPSHOT-close-counters");
        test::check(state.outstanding_render_snapshot_pins==63 && state.outstanding_pins==64 && state.outstanding_gpu_retires==0 && state.state==ARS_FENCE_PENDING,
            "SNAPSHOT-root-accounting-no-GPU-completion-claim");
        test::status(ARS_E_BUSY,a.router.finish_close(a.runtime),"SNAPSHOT-finalizer-waits-for-last-reader");
        for(auto &reader:readers) reader.reset();
    }
    {
        test::Authority a; ars::motion::Program program;
        std::array<ars::core::StageLease,8> stages; ars::core::StageLease overflow;
        for(auto &stage:stages) test::status(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"STAGE-eight-bounded-reservations");
        test::status(ARS_E_BUSY,a.router.prepare_cpu(a.runtime,a.character,{},program,&overflow),"STAGE-no-unbounded-candidate-scratch");
        stages[0].reset(); test::status(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},program,&overflow),"STAGE-capacity-restored-by-direct-release");
    }
    {
        ars::core::RuntimeBudget budget; budget.publication_bytes=1; test::Authority a(nullptr,nullptr,budget);
        ars::motion::Program program; ars::core::StageLease stage;
        test::status(ARS_E_OUT_OF_BUDGET,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"B3-BYTES-no-partial-publication-context");
    }
    {
        test::Authority a; ars::motion::Program program; test::publish(a,{},program);
        ars::core::SnapshotLease before; test::status(ARS_OK,a.router.read_cpu(a.runtime,a.character,&before),"ATOMIC-initial-reader");
        auto generation=before.get()->generation;
        program.count=513; ars::core::StageLease stage;
        test::status(ARS_E_OUT_OF_BUDGET,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"ATOMIC-node-overflow-before-state-change");
        program.count=2; program.nodes[0]={ars::motion::Op::neutral}; program.nodes[1]={ars::motion::Op::store,0,0,256};
        test::status(ARS_E_OUT_OF_BUDGET,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"ATOMIC-cell-overflow");
        program.count=34;
        for(unsigned i=1;i<34;++i) program.nodes[i]={ars::motion::Op::trigger,0};
        test::status(ARS_E_OUT_OF_BUDGET,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"ATOMIC-trigger-overflow-no-partial-writes");
        ars::core::SnapshotLease after; test::status(ARS_OK,a.router.read_cpu(a.runtime,a.character,&after),"ATOMIC-reader-after-rejections");
        test::check(after.get()->generation==generation && after.get()->state.trigger_count==0,"ATOMIC-old-revision-unchanged");
        program={}; program.count=512;
        test::publish(a,{},program);
        ars::core::ComponentFixture::pose_generation(*a.context,a.character,UINT64_MAX);
        program={}; test::status(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"POSE-exhaustion-stage");
        test::status(ARS_E_HANDLE_EXHAUSTED,a.router.publish_cpu(&stage),"POSE-counter-never-wraps");
        test::status(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"POSE-exhaustion-restage");
        test::status(ARS_E_HANDLE_EXHAUSTED,a.router.publish_cpu(&stage),"POSE-exhaustion-terminal");
    }
    std::puts("PASS B3 immutable snapshot lifetime, resource budgets and atomic failure");
}
