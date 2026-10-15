#include "ars/ars.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures;
static void expect(const char *id, ars_status expected, ars_status actual) {
    ++checks; if (expected != actual) ++failures;
    printf("%s %s expected=%d actual=%d\n", expected==actual?"PASS":"FAIL",id,(int)expected,(int)actual);
}
static void zero(const char *id,const void *p,size_t size) {
    const unsigned char *b=(const unsigned char *)p; size_t i; int nonzero=0;
    for(i=0;i<size;++i) nonzero |= b[i];
    expect(id,0,nonzero?1:0);
}
#define INIT(x) do { memset(&(x),0,sizeof(x)); (x).struct_size=(uint32_t)sizeof(x); (x).abi_version=ARS_ABI_VERSION; } while(0)
static void *ARS_CALL allocate(void *u,uint64_t n,uint32_t a) { (void)u; (void)a; return malloc((size_t)n); }

int main(void) {
    ars_runtime_desc desc; ars_runtime_h runtime={{0}},stale,invalid={{0}};
    ars_capabilities caps; ars_close_state close_state;
    INIT(desc); INIT(caps); INIT(close_state);
    expect("ABI-version",(ars_status)ARS_ABI_VERSION,(ars_status)ars_get_abi_version());
    expect("B1-create",ARS_OK,ars_runtime_create(&desc,&runtime));
    if(failures) { puts("RED: live CPU runtime is missing; do not treat foundation UNSUPPORTED as OK"); return EXIT_FAILURE; }
    expect("B1-nonzero-UID",1,runtime.id.runtime_uid?1:0);
    stale=runtime;
    expect("B1-caps",ARS_OK,ars_runtime_get_capabilities(runtime,&caps));
    expect("B1-no-features",0,caps.feature_bits?1:0);
    expect("B1-no-motion-limits",0,(ars_status)caps.max_active_sources);
    expect("B1-finish-before-begin",ARS_E_BUSY,ars_runtime_finish_close(runtime));
    expect("B1-query-open",ARS_OK,ars_runtime_query_close(runtime,&close_state));
    expect("B1-open-fence-pending",ARS_FENCE_PENDING,(ars_status)close_state.state);
    expect("B1-own-query-counted",1,(ars_status)close_state.outstanding_close_management_pins);
    expect("B1-own-query-total",1,(ars_status)close_state.outstanding_pins);
    {
        ars_runtime_h forged=runtime;
        forged.id.slot=UINT32_MAX;
        expect("B1-slot-bounds",ARS_E_STALE_HANDLE,ars_runtime_begin_close(forged));
        forged=runtime; forged.id.generation=0;
        expect("B1-generation",ARS_E_STALE_HANDLE,ars_runtime_begin_close(forged));
    }
    expect("B1-begin",ARS_OK,ars_runtime_begin_close(runtime));
    expect("B1-repeat-begin",ARS_E_CLOSED,ars_runtime_begin_close(runtime));
    INIT(caps);
    expect("B1-closed-work",ARS_E_CLOSED,ars_runtime_get_capabilities(runtime,&caps));
    zero("B1-closed-output-zero",&caps,sizeof(caps));
    INIT(close_state);
    expect("B1-query-closing",ARS_OK,ars_runtime_query_close(runtime,&close_state));
    expect("B1-CPU-fence-complete",ARS_FENCE_COMPLETE,(ars_status)close_state.state);
    expect("B1-no-GPU-retire",0,(ars_status)close_state.outstanding_gpu_retires);
    expect("B1-drain-still-unsupported",ARS_E_UNSUPPORTED,ars_runtime_drain_gpu(runtime));
    expect("B1-finish",ARS_OK,ars_runtime_finish_close(runtime));
    expect("B1-repeat-finish-stale",ARS_E_STALE_HANDLE,ars_runtime_finish_close(stale));
    INIT(close_state);
    expect("B1-query-stale",ARS_E_STALE_HANDLE,ars_runtime_query_close(stale,&close_state));
    zero("B1-stale-query-zero",&close_state,sizeof(close_state));
    expect("B1-zero-ID",ARS_E_INVALID_ARGUMENT,ars_runtime_begin_close(invalid));
    memset(&runtime,0xa5,sizeof(runtime));
    expect("B1-null-desc",ARS_E_INVALID_ARGUMENT,ars_runtime_create(NULL,&runtime));
    zero("B1-create-error-zero",&runtime,sizeof(runtime));
    INIT(desc); desc.alloc=allocate;
    expect("B1-unpaired-allocator",ARS_E_INVALID_ARGUMENT,ars_runtime_create(&desc,&runtime));
    zero("B1-unpaired-zero",&runtime,sizeof(runtime));
    INIT(desc); desc.reserved0=1;
    expect("B1-reserved",ARS_E_INVALID_ARGUMENT,ars_runtime_create(&desc,&runtime));
    INIT(desc); desc.abi_version=UINT32_C(2)<<16;
    expect("B1-wrong-major",ARS_E_INVALID_ARGUMENT,ars_runtime_create(&desc,&runtime));
    {
        uint32_t *prefix=(uint32_t *)malloc(8); uint32_t before[2]={8,ARS_ABI_VERSION};
        if(!prefix) return EXIT_FAILURE;
        memcpy(prefix,before,8);
        expect("B1-short-desc",ARS_E_INVALID_ARGUMENT,ars_runtime_create((const ars_runtime_desc *)(void *)prefix,&runtime));
        expect("B1-short-query",ARS_E_INVALID_ARGUMENT,ars_runtime_query_close(stale,(ars_close_state *)(void *)prefix));
        expect("B1-short-caps",ARS_E_INVALID_ARGUMENT,ars_runtime_get_capabilities(stale,(ars_capabilities *)(void *)prefix));
        expect("B1-short-no-write",0,memcmp(prefix,before,8)?1:0);
        free(prefix);
    }
    {
        struct { ars_runtime_desc desc; unsigned char suffix[16]; } longer;
        struct { ars_capabilities caps; unsigned char suffix[16]; } out;
        INIT(longer.desc); longer.desc.struct_size=(uint32_t)sizeof(longer); longer.desc.abi_version=ARS_ABI_VERSION+7;
        memset(longer.suffix,0xa5,sizeof(longer.suffix));
        expect("B1-higher-minor-create",ARS_OK,ars_runtime_create(&longer.desc,&runtime));
        INIT(out.caps); out.caps.struct_size=(uint32_t)sizeof(out); memset(out.suffix,0xa5,sizeof(out.suffix));
        expect("B1-long-caps",ARS_OK,ars_runtime_get_capabilities(runtime,&out.caps));
        expect("B1-output-prefix",(ars_status)sizeof(ars_capabilities),(ars_status)out.caps.struct_size);
        { size_t i; for(i=0;i<sizeof(out.suffix);++i) expect("B1-output-canary",0xa5,out.suffix[i]); }
        expect("B1-second-begin",ARS_OK,ars_runtime_begin_close(runtime));
        expect("B1-second-finish",ARS_OK,ars_runtime_finish_close(runtime));
    }
    {
        ars_source_desc source; ars_source_admin_h admin; ars_ingress_cap cap;
        ars_model_h model; ars_character_h character; ars_character_desc cd; ars_asset_source asset;
        INIT(source); INIT(cd); INIT(asset);
        memset(&admin,0xa5,sizeof(admin)); memset(&cap,0xa5,sizeof(cap));
        expect("B1-source-not-enabled",ARS_E_UNSUPPORTED,ars_source_register(stale,(ars_character_h){{0}},&source,&admin,&cap));
        zero("B1-no-admin",&admin,sizeof(admin)); zero("B1-no-cap",&cap,sizeof(cap));
        expect("B1-no-model-loader",ARS_E_UNSUPPORTED,ars_model_load(stale,&asset,&model));
        expect("B1-no-public-fixture",ARS_E_UNSUPPORTED,ars_character_create(stale,model,&cd,&character));
    }
    printf("B1 C ABI checks=%u failures=%u; CPU lifecycle only\n",checks,failures);
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
