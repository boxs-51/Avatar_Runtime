#include "runtime_test_support.hpp"
#include <array>
#include <cstring>
#include <limits>
#include <thread>
using ars::core::ComponentFixture;

namespace {
ars_source_desc descriptor(ars_utf8_view *ids,std::uint32_t count=1) {
    ars_source_desc desc{}; desc.struct_size=sizeof(desc); desc.abi_version=ARS_ABI_VERSION;
    desc.allowed_channel_ids=ids; desc.allowed_channel_count=count; desc.max_channels_per_frame=1;
    desc.max_frames_per_second=240; desc.lease_ttl_ns=ars::security::max_ttl; return desc;
}
void pop(test::Authority &a,ars_source_admin_h source) {
    ars::core::FrameLease frame;
    test::status(ARS_OK,a.router.dequeue_frame(a.runtime,source,&frame),"B2-owned-frame-pop");
}
struct RecursiveCapture {
    test::Authority *owner=nullptr;
    ars_ingress_cap cap{};
    ars_status nested=ARS_OK;
    bool throw_error=false;
    static std::uint64_t ARS_CALL read(void *user) {
        auto &state=*static_cast<RecursiveCapture *>(user);
        if(state.throw_error) throw 42;
        test::Frame nested; nested.value.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;
        state.nested=state.owner->router.submit_frame(state.owner->runtime,state.cap,&nested.value);
        return 0; // Capture zero never renews or backdates security authority.
    }
};
}

int main() {
    ars::core::RuntimeRouter router;
    ars_runtime_desc runtime_desc{}; runtime_desc.struct_size=sizeof(runtime_desc); runtime_desc.abi_version=ARS_ABI_VERSION;
    ars_runtime_h runtime{}; ars_character_h character{};
    test::status(ARS_OK,router.create(runtime_desc,&runtime),"B2-real-runtime-create");
    std::array<ars::security::SemanticChannel,1> catalog{};
    std::memcpy(catalog[0].id.data(),"mouth.open",11);
    test::status(ARS_OK,router.bind_character(runtime,catalog,&character),"B2-validated-private-CPU-binding");
    test::check(character.id.runtime_uid==runtime.id.runtime_uid,"B2-binding-owned-by-real-runtime");
    test::status(ARS_OK,router.begin_close(runtime),"B2-close");
    test::status(ARS_OK,router.finish_close(runtime),"B2-finish");
    {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{};
        test::status(ARS_OK,a.register_source(&source,&cap),"GRANT-register-real-authority");
        test::Frame frame;
        test::status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"ADMIT-first-sequence");
        test::status(ARS_E_INVALID_FRAME,a.router.submit_frame(a.runtime,cap,&frame.value),"ADMIT-duplicate-sequence");
        frame.value.sequence=0;
        test::status(ARS_E_INVALID_FRAME,a.router.submit_frame(a.runtime,cap,&frame.value),"ADMIT-zero-sequence");
        frame.value.sequence=2; frame.channel.semantic_id={"head.yaw_rad",12};
        test::status(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"GRANT-channel-not-in-immutable-scope");
        frame.channel.semantic_id={"mouth.open",10};
        frame.value.channel_count=257;
        test::status(ARS_E_INVALID_ARGUMENT,a.router.submit_frame(a.runtime,cap,&frame.value),"FRAME-count-overflow-before-read");
        frame.value.channel_count=1; frame.value.channels=nullptr;
        test::status(ARS_E_INVALID_ARGUMENT,a.router.submit_frame(a.runtime,cap,&frame.value),"FRAME-null-with-count");
        frame.value.channels=&frame.channel; frame.value.struct_size=8;
        test::status(ARS_E_INVALID_ARGUMENT,a.router.submit_frame(a.runtime,cap,&frame.value),"FRAME-short-prefix-before-tail");
        frame.value.struct_size=sizeof(frame.value); frame.value.reserved_flags=1;
        test::status(ARS_E_INVALID_ARGUMENT,a.router.submit_frame(a.runtime,cap,&frame.value),"FRAME-reserved-flags");
        frame.value.reserved_flags=0;
        const std::array<ars_motion_channel,2> duplicates{frame.channel,frame.channel};
        frame.value.channels=duplicates.data(); frame.value.channel_count=2;
        test::status(ARS_E_INVALID_FRAME,a.router.submit_frame(a.runtime,cap,&frame.value),"FRAME-duplicate-ID-before-queue");
        frame.value.channels=&frame.channel; frame.value.channel_count=1;
        frame.channel.value[0]=std::numeric_limits<float>::quiet_NaN();
        test::status(ARS_E_INVALID_FRAME,a.router.submit_frame(a.runtime,cap,&frame.value),"FRAME-NaN-before-queue");
        frame.channel.value[0]=0.7F; frame.channel.kind=ARS_VALUE_VEC3;
        test::status(ARS_E_INVALID_FRAME,a.router.submit_frame(a.runtime,cap,&frame.value),"FRAME-kind-must-match-catalog");
        frame.channel.kind=ARS_VALUE_SCALAR;
        auto wrong=cap; wrong.hi^=UINT64_C(1)<<60;
        test::status(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,wrong,&frame.value),"CAP-bit-flip-denied");
        test::status(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,{},&frame.value),"CAP-zero-denied");
        frame.value.sequence=std::numeric_limits<std::uint64_t>::max();
        test::status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"SEQ-last-uint64-value");
        frame.value.sequence=1;
        test::status(ARS_E_INVALID_FRAME,a.router.submit_frame(a.runtime,cap,&frame.value),"SEQ-no-wrap");
        ars_ingress_cap next{};
        test::status(ARS_OK,a.router.rotate_source(a.runtime,source,&next),"ROTATE-fresh-token");
        test::check(!ars::security::same(cap,next),"ROTATE-nonreuse");
        test::status(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"ROTATE-old-token-permanently-denied");
        test::status(ARS_OK,a.router.submit_frame(a.runtime,next,&frame.value),"ROTATE-new-sequence-domain");
        test::status(ARS_OK,a.router.revoke_source(a.runtime,source),"REVOKE-explicit");
        test::status(ARS_OK,a.router.revoke_source(a.runtime,source),"REVOKE-idempotent");
        test::status(ARS_E_REVOKED,a.router.rotate_source(a.runtime,source,&next),"REVOKE-terminal-no-rotate-recovery");
        test::check(!ars::security::nonzero(next),"REVOKE-failed-rotation-zero-output");
        test::status(ARS_OK,a.router.release_source(a.runtime,source),"ADMIN-release");
        test::status(ARS_E_STALE_HANDLE,a.router.revoke_source(a.runtime,source),"ADMIN-stale-after-release");
    }
    {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{};
        char name[]="mouth.open",label[]="camera"; ars_utf8_view allowed{name,10}; auto desc=descriptor(&allowed);
        desc.diagnostic_label={label,6};
        test::status(ARS_OK,a.router.register_source(a.runtime,a.character,&desc,&source,&cap),"GRANT-deep-copy");
        name[0]='X'; label[0]='X'; test::Frame frame;
        test::status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"GRANT-input-mutation-cannot-widen-or-destroy-scope");
        auto &record=ComponentFixture::source(*a.context,source);
        test::check(record.label[0]=='c',"GRANT-label-owned-not-authority");
        test::status(ARS_OK,a.router.release_source(a.runtime,source),"GRANT-copied-cleanup");
        for(const char *invalid:{"Mouth.open","mouth.*","mouth\xc3\xa9"}) {
            allowed={invalid,static_cast<std::uint32_t>(std::strlen(invalid))}; desc=descriptor(&allowed);
            test::status(ARS_E_INVALID_ARGUMENT,a.router.register_source(a.runtime,a.character,&desc,&source,&cap),"GRANT-noncanonical-ID");
            test::check(!source.id.runtime_uid && !ars::security::nonzero(cap),"GRANT-failure-zero-outputs");
        }
        const char embedded[]={'m','o','u','t','h','\0','x'}; allowed={embedded,sizeof(embedded)}; desc=descriptor(&allowed);
        test::status(ARS_E_INVALID_ARGUMENT,a.router.register_source(a.runtime,a.character,&desc,&source,&cap),"GRANT-embedded-NUL");
        desc=descriptor(nullptr,0);
        test::status(ARS_OK,a.router.register_source(a.runtime,a.character,&desc,&source,&cap),"GRANT-empty-deny-all-registration");
        test::status(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"GRANT-empty-is-never-wildcard");
    }
    {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{};
        test::status(ARS_OK,a.register_source(&source,&cap),"QUEUE-register"); test::Frame frame;
        for(unsigned i=1;i<=16;++i) { frame.value.sequence=i; test::status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"QUEUE-fill"); }
        auto &record=ComponentFixture::source(*a.context,source); const auto credit=record.credit;
        frame.value.sequence=17;
        test::status(ARS_E_QUEUE_FULL,a.router.submit_frame(a.runtime,cap,&frame.value),"QUEUE-full-no-overwrite");
        test::check(record.sequence==16 && record.credit==credit,"QUEUE-failure-no-sequence-or-quota-charge");
        pop(a,source);
        test::status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"QUEUE-same-sequence-retry"); pop(a,source);
        frame.value.sequence=18; ComponentFixture::append_failure(*a.context,source,true);
        const auto before=record.credit;
        test::status(ARS_E_OUT_OF_BUDGET,a.router.submit_frame(a.runtime,cap,&frame.value),"QUEUE-append-OOM");
        test::check(record.sequence==17 && record.credit==before,"QUEUE-OOM-transaction-rollback");
        ComponentFixture::append_failure(*a.context,source,false);
        test::status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"QUEUE-OOM-same-sequence-retry");
        ComponentFixture::workspace_limit(*a.context,0); frame.value.sequence=19;
        test::status(ARS_E_BUSY,a.router.submit_frame(a.runtime,cap,&frame.value),"QUEUE-preparation-budget");
        ComponentFixture::workspace_limit(*a.context,16);
    }
    {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{};
        test::status(ARS_OK,a.register_source(&source,&cap,1),"RATE-one-frame-per-second"); test::Frame frame;
        test::status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"RATE-initial-burst"); frame.value.sequence=2;
        test::status(ARS_E_RATE_LIMIT,a.router.submit_frame(a.runtime,cap,&frame.value),"RATE-no-credit");
        auto &record=ComponentFixture::source(*a.context,source);
        test::check(record.sequence==1 && record.credit==0,"RATE-denial-no-charge");
        a.time.now+=1000000000ULL;
        test::status(ARS_OK,a.router.submit_frame(a.runtime,cap,&frame.value),"RATE-refill-same-sequence");
    }
    {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{},next{};
        test::status(ARS_OK,a.register_source(&source,&cap,240,100),"TTL-register");
        a.time.now=200; test::Frame frame;
        test::status(ARS_E_SOURCE_EXPIRED,a.router.submit_frame(a.runtime,cap,&frame.value),"TTL-exclusive-expiry-boundary");
        a.time.now=100;
        test::status(ARS_E_CLOCK_REGRESSION,a.router.rotate_source(a.runtime,source,&next),"TTL-backwards-time-not-recovery");
        a.time.now=300;
        test::status(ARS_E_SECURITY_CLOCK_FAULT,a.router.rotate_source(a.runtime,source,&next),"CLOCK-sticky-runtime-poison");
        test::status(ARS_OK,a.router.release_source(a.runtime,source),"TTL-terminal-admin-can-release");
    }
    {
        test::Authority a; ars_source_admin_h source{},other{}; ars_ingress_cap cap{},next{};
        a.time.now=std::numeric_limits<std::uint64_t>::max()-10;
        test::status(ARS_E_INVALID_ARGUMENT,a.register_source(&source,&cap,240,11),"TTL-checked-add-overflow");
        test::check(!source.id.runtime_uid && !ars::security::nonzero(cap),"TTL-overflow-zero-outputs");
        test::status(ARS_OK,a.register_source(&other,&next,240,10),"TTL-rollback-releases-capacity");
    }
    {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{},next{};
        a.random.fail=true;
        test::status(ARS_E_INTERNAL,a.register_source(&source,&cap),"RNG-register-failure");
        a.random.fail=false;
        test::status(ARS_OK,a.register_source(&source,&cap),"RNG-registration-rollback-recovery");
        a.random.fail=true;
        test::status(ARS_E_INTERNAL,a.router.rotate_source(a.runtime,source,&next),"RNG-fail-after-revoke");
        test::Frame frame;
        test::status(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"RNG-old-token-never-restored");
        a.random.fail=false;
        test::status(ARS_OK,a.router.rotate_source(a.runtime,source,&next),"RNG-retry-only-ROTATE_FAILED");
    }
    {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{},next{};
        test::status(ARS_OK,a.register_source(&source,&cap),"LEDGER-register");
        a.random.constant=true; a.random.next=cap.lo; const auto calls=a.random.calls.load();
        test::status(ARS_E_HANDLE_EXHAUSTED,a.router.rotate_source(a.runtime,source,&next),"LEDGER-retired-token-collision");
        test::check(a.random.calls==calls+8,"LEDGER-bounded-collision-retries");
        ComponentFixture::ledger_limit(*a.context,ComponentFixture::issued(*a.context));
        test::status(ARS_E_HANDLE_EXHAUSTED,a.router.rotate_source(a.runtime,source,&next),"LEDGER-lifetime-budget-no-recycle");
    }
    for(unsigned counter=0;counter<4;++counter) {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{},next{};
        test::status(ARS_OK,a.register_source(&source,&cap),"EXHAUST-register");
        ComponentFixture::force_counter(*a.context,source,counter); test::Frame frame;
        if(counter==3) test::status(ARS_E_HANDLE_EXHAUSTED,a.router.submit_frame(a.runtime,cap,&frame.value),"EXHAUST-accepted-ordinal");
        else test::status(ARS_E_HANDLE_EXHAUSTED,a.router.rotate_source(a.runtime,source,&next),"EXHAUST-epoch-control-ticket");
        test::status(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"EXHAUST-no-stale-authority");
        test::check(ComponentFixture::source(*a.context,source).tombstone.dirty.load(std::memory_order_acquire),"EXHAUST-release-tombstone-required");
        test::status(ARS_E_HANDLE_EXHAUSTED,a.router.rotate_source(a.runtime,source,&next),"EXHAUST-terminal-no-assert-or-wrap");
    }
    {
        test::Authority a; std::array<ars_source_admin_h,16> sources{}; std::array<ars_ingress_cap,16> caps{};
        for(unsigned i=0;i<16;++i) test::status(ARS_OK,a.register_source(&sources[i],&caps[i]),"LIMIT-sixteen-source-capacity");
        ars_source_admin_h overflow{}; ars_ingress_cap cap{};
        test::status(ARS_E_OUT_OF_BUDGET,a.register_source(&overflow,&cap),"LIMIT-no-active-source-eviction");
        test::status(ARS_OK,a.router.revoke_source(a.runtime,sources[0]),"LIMIT-terminal-frees-active-capacity");
        test::status(ARS_OK,a.register_source(&overflow,&cap),"LIMIT-new-registration-no-authority-reuse");
        test::check(!ars::security::same(cap,caps[0]),"LIMIT-token-nonreuse-after-capacity-release");
    }
    {
        RecursiveCapture callback; test::Authority a(RecursiveCapture::read,&callback); callback.owner=&a;
        ars_source_admin_h source{}; test::status(ARS_OK,a.register_source(&source,&callback.cap),"CALLBACK-register");
        test::Frame frame; frame.value.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;
        test::status(ARS_OK,a.router.submit_frame(a.runtime,callback.cap,&frame.value),"CALLBACK-capture-outside-authority-locks");
        test::check(callback.nested==ARS_E_BUSY,"CALLBACK-recursive-ingress-controlled-denial");
        callback.throw_error=true; frame.value.sequence=2;
        test::status(ARS_E_INTERNAL,a.router.submit_frame(a.runtime,callback.cap,&frame.value),"CALLBACK-exception-contained-no-charge");
        callback.throw_error=false;
        test::status(ARS_OK,a.router.submit_frame(a.runtime,callback.cap,&frame.value),"CALLBACK-context-guard-and-pins-restored");
    }
    {
        test::Authority a; std::array<ars_source_admin_h,64> retired{}; ars_ingress_cap cap{};
        for(auto &source:retired) {
            test::status(ARS_OK,a.register_source(&source,&cap),"RECORD-lifetime-admin-bound");
            test::status(ARS_OK,a.router.revoke_source(a.runtime,source),"RECORD-terminal-retains-admin");
        }
        ars_source_admin_h overflow{};
        test::status(ARS_E_OUT_OF_BUDGET,a.register_source(&overflow,&cap),"RECORD-no-eviction-of-64-held-admins");
        test::status(ARS_OK,a.router.release_source(a.runtime,retired[0]),"RECORD-direct-release");
        test::status(ARS_OK,a.register_source(&overflow,&cap),"RECORD-reuse-only-after-direct-pins-drain");
        test::check(!ars::security::same(retired[0].id,overflow.id),"RECORD-new-registry-generation");
    }
    {
        test::Authority a; std::array<ars_source_admin_h,16> sources{}; std::array<ars_ingress_cap,16> caps{};
        unsigned accepted=0,denied=0;
        for(unsigned i=0;i<16;++i) {
            test::status(ARS_OK,a.register_source(&sources[i],&caps[i],1),"FLOOD-sixteen-sources-rate-one");
            for(unsigned attempt=1;attempt<=100;++attempt) {
                test::Frame frame(attempt); auto result=a.router.submit_frame(a.runtime,caps[i],&frame.value);
                if(result==ARS_OK) ++accepted;
                else if(result==ARS_E_RATE_LIMIT) ++denied;
                else test::status(ARS_E_RATE_LIMIT,result,"FLOOD-unexpected-status");
            }
            test::check(ComponentFixture::queue(*a.context,sources[i]).size()==1 &&
                        ComponentFixture::source(*a.context,sources[i]).sequence==1,"FLOOD-bounded-queue-and-charge");
        }
        test::check(accepted==16 && denied==1584,"FLOOD-exact-100x-rate-accounting");
    }
    for(bool superseded_active:{false,true}) {
        test::Authority a; ars_source_admin_h source{}; ars_ingress_cap cap{},out{};
        test::status(ARS_OK,a.register_source(&source,&cap),"RESERVATION-register");
        test::Gate minted; a.random.gate=&minted; ars_status result=ARS_OK;
        std::thread worker([&]{result=a.router.rotate_source(a.runtime,source,&out);});
        minted.wait_entered(); ComponentFixture::supersede_ticket(*a.context,source,superseded_active); minted.release(); worker.join();
        test::check(result!=ARS_OK && !ars::security::nonzero(out),"RESERVATION-phase-C-exact-ticket-required");
        test::Frame frame;
        test::status(ARS_E_SCOPE_DENIED,a.router.submit_frame(a.runtime,cap,&frame.value),"RESERVATION-old-token-stays-invalid");
    }
    std::puts("PASS B2 CPU authority: full original 87 security verdicts remain HOLD");
}
