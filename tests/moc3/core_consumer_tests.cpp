#include "core_consumer.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include "../motion/semantic_candidate.hpp"
using namespace ars::moc3;
void check(bool ok,const char* what){if(!ok){std::fprintf(stderr,"F4_FAIL %s\n",what);std::exit(1);}}
int main(){
    const std::vector<ParameterInfo> mapping{{"generated",-30,0,30},{"secondary",0,1,1}};
    CoreConsumer invalid;auto duplicate=mapping;duplicate[1]=duplicate[0];
    check(invalid.open(duplicate)==ARS_E_INVALID_ARGUMENT,"duplicate mapping");
    CoreConsumer consumer;check(consumer.open(mapping)==ARS_OK,"real core open");
    std::vector<Parameter> out;std::uint64_t generation=0;
    check(consumer.read(out,generation)!=ARS_OK,"no fabricated initial snapshot");
    auto reject=[&](std::vector<Parameter> input){check(consumer.prepare(1,input,out,generation)==ARS_E_INVALID_ARGUMENT,"invalid input before core ingress");};
    reject({{"missing",0}});reject({{"generated",31}});reject({{"generated",std::numeric_limits<float>::infinity()}});
    reject({{"generated",1},{"generated",2}});
    check(consumer.prepare(1,std::vector<Parameter>{{"generated",12}},out,generation)==ARS_OK&&generation==1&&out[0].value==12&&out[1].value==1,"actual semantic candidate");
    std::vector<Parameter> pending;std::uint64_t g=0;
    check(consumer.read(pending,g)!=ARS_OK,"prepared value not published");
    check(consumer.prepare(2,{},pending,g)==ARS_E_BUSY,"single candidate");
    check(consumer.publish()==ARS_OK&&consumer.read(out,generation)==ARS_OK&&generation==1&&out[0].value==12,"core publish");
    check(consumer.prepare(1,{},pending,g)==ARS_E_INVALID_FRAME,"native source replay denied");
    check(consumer.read(out,generation)==ARS_OK&&generation==1&&out[0].value==12,"replay preserves pose");
    check(consumer.prepare(2,{},pending,g)==ARS_OK&&pending[0].value==0&&pending[1].value==1,"release defaults");
    check(consumer.publish()==ARS_OK,"release commit");
    ars_status wrong=ARS_OK;std::thread t([&]{wrong=consumer.prepare(3,{},pending,g);});t.join();
    check(wrong==ARS_E_WRONG_THREAD,"consumer owner");
    check(consumer.prepare(3,std::vector<Parameter>{{"generated",5}},pending,g)==ARS_OK,"late revoke candidate");
    check(consumer.revoke()==ARS_OK&&consumer.publish()==ARS_E_REVOKED,"actual final authorization fence");
    check(consumer.read(out,generation)==ARS_OK&&generation==2&&out[0].value==0,"revoked candidate cannot publish");
    check(consumer.close()==ARS_OK,"close drains all core leases");
    CoreConsumer retry;check(retry.open(mapping,true)==ARS_OK,"opt-in recoverable consumer");
    check(retry.prepare(1,{},out,generation)==ARS_OK&&retry.publish()==ARS_OK,"recoverable initial accepted");
    check(retry.prepare(2,std::vector<Parameter>{{"generated",9}},out,generation)==ARS_OK,"recoverable candidate");retry.discard();
    check(retry.read(out,generation)==ARS_OK&&generation==1&&out[0].value==0,"discard preserves actual accepted core");
    check(retry.prepare(3,std::vector<Parameter>{{"generated",8}},out,generation)==ARS_OK&&generation==2&&retry.publish()==ARS_OK,"fresh ingress after discarded candidate, no authority rotation");
    check(retry.prepare(4,{},out,generation)==ARS_OK&&retry.revoke()==ARS_OK&&retry.publish()==ARS_E_REVOKED,"recoverable still rejects actual revoke");
    retry.discard();check(retry.prepare(5,{},out,generation)!=ARS_OK,"revoked source never auto-renewed");check(retry.close()==ARS_OK,"recoverable close");
    ars::core::RuntimeRouter router;ars::core::StageLease empty;float sentinel=7;g=55;
    check(router.copy_cpu_candidate(empty,std::span(&sentinel,1),&g)==ARS_E_INVALID_ARGUMENT&&sentinel==7&&g==0,"empty candidate copy fails closed");
    ars::core::RuntimeRouter foreign;ars_runtime_h runtime{};ars_character_h character{};
    ars_runtime_desc descriptor{};descriptor.struct_size=sizeof(descriptor);descriptor.abi_version=ARS_ABI_VERSION;
    check(router.create(descriptor,&runtime)==ARS_OK,"copy runtime");
    ars::security::SemanticChannel channel{};std::snprintf(channel.id.data(),channel.id.size(),"control.0");channel.neutral[0]=.25f;
    check(router.bind_character(runtime,std::span(&channel,1),&character)==ARS_OK,"copy character");
    ars::motion::Program program;program.count=2;
    program.nodes[0]={ars::motion::Op::neutral,0,0,0};program.nodes[1]={ars::motion::Op::output,0,0,0};
    ars::core::StageLease stage;
    check(router.prepare_cpu(runtime,character,{},program,&stage)==ARS_OK,"copy actual candidate");
    check(foreign.copy_cpu_candidate(stage,std::span(&sentinel,1),&g)==ARS_E_INVALID_ARGUMENT&&sentinel==7,"wrong router copy denied");
    check(router.copy_cpu_candidate(stage,{},&g)==ARS_E_INVALID_ARGUMENT&&sentinel==7,"copy size checked before mutation");
    check(router.copy_cpu_candidate(stage,std::span(&sentinel,1),&g)==ARS_OK&&sentinel==.25f&&g==1,"copy neutral bounded value");
    check(router.publish_cpu(&stage)==ARS_OK,"copy preserves publish path");
    check(router.begin_close(runtime)==ARS_OK&&router.finish_close(runtime)==ARS_OK,"copy lease cleanup");
    std::puts("F4_REAL_CORE_CONSUMER_PASS");
}
