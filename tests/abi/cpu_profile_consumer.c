#define _CRT_SECURE_NO_WARNINGS
/* Installed SDK oracle: this translation unit uses only the unchanged C ABI. */
#include <ars/ars.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);exit(1);} } while(0)
struct host { uint64_t live,calls;ars_runtime_h runtime;unsigned probes;int probing; };
static void probe(struct host *h) {
    if(!h->runtime.id.runtime_uid || h->probing)return;
    h->probing=1;
    { ars_status s=ars_runtime_begin_close(h->runtime);CHECK(s==ARS_E_BUSY || s==ARS_E_STALE_HANDLE); }
    { ars_status s=ars_runtime_update(h->runtime,1);CHECK(s==ARS_E_BUSY || s==ARS_E_STALE_HANDLE); }
    { ars_model_h m={{0,0,0}};ars_status s=ars_model_release(h->runtime,m);CHECK(s==ARS_E_BUSY || s==ARS_E_STALE_HANDLE); }
    { ars_status s=ars_runtime_finish_close(h->runtime);CHECK(s==ARS_E_BUSY || s==ARS_E_STALE_HANDLE); }
    ++h->probes;h->probing=0;
}
static void *ARS_CALL allocate(void *u,uint64_t n,uint32_t alignment) {
    struct host *h=(struct host *)u;void *p;probe(h);++h->calls;
    p=_aligned_malloc((size_t)(n?n:1),alignment<sizeof(void*)?sizeof(void*):alignment);if(p)h->live+=n;return p;
}
static void ARS_CALL release(void *u,void *p,uint64_t n,uint32_t alignment) {
    struct host *h=(struct host *)u;(void)alignment;probe(h);CHECK(h->live>=n);h->live-=n;_aligned_free(p);
}
int main(int argc,char **argv) {
    FILE *f;long size;unsigned char *bytes;struct host host={0};
    ars_runtime_desc desc={0};ars_runtime_h runtime={{0,0,0}};ars_model_h model={{0,0,0}};ars_character_h character={{0,0,0}};
    ars_asset_source asset={0};ars_character_desc instance={0};ars_source_desc source={0};ars_source_admin_h admin={{0,0,0}};ars_ingress_cap cap={0,0};
    ars_motion_channel channel={{"mouth.open",10},ARS_VALUE_SCALAR,0,{0.75f,0,0,0}};ars_motion_frame frame={0};
    ars_utf8_view allowed={"mouth.open",10};ars_capabilities caps={0};ars_close_state close={0};
    if(argc!=2 && argc!=3)return 2;
    f=fopen(argv[1],"rb");CHECK(f!=NULL);CHECK(fseek(f,0,SEEK_END)==0);size=ftell(f);CHECK(size>0);rewind(f);
    bytes=(unsigned char*)malloc((size_t)size);CHECK(bytes!=NULL);CHECK(fread(bytes,1,(size_t)size,f)==(size_t)size);fclose(f);
    desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;desc.alloc=allocate;desc.free_fn=release;desc.allocator_user_data=&host;
    CHECK(ars_get_abi_version()==ARS_ABI_VERSION);CHECK(ars_runtime_create(&desc,&runtime)==ARS_OK);host.runtime=runtime;
    caps.struct_size=sizeof(caps);caps.abi_version=ARS_ABI_VERSION;CHECK(ars_runtime_get_capabilities(runtime,&caps)==ARS_OK);
    CHECK(caps.feature_bits==ARS_CAP_HOST_SCOPED_MOTION_INGRESS && caps.max_channels_per_frame==256 && caps.max_active_sources==16);
    asset.struct_size=sizeof(asset);asset.abi_version=ARS_ABI_VERSION;asset.kind=ARS_ASSET_MEMORY_BUNDLE;asset.bytes=bytes;asset.byte_count=(uint64_t)size;
    CHECK(ars_model_load(runtime,&asset,&model)==ARS_OK);memset(bytes,0,(size_t)size);free(bytes);asset.bytes=NULL;
    instance.struct_size=sizeof(instance);instance.abi_version=ARS_ABI_VERSION;instance.initial_mode=ARS_MODE_AUTO;
    CHECK(ars_character_create(runtime,model,&instance,&character)==ARS_OK);CHECK(ars_model_release(runtime,model)==ARS_OK);CHECK(ars_model_release(runtime,model)==ARS_E_STALE_HANDLE);
    if(argc==3){CHECK(ars_runtime_update(runtime,1)==ARS_OK);goto destroy_character;}
    source.struct_size=sizeof(source);source.abi_version=ARS_ABI_VERSION;source.allowed_channel_ids=&allowed;source.allowed_channel_count=1;
    source.max_channels_per_frame=1;source.max_frames_per_second=240;source.lease_ttl_ns=UINT64_C(3600000000000);
    CHECK(ars_source_register(runtime,character,&source,&admin,&cap)==ARS_OK);
    frame.struct_size=sizeof(frame);frame.abi_version=ARS_ABI_VERSION;frame.sequence=1;frame.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;
    frame.channel_count=1;frame.channels=&channel;frame.confidence=1;
    CHECK(ars_motion_submit(runtime,cap,&frame)==ARS_OK);CHECK(ars_motion_submit(runtime,cap,&frame)==ARS_E_INVALID_FRAME);
    CHECK(ars_runtime_update(runtime,1)==ARS_OK);CHECK(ars_runtime_update(runtime,2)==ARS_OK);
    CHECK(ars_source_rotate(runtime,admin,&cap)==ARS_OK);frame.sequence=1;CHECK(ars_motion_submit(runtime,cap,&frame)==ARS_OK);CHECK(ars_runtime_update(runtime,3)==ARS_OK);
    CHECK(ars_source_revoke(runtime,admin)==ARS_OK);CHECK(ars_motion_submit(runtime,cap,&frame)==ARS_E_SCOPE_DENIED);CHECK(ars_runtime_update(runtime,4)==ARS_OK);
    CHECK(ars_character_destroy(runtime,character)==ARS_OK);CHECK(ars_source_admin_release(runtime,admin)==ARS_OK);goto after_destroy;
    destroy_character: CHECK(ars_character_destroy(runtime,character)==ARS_OK);
    after_destroy:;
    CHECK(ars_character_destroy(runtime,character)==ARS_E_STALE_HANDLE);
    CHECK(ars_runtime_begin_close(runtime)==ARS_OK);close.struct_size=sizeof(close);close.abi_version=ARS_ABI_VERSION;CHECK(ars_runtime_query_close(runtime,&close)==ARS_OK);
    CHECK(close.state==ARS_FENCE_COMPLETE && close.outstanding_gpu_retires==0);CHECK(ars_runtime_finish_close(runtime)==ARS_OK);
    CHECK(host.live==0 && host.probes>0);printf("PASS public CPU C11/C++20 lifecycle alloc=%llu callback_probes=%u\n",(unsigned long long)host.calls,host.probes);return 0;
}
