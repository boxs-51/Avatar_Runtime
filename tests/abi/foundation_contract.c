/* Foundation behavior, not runtime security or graphics validation. */
#include "ars/ars.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures, callbacks;
static ars_runtime_h runtime_id;
static ars_model_h model_id;
static ars_character_h character_id;
static ars_source_admin_h admin_id;

static void status(const char *id, ars_status expected, ars_status actual) {
    ++checks;
    if (expected != actual) ++failures;
    printf("%s %s expected=%d actual=%d\n", expected == actual ? "PASS" : "FAIL",
           id, (int)expected, (int)actual);
}
static void bytes(const char *id, const void *ptr, size_t count, unsigned char value) {
    const unsigned char *p = (const unsigned char *)ptr;
    size_t i;
    int ok = 1;
    for (i = 0; i < count; ++i) if (p[i] != value) ok = 0;
    ++checks;
    if (!ok) ++failures;
    printf("%s %s bytes=%zu expected=%u\n", ok ? "PASS" : "FAIL", id, count, (unsigned)value);
}
#define INIT(x) do { memset(&(x), 0, sizeof(x)); (x).struct_size = (uint32_t)sizeof(x); (x).abi_version = ARS_ABI_VERSION; } while (0)
#define ZEROED(id,x) bytes(id, &(x), sizeof(x), 0)

static void *ARS_CALL host_alloc(void *u, uint64_t n, uint32_t a) {
    (void)u; (void)n; (void)a; ++callbacks; return NULL;
}
static void ARS_CALL host_free(void *u, void *p, uint64_t n, uint32_t a) {
    (void)u; (void)p; (void)n; (void)a; ++callbacks;
}
static uint64_t ARS_CALL host_clock(void *u) { (void)u; ++callbacks; return 0; }

static void all_symbols(void) {
    ars_runtime_desc runtime;
    ars_asset_source asset;
    ars_character_desc character;
    ars_layer_change layer;
    ars_source_desc source;
    ars_motion_frame frame;
    ars_render_target target;
    ars_capabilities caps;
    ars_close_state close_state;
    ars_ingress_cap key;
    INIT(runtime); INIT(asset); INIT(character); INIT(layer);
    INIT(source); INIT(frame); INIT(target); INIT(caps); INIT(close_state);
    runtime.alloc = host_alloc; runtime.free_fn = host_free;
    runtime.simulation_now_ns = host_clock;
    /* Accessible nested objects with impossible semantic lengths: never consume. */
    asset.bytes = (const uint8_t *)&runtime; asset.byte_count = UINT64_MAX;
    source.allowed_channel_ids = &source.diagnostic_label; source.allowed_channel_count = UINT32_MAX;
    frame.channels = NULL; frame.channel_count = UINT32_MAX;
    status("ABI-01-version", (ars_status)ARS_ABI_VERSION, (ars_status)ars_get_abi_version());
    memset(&runtime_id, 0xa5, sizeof(runtime_id));
    status("ABI-02-create", ARS_E_UNSUPPORTED, ars_runtime_create(&runtime, &runtime_id));
    ZEROED("OUT-01-runtime", runtime_id);
    status("ABI-02-capabilities", ARS_E_UNSUPPORTED, ars_runtime_get_capabilities(runtime_id, &caps));
    ZEROED("OUT-02-capabilities", caps);
    memset(&model_id, 0xa5, sizeof(model_id));
    status("ABI-02-load", ARS_E_UNSUPPORTED, ars_model_load(runtime_id, &asset, &model_id));
    ZEROED("OUT-01-model", model_id);
    status("ABI-02-release", ARS_E_UNSUPPORTED, ars_model_release(runtime_id, model_id));
    memset(&character_id, 0xa5, sizeof(character_id));
    status("ABI-02-character", ARS_E_UNSUPPORTED, ars_character_create(runtime_id, model_id, &character, &character_id));
    ZEROED("OUT-01-character", character_id);
    status("ABI-02-destroy", ARS_E_UNSUPPORTED, ars_character_destroy(runtime_id, character_id));
    status("ABI-02-mode", ARS_E_UNSUPPORTED, ars_character_set_mode(runtime_id, character_id, ARS_MODE_2D));
    status("ABI-02-layer", ARS_E_UNSUPPORTED, ars_character_set_visual_layer(runtime_id, character_id, &layer));
    memset(&admin_id, 0xa5, sizeof(admin_id)); memset(&key, 0xa5, sizeof(key));
    status("ABI-02-register", ARS_E_UNSUPPORTED, ars_source_register(runtime_id, character_id, &source, &admin_id, &key));
    ZEROED("OUT-01-admin", admin_id); ZEROED("OUT-01-key", key);
    status("ABI-02-admin-release", ARS_E_UNSUPPORTED, ars_source_admin_release(runtime_id, admin_id));
    memset(&key, 0xa5, sizeof(key));
    status("ABI-02-rotate", ARS_E_UNSUPPORTED, ars_source_rotate(runtime_id, admin_id, &key));
    ZEROED("OUT-01-rotated-key", key);
    status("ABI-02-revoke", ARS_E_UNSUPPORTED, ars_source_revoke(runtime_id, admin_id));
    status("ABI-02-submit", ARS_E_UNSUPPORTED, ars_motion_submit(runtime_id, key, &frame));
    status("ABI-02-update", ARS_E_UNSUPPORTED, ars_runtime_update(runtime_id, 0));
    status("ABI-02-render", ARS_E_UNSUPPORTED, ars_runtime_render(runtime_id, &target));
    status("ABI-02-begin", ARS_E_UNSUPPORTED, ars_runtime_begin_close(runtime_id));
    status("ABI-02-drain", ARS_E_UNSUPPORTED, ars_runtime_drain_gpu(runtime_id));
    status("ABI-02-query", ARS_E_UNSUPPORTED, ars_runtime_query_close(runtime_id, &close_state));
    ZEROED("OUT-03-close", close_state);
    status("ABI-02-finish", ARS_E_UNSUPPORTED, ars_runtime_finish_close(runtime_id));
    status("SIDE-01-callbacks", 0, (ars_status)callbacks);
}

/* Exercise each descriptor's bounds/version/reserved validation through the ABI. */
#define DESCRIPTOR_CASES(type, name, field, call) do { \
    struct { type value; unsigned char suffix[16]; } storage; \
    type *d = &storage.value; \
    uint32_t sizes[] = {0, 4, 8, (uint32_t)sizeof(type)-1}; \
    size_t n; char label[100]; \
    for (n=0; n<sizeof(sizes)/sizeof(sizes[0]); ++n) { \
        INIT(*d); d->struct_size=sizes[n]; \
        (void)snprintf(label,sizeof(label),"BUF-02-%s-size-%u",name,(unsigned)sizes[n]); \
        status(label,ARS_E_INVALID_ARGUMENT,(call)); \
    } \
    INIT(*d); d->abi_version=UINT32_C(2)<<16; \
    status("VER-01-" name,ARS_E_INVALID_ARGUMENT,(call)); \
    INIT(*d); d->field=1; \
    status("VER-04-" name,ARS_E_INVALID_ARGUMENT,(call)); \
    INIT(*d); status("BUF-03-" name,ARS_E_UNSUPPORTED,(call)); \
    INIT(*d); d->struct_size=(uint32_t)sizeof(storage); d->abi_version=ARS_ABI_VERSION+99; \
    memset(storage.suffix,0xa5,sizeof(storage.suffix)); \
    status("VER-03-" name,ARS_E_UNSUPPORTED,(call)); \
    bytes("BUF-05-" name,storage.suffix,sizeof(storage.suffix),0xa5); \
} while (0)

static void descriptors(void) {
    ars_runtime_h r;
    ars_model_h m;
    ars_character_h c;
    ars_source_admin_h a;
    ars_ingress_cap k = {0};
    DESCRIPTOR_CASES(ars_runtime_desc,"runtime",reserved_flags,ars_runtime_create(d,&r));
    DESCRIPTOR_CASES(ars_runtime_desc,"runtime-reserved0",reserved0,ars_runtime_create(d,&r));
    DESCRIPTOR_CASES(ars_asset_source,"asset",reserved_flags,ars_model_load(runtime_id,d,&m));
    DESCRIPTOR_CASES(ars_character_desc,"character",reserved_flags,ars_character_create(runtime_id,model_id,d,&c));
    DESCRIPTOR_CASES(ars_layer_change,"layer",reserved_flags,ars_character_set_visual_layer(runtime_id,character_id,d));
    DESCRIPTOR_CASES(ars_source_desc,"source",reserved_flags,ars_source_register(runtime_id,character_id,d,&a,&k));
    DESCRIPTOR_CASES(ars_source_desc,"source-reserved0",reserved0,ars_source_register(runtime_id,character_id,d,&a,&k));
    DESCRIPTOR_CASES(ars_motion_frame,"frame",reserved_flags,ars_motion_submit(runtime_id,k,d));
    DESCRIPTOR_CASES(ars_render_target,"target",reserved_flags,ars_runtime_render(runtime_id,d));
    DESCRIPTOR_CASES(ars_render_target,"target-reserved0",reserved0,ars_runtime_render(runtime_id,d));
    memset(&r,0xa5,sizeof(r));
    status("BUF-01-null-runtime",ARS_E_INVALID_ARGUMENT,ars_runtime_create(NULL,&r));
    ZEROED("OUT-01-invalid-runtime",r);
    memset(&m,0xa5,sizeof(m));
    status("BUF-01-null-asset",ARS_E_INVALID_ARGUMENT,ars_model_load(runtime_id,NULL,&m));
    ZEROED("OUT-01-invalid-model",m);
    memset(&c,0xa5,sizeof(c));
    status("BUF-01-null-character",ARS_E_INVALID_ARGUMENT,ars_character_create(runtime_id,model_id,NULL,&c));
    ZEROED("OUT-01-invalid-character",c);
    status("BUF-01-null-layer",ARS_E_INVALID_ARGUMENT,ars_character_set_visual_layer(runtime_id,character_id,NULL));
    status("BUF-01-null-frame",ARS_E_INVALID_ARGUMENT,ars_motion_submit(runtime_id,k,NULL));
    status("BUF-01-null-target",ARS_E_INVALID_ARGUMENT,ars_runtime_render(runtime_id,NULL));
    {
        ars_source_desc source;
        ars_runtime_desc runtime;
        ars_asset_source asset;
        ars_character_desc character;
        INIT(source); INIT(runtime); INIT(asset); INIT(character);
        memset(&a,0xa5,sizeof(a)); memset(&k,0xa5,sizeof(k));
        status("BUF-01-null-source",ARS_E_INVALID_ARGUMENT,ars_source_register(runtime_id,character_id,NULL,&a,&k));
        ZEROED("OUT-01-invalid-admin",a); ZEROED("OUT-01-invalid-key",k);
        memset(&k,0xa5,sizeof(k));
        status("BUF-01-null-admin-output",ARS_E_INVALID_ARGUMENT,ars_source_register(runtime_id,character_id,&source,NULL,&k));
        ZEROED("OUT-01-partial-key",k);
        memset(&a,0xa5,sizeof(a));
        status("BUF-01-null-key-output",ARS_E_INVALID_ARGUMENT,ars_source_register(runtime_id,character_id,&source,&a,NULL));
        ZEROED("OUT-01-partial-admin",a);
        status("BUF-01-null-runtime-output",ARS_E_INVALID_ARGUMENT,ars_runtime_create(&runtime,NULL));
        status("BUF-01-null-model-output",ARS_E_INVALID_ARGUMENT,ars_model_load(runtime_id,&asset,NULL));
        status("BUF-01-null-character-output",ARS_E_INVALID_ARGUMENT,ars_character_create(runtime_id,model_id,&character,NULL));
        status("BUF-01-null-rotate-output",ARS_E_INVALID_ARGUMENT,ars_source_rotate(runtime_id,admin_id,NULL));
    }
    /* Only an 8-byte accessible prefix exists: sanitizers detect tail reads. */
    {
        uint32_t *prefix = (uint32_t *)malloc(8);
        if (!prefix) { status("BUF-02-prefix-allocation",1,0); return; }
        prefix[0]=8; prefix[1]=ARS_ABI_VERSION;
        status("BUF-02-prefix-runtime",ARS_E_INVALID_ARGUMENT,ars_runtime_create((const ars_runtime_desc *)(const void *)prefix,&r));
        status("BUF-02-prefix-asset",ARS_E_INVALID_ARGUMENT,ars_model_load(runtime_id,(const ars_asset_source *)(const void *)prefix,&m));
        status("BUF-02-prefix-character",ARS_E_INVALID_ARGUMENT,ars_character_create(runtime_id,model_id,(const ars_character_desc *)(const void *)prefix,&c));
        status("BUF-02-prefix-layer",ARS_E_INVALID_ARGUMENT,ars_character_set_visual_layer(runtime_id,character_id,(const ars_layer_change *)(const void *)prefix));
        status("BUF-02-prefix-source",ARS_E_INVALID_ARGUMENT,ars_source_register(runtime_id,character_id,(const ars_source_desc *)(const void *)prefix,&a,&k));
        status("BUF-02-prefix-frame",ARS_E_INVALID_ARGUMENT,ars_motion_submit(runtime_id,k,(const ars_motion_frame *)(const void *)prefix));
        status("BUF-02-prefix-target",ARS_E_INVALID_ARGUMENT,ars_runtime_render(runtime_id,(const ars_render_target *)(const void *)prefix));
        free(prefix);
    }
}

#define SIZED_OUTPUT(type,name,field,call) do { \
    struct { type value; unsigned char suffix[16]; } storage, before; \
    type *out=&storage.value; size_t n; \
    uint32_t sizes[]={0,4,8,(uint32_t)sizeof(type)-1}; \
    char label[100]; \
    for(n=0;n<sizeof(sizes)/sizeof(sizes[0]);++n) { \
        memset(&storage,0xa5,sizeof(storage)); out->struct_size=sizes[n]; out->abi_version=ARS_ABI_VERSION; \
        memcpy(&before,&storage,sizeof(storage)); \
        (void)snprintf(label,sizeof(label),"BUF-02-%s-size-%u",name,(unsigned)sizes[n]); \
        status(label,ARS_E_INVALID_ARGUMENT,(call)); \
        status("BUF-05-short-no-write-" name,0,(ars_status)memcmp(&storage,&before,sizeof(storage))); \
    } \
    INIT(*out); out->abi_version=UINT32_C(2)<<16; memcpy(&before,&storage,sizeof(storage)); \
    status("VER-01-" name,ARS_E_INVALID_ARGUMENT,(call)); \
    status("BUF-05-major-no-write-" name,0,(ars_status)memcmp(&storage,&before,sizeof(storage))); \
    INIT(*out); out->field=1; memcpy(&before,&storage,sizeof(storage)); \
    status("VER-04-" name,ARS_E_INVALID_ARGUMENT,(call)); \
    status("BUF-05-reserved-no-write-" name,0,(ars_status)memcmp(&storage,&before,sizeof(storage))); \
    INIT(*out); memset(storage.suffix,0xa5,sizeof(storage.suffix)); \
    status("BUF-03-" name,ARS_E_UNSUPPORTED,(call)); \
    bytes("OUT-02-zero-" name,out,sizeof(*out),0); \
    bytes("BUF-05-exact-suffix-" name,storage.suffix,sizeof(storage.suffix),0xa5); \
    INIT(*out); out->struct_size=(uint32_t)sizeof(storage); out->abi_version=ARS_ABI_VERSION+99; \
    memset(storage.suffix,0xa5,sizeof(storage.suffix)); \
    status("BUF-04-" name,ARS_E_UNSUPPORTED,(call)); \
    bytes("OUT-03-long-zero-" name,out,sizeof(*out),0); \
    bytes("BUF-05-long-suffix-" name,storage.suffix,sizeof(storage.suffix),0xa5); \
} while(0)

int main(void) {
    all_symbols(); descriptors();
    SIZED_OUTPUT(ars_capabilities,"capabilities",reserved_flags,ars_runtime_get_capabilities(runtime_id,out));
    SIZED_OUTPUT(ars_close_state,"close",reserved0,ars_runtime_query_close(runtime_id,out));
    status("BUF-01-null-caps",ARS_E_INVALID_ARGUMENT,ars_runtime_get_capabilities(runtime_id,NULL));
    status("BUF-01-null-close",ARS_E_INVALID_ARGUMENT,ars_runtime_query_close(runtime_id,NULL));
    status("SIDE-02-final-callbacks",0,(ars_status)callbacks);
    printf("FOUNDATION CASES checks=%u failures=%u; NOT runtime/GPU/security certification\n",checks,failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
