#include "runtime_test_support.hpp"
#include "../../src/motion/publish_gate.hpp"
int main() {
    test::Authority a; ars::motion::Program program; ars::core::StageLease stage;
    test::status(ARS_OK,a.router.prepare_cpu(a.runtime,a.character,{},program,&stage),"B3-real-runtime-prepare-neutral-candidate");
}
