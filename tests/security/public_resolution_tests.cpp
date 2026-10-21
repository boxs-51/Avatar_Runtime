#include "runtime_test_support.hpp"
#include "../../src/motion/public_resolution.hpp"
#include <cmath>
#include <memory>
#include <thread>
using namespace ars;
namespace {
struct Setup {
    test::Authority a;
    std::array<security::SemanticChannel,6> catalog{};
    std::array<const char *,6> names{"scalar","flag","vector2","vector3","rotation","pulse"};
    std::array<std::uint32_t,6> kinds{ARS_VALUE_SCALAR,ARS_VALUE_BOOL,ARS_VALUE_VEC2,ARS_VALUE_VEC3,ARS_VALUE_QUAT_XYZW,ARS_VALUE_TRIGGER};
    std::array<ars_source_admin_h,2> sources{};
    std::array<ars_ingress_cap,2> caps{};
    std::array<motion::RetainedInput,2> caches{};
    motion::PublicResolution resolution;
    motion::Program program;
    Setup() {
        test::status(ARS_OK,a.router.destroy_character(a.runtime,a.character),"replace fixture catalog");
        for(std::size_t i=0;i<catalog.size();++i) { std::memcpy(catalog[i].id.data(),names[i],std::strlen(names[i])+1);catalog[i].kind=kinds[i]; }
        catalog[4].neutral[3]=1;
        test::status(ARS_OK,a.router.bind_character(a.runtime,catalog,&a.character),"six typed semantics");
        std::array<ars_utf8_view,6> ids{};
        for(std::size_t i=0;i<ids.size();++i) ids[i]={names[i],static_cast<std::uint32_t>(std::strlen(names[i]))};
        ars_source_desc desc{};desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;
        desc.allowed_channel_ids=ids.data();desc.allowed_channel_count=6;desc.max_channels_per_frame=6;desc.max_frames_per_second=240;desc.lease_ttl_ns=security::max_ttl;
        for(unsigned i=0;i<2;++i) {
            test::status(ARS_OK,a.router.register_source(a.runtime,a.character,&desc,&sources[i],&caps[i]),"typed source");
            resolution.sources[i]={sources[i],1,0,&caches[i]};
        }
        resolution.count=2;program.public_resolution=&resolution;
        resolution.constraints[0]={-1,1,true,true};
    }
    void submit(unsigned source,std::uint64_t seq,std::initializer_list<std::pair<unsigned,std::array<float,4>>> values,float confidence=1) {
        std::array<ars_motion_channel,6> channels{};unsigned n=0;
        for(auto [index,value]:values) { auto &c=channels[n++];c.semantic_id={names[index],static_cast<std::uint32_t>(std::strlen(names[index]))};c.kind=kinds[index];std::copy(value.begin(),value.end(),c.value); }
        ars_motion_frame frame{};frame.struct_size=sizeof(frame);frame.abi_version=ARS_ABI_VERSION;frame.sequence=seq;
        frame.timestamp_mode=ARS_TIMESTAMP_HOST_MONOTONIC;frame.confidence=confidence;frame.channel_count=n;frame.channels=channels.data();
        test::status(ARS_OK,a.router.submit_frame(a.runtime,caps[source],&frame),"typed owned ingress");
    }
    void publish() { test::publish(a,std::span(sources.data(),resolution.count),program); }
    void read(core::SnapshotLease &lease) { test::status(ARS_OK,a.router.read_cpu(a.runtime,a.character,&lease),"typed read"); }
};
void blending() {
    Setup s;
    s.submit(0,1,{{0,{2,0,0,0}},{1,{1,0,0,0}},{2,{2,4,0,0}},{3,{1,2,3,0}},{4,{0,0,0,1}},{5,{1,0,0,0}}});
    s.submit(1,1,{{0,{0,0,0,0}},{1,{0,0,0,0}},{2,{0,0,0,0}},{3,{3,4,5,0}},{4,{0,0,0,-1}}});
    s.publish();core::SnapshotLease first;s.read(first);const auto &v=first.get()->outputs;
    test::check(v[0].scalar==1 && v[1].scalar==1 && v[2].scalar==1 && v[2].tail[0]==2 && v[3].scalar==2 && v[3].tail[0]==3 && v[3].tail[1]==4 && v[4].tail[2]==1 && v[5].scalar==1,"all typed blends / stable bool tie / quaternion hemisphere / trigger");
    // Each retained channel keeps the confidence from its own last update.
    s.submit(0,2,{{0,{-1,0,0,0}}},0.25f);s.publish();core::SnapshotLease second;s.read(second);
    test::check(std::abs(second.get()->outputs[0].scalar+0.2f)<1e-6f && second.get()->outputs[2].tail[0]==2 && second.get()->outputs[5].scalar==0,"partial channels retain values/confidence; trigger lasts one update");
    s.resolution.sources[0].priority=5;s.publish();core::SnapshotLease third;s.read(third);
    test::check(third.get()->outputs[0].scalar==-1 && third.get()->outputs[2].tail[0]==4,"highest priority retained band");
    test::check(s.caches[0].frame.count==5,"trigger never enters retained cache");
}
void fence(const char *kind) {
    Setup s;s.submit(0,1,{{0,{0.5f,0,0,0}}});s.publish();
    core::SnapshotLease before;s.read(before);const auto generation=before.get()->generation;const auto sequence=s.caches[0].frame.channels[0].value[0];
    s.submit(0,2,{{0,{0.75f,0,0,0}},{5,{1,0,0,0}}});
    core::StageLease stage;test::status(ARS_OK,s.a.router.prepare_cpu(s.a.runtime,s.a.character,s.sources,s.program,&stage),"prepare atomic cache candidate");
    ars_status expected=ARS_E_REVOKED;
    if(std::strcmp(kind,"revoke")==0)test::status(ARS_OK,s.a.router.revoke_source(s.a.runtime,s.sources[0]),"revoke before final fence");
    else if(std::strcmp(kind,"rotate")==0)test::status(ARS_OK,s.a.router.rotate_source(s.a.runtime,s.sources[0],&s.caps[0]),"rotate before final fence");
    else if(std::strcmp(kind,"expiry")==0) { s.a.time.now=security::max_ttl+101;expected=ARS_E_SOURCE_EXPIRED; }
    else if(std::strcmp(kind,"clock")==0) { s.a.time.fail=true;expected=ARS_E_CLOCK_FAILURE; }
    else if(std::strcmp(kind,"regression")==0) { s.a.time.now=99;expected=ARS_E_CLOCK_REGRESSION; }
    else if(std::strcmp(kind,"close")==0) { test::status(ARS_OK,s.a.router.begin_close(s.a.runtime),"close before final fence");expected=ARS_E_CLOSED; }
    test::status(expected,s.a.router.publish_cpu(&stage),"rejected final fence");
    if(std::strcmp(kind,"regression")==0) {
        core::StageLease retry;
        test::status(ARS_E_SECURITY_CLOCK_FAULT,s.a.router.prepare_cpu(s.a.runtime,s.a.character,s.sources,s.program,&retry),"security clock fault is sticky after first regression");
    }
    test::check(s.caches[0].frame.channels[0].value[0]==sequence && before.get()->generation==generation && before.get()->outputs[5].scalar==0,"failed fence changes neither cache nor old snapshot nor pulse");
}
void multi_batch(const char *failure) {
    Setup s;ars_character_h second{};
    test::status(ARS_OK,s.a.router.bind_character(s.a.runtime,s.catalog,&second),"second typed character");
    ars_utf8_view id{"scalar",6};ars_source_desc desc{};desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;
    desc.allowed_channel_ids=&id;desc.allowed_channel_count=1;desc.max_channels_per_frame=1;desc.max_frames_per_second=240;desc.lease_ttl_ns=security::max_ttl;
    ars_source_admin_h admin{};ars_ingress_cap cap{};test::status(ARS_OK,s.a.router.register_source(s.a.runtime,second,&desc,&admin,&cap),"second character source");
    auto resolutions=std::make_unique<std::array<motion::PublicResolution,2>>();motion::RetainedInput second_cache;
    (*resolutions)[0].count=1;(*resolutions)[0].sources[0]={s.sources[0],1,0,&s.caches[0]};
    (*resolutions)[1].count=1;(*resolutions)[1].sources[0]={admin,1,0,&second_cache};
    auto storage=std::make_unique<motion::PublicBatch>();
    auto prepare=[&](core::BatchLease &batch) {
        for(unsigned i=0;i<2;++i) {
            motion::Program program;program.public_resolution=&(*resolutions)[i];core::StageLease stage;
            std::array<ars_source_admin_h,1> selected{i==0?s.sources[0]:admin};
            test::status(ARS_OK,s.a.router.prepare_cpu(s.a.runtime,i==0?s.a.character:second,selected,program,&stage),"prepare all-character candidate");
            test::status(ARS_OK,s.a.router.append_cpu_batch(&stage,&batch),"bounded batch owns candidate pins");
        }
    };
    auto submit_second=[&](std::uint64_t sequence,float value) {
        ars_motion_channel channel{{"scalar",6},ARS_VALUE_SCALAR,0,{value,0,0,0}};ars_motion_frame frame{};
        frame.struct_size=sizeof(frame);frame.abi_version=ARS_ABI_VERSION;frame.sequence=sequence;frame.timestamp_mode=ARS_TIMESTAMP_HOST_MONOTONIC;
        frame.channel_count=1;frame.channels=&channel;frame.confidence=1;test::status(ARS_OK,s.a.router.submit_frame(s.a.runtime,cap,&frame),"second queued frame");
    };
    s.submit(0,1,{{0,{0.25f,0,0,0}}});submit_second(1,0.5f);
    { core::BatchLease batch(*storage);prepare(batch);test::status(ARS_OK,s.a.router.publish_cpu_batch(&batch),"initial all-character atomic publication"); }
    core::SnapshotLease first_before,second_before;s.read(first_before);test::status(ARS_OK,s.a.router.read_cpu(s.a.runtime,second,&second_before),"read second baseline");
    s.submit(0,2,{{0,{0.75f,0,0,0}},{5,{1,0,0,0}}});submit_second(2,0.9f);
    core::BatchLease batch(*storage);prepare(batch);ars_status expected=ARS_E_REVOKED;
    if(std::strcmp(failure,"pose")==0){core::ComponentFixture::pose_generation(*s.a.context,second,UINT64_MAX);expected=ARS_E_HANDLE_EXHAUSTED;}
    else if(std::strcmp(failure,"revoke")==0)test::status(ARS_OK,s.a.router.revoke_source(s.a.runtime,admin),"revoke second batch source");
    else if(std::strcmp(failure,"rotate")==0)test::status(ARS_OK,s.a.router.rotate_source(s.a.runtime,admin,&cap),"rotate second batch source");
    else if(std::strcmp(failure,"clock")==0){s.a.time.fail=true;expected=ARS_E_CLOCK_FAILURE;}
    else if(std::strcmp(failure,"close")==0){test::status(ARS_OK,s.a.router.begin_close(s.a.runtime),"close whole batch");expected=ARS_E_CLOSED;}
    unsigned committed=0;auto commit=[](void *p) noexcept {++*static_cast<unsigned *>(p);};
    test::status(expected,s.a.router.publish_cpu_batch(&batch,commit,&committed),"second character rejection rejects entire batch");
    test::check(committed==0 && s.caches[0].frame.channels[0].value[0]==0.25f && second_cache.frame.channels[0].value[0]==0.5f && first_before.get()->generation==1 && second_before.get()->generation==1,"no earlier snapshot/cache/S-time commit on later rejection");
    if(std::strcmp(failure,"close")!=0)test::check(core::ComponentFixture::queue(*s.a.context,s.sources[0]).size()==1,"earlier queue/trigger not consumed");
    if(std::strcmp(failure,"pose")==0) {
        core::ComponentFixture::pose_generation(*s.a.context,second,1);
        core::BatchLease retry(*storage);prepare(retry);test::status(ARS_OK,s.a.router.publish_cpu_batch(&retry,commit,&committed),"retry commits all characters once");
        core::SnapshotLease first,other;s.read(first);test::status(ARS_OK,s.a.router.read_cpu(s.a.runtime,second,&other),"second accepted snapshot");
        test::check(committed==1 && first.get()->generation==2 && other.get()->generation==2 && first.get()->outputs[0].scalar==0.75f && other.get()->outputs[0].scalar==0.9f && first.get()->outputs[5].scalar==1,"snapshot/queue/cache/bookkeeping share successful commit");
    }
}
void full_character_batch() {
    test::Authority a;
    std::array<ars_character_h,security::max_characters> characters{};characters[0]=a.character;
    std::array<ars_source_admin_h,security::max_sources> sources{};std::array<ars_ingress_cap,security::max_sources> caps{};
    auto resolutions=std::make_unique<std::array<motion::PublicResolution,security::max_characters>>();
    auto caches=std::make_unique<std::array<motion::RetainedInput,security::max_sources>>();
    std::array<security::SemanticChannel,1> catalog{};std::memcpy(catalog[0].id.data(),"mouth.open",11);
    ars_utf8_view id{"mouth.open",10};ars_source_desc desc{};desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;desc.allowed_channel_ids=&id;desc.allowed_channel_count=1;desc.max_channels_per_frame=1;desc.max_frames_per_second=240;desc.lease_ttl_ns=security::max_ttl;
    auto storage=std::make_unique<motion::PublicBatch>();core::BatchLease batch(*storage);
    for(unsigned i=0;i<characters.size();++i) {
        if(i)test::status(ARS_OK,a.router.bind_character(a.runtime,catalog,&characters[i]),"all16 characters fit original cap");
        test::status(ARS_OK,a.router.register_source(a.runtime,characters[i],&desc,&sources[i],&caps[i]),"all16 sources fit original cap");
        test::submit(a,caps[i],1,0.5f);auto &resolution=(*resolutions)[i];resolution.count=1;resolution.sources[0]={sources[i],1,0,&(*caches)[i]};
        motion::Program p;p.public_resolution=&resolution;core::StageLease stage;
        test::status(ARS_OK,a.router.prepare_cpu(a.runtime,characters[i],std::span(&sources[i],1),p,&stage),"prepare all16 via unchanged8-slot scratch");
        test::status(ARS_OK,a.router.append_cpu_batch(&stage,&batch),"append all16 bounded snapshots");
    }
    test::status(ARS_OK,a.router.publish_cpu_batch(&batch),"all16 characters atomically update");
    for(auto character:characters){core::SnapshotLease pin;test::status(ARS_OK,a.router.read_cpu(a.runtime,character,&pin),"all16 read");test::check(pin.get()->generation==1 && pin.get()->outputs[0].scalar==0.5f,"all16 snapshots accepted");}
}
void last_batch_pin() {
    for(unsigned iteration=0;iteration<128;++iteration) {
        core::RuntimeRouter router;ars_runtime_desc desc{};ars_runtime_h runtime{};test::status(ARS_OK,router.create(desc,&runtime),"last batch runtime");
        std::array<security::SemanticChannel,1> catalog{};std::memcpy(catalog[0].id.data(),"value",6);ars_character_h character{};
        test::status(ARS_OK,router.bind_character(runtime,catalog,&character),"last batch character");
        auto scratch=std::make_unique<motion::PublicBatch>();core::BatchLease batch(*scratch);core::StageLease stage;motion::Program program;
        test::status(ARS_OK,router.prepare_cpu(runtime,character,{},program,&stage),"last batch prepare");test::status(ARS_OK,router.append_cpu_batch(&stage,&batch),"last work pin belongs to batch");
        test::status(ARS_OK,router.begin_close(runtime),"close with only batch pin");
        std::atomic<bool> start=false;ars_status published=ARS_E_INTERNAL;
        std::thread publisher([&]{while(!start.load(std::memory_order_acquire))std::this_thread::yield();published=router.publish_cpu_batch(&batch);});
        start.store(true,std::memory_order_release);bool finished=false;
        for(unsigned i=0;i<10000;++i){const auto status=router.finish_close(runtime);if(status==ARS_OK){finished=true;break;}test::status(ARS_E_BUSY,status,"finalizer waits for batch and mutation guard");std::this_thread::yield();}
        publisher.join();test::status(ARS_E_CLOSED,published,"last pin publication rejected by close");
        if(!finished)test::status(ARS_OK,router.finish_close(runtime),"finalizer after publication guard exits");
        test::check(router.occupied()==0,"runtime reclaimed once after final batch guard");
    }
}
void bind_rollback() {
    for(unsigned failure=0;failure<3;++failure) {
        core::HandleRegistry registry(123,1,failure==1?0:1024);security::CallbackState callback;
        ars_runtime_desc desc{};auto storage=std::make_unique<security::SourceAuthority>(registry,desc,123,callback);auto &authority=*storage;
        ars_handle_id occupied{};if(failure==0)test::status(ARS_OK,registry.insert(core::ObjectType::runtime,&registry,&occupied),"fill registry for failed bind");
        if(failure==2)authority.request_close();
        std::atomic<unsigned> retired=0;
        struct Owner {
            security::SourceAuthority &authority;std::atomic<unsigned> &retired;
            ~Owner() { ++retired; authority.collect_retired(); } // Reentrant sweep proves objects_ is not held.
        };
        std::array<security::SemanticChannel,1> catalog{};std::memcpy(catalog[0].id.data(),"value",6);
        ars_character_h out{{9,9,9}};
        const auto expected=failure==0?ARS_E_OUT_OF_BUDGET:failure==1?ARS_E_BUSY:ARS_E_CLOSED;
        test::status(expected,authority.bind(catalog,&out,std::make_shared<Owner>(authority,retired)),"bind failure rollback");
        test::check(retired==1 && out.id.runtime_uid==0 && out.id.slot==0 && out.id.generation==0,"failed bind frees owner before return and zeros handle");
    }
}
void retention() {
    test::Authority a;std::atomic<unsigned> deleted=0;
    struct Owner { std::atomic<unsigned> &count;~Owner(){++count;} };
    std::array<security::SemanticChannel,1> catalog{};std::memcpy(catalog[0].id.data(),"value",6);
    auto owner=std::make_shared<Owner>(deleted);ars_character_h character{};
    test::status(ARS_OK,a.context->bind(catalog,&character,owner),"bind model owner by character generation");owner.reset();
    motion::Program p;core::StageLease stage;test::status(ARS_OK,a.router.prepare_cpu(a.runtime,character,{},p,&stage),"pin owned character candidate");
    test::status(ARS_OK,a.router.publish_cpu(&stage),"owned snapshot");core::SnapshotLease snapshot;
    test::status(ARS_OK,a.router.read_cpu(a.runtime,character,&snapshot),"owned historical snapshot");
    test::status(ARS_OK,a.router.destroy_character(a.runtime,character),"destroy owned character");a.context->collect_retired();
    test::check(deleted==0,"old snapshot retains model owner");
    ars_character_h replacement{};auto next=std::make_shared<Owner>(deleted);
    test::status(ARS_OK,a.context->bind(catalog,&replacement,next),"replacement generation cannot reuse pinned slot");next.reset();
    snapshot.reset();a.context->collect_retired();test::check(deleted==1,"old generation frees exactly once outside locks");
    test::status(ARS_OK,a.router.destroy_character(a.runtime,replacement),"destroy replacement");a.context->collect_retired();test::check(deleted==2,"replacement model remains independent");
}
}
int main() {
    blending();for(auto kind:{"revoke","rotate","expiry","clock","regression","close"})fence(kind);retention();bind_rollback();for(auto failure:{"pose","revoke","rotate","clock","close"})multi_batch(failure);full_character_batch();last_batch_pin();
    std::puts("PASS typed transactional motion and generation-bound owner retirement");
}
