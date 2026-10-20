/* Installed SDK behavioral contract, C11 source linked to the real runtime. */
#include <ars/ars.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
_Static_assert(sizeof(ars_status)==4 && sizeof(ars_handle_id)==16 && sizeof(ars_ingress_cap)==16,"ABI widths");
_Static_assert(offsetof(ars_runtime_desc,struct_size)==0 && offsetof(ars_runtime_desc,abi_version)==4,"ABI prefix");
_Static_assert(ARS_E_UNSUPPORTED==2 && ARS_E_SECURITY_CLOCK_FAULT==20,"stable statuses");
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { printf("FAIL line=%d\n",__LINE__); return 1; } } while(0)
#define INIT(v) do { memset(&(v),0,sizeof(v)); (v).struct_size=(uint32_t)sizeof(v); (v).abi_version=ARS_ABI_VERSION; } while(0)
static int iszero(const void *data,size_t size) { const unsigned char *p=(const unsigned char *)data; size_t i; for(i=0;i<size;++i)if(p[i])return 0;return 1; }
static int canary(const unsigned char *data,size_t size) { size_t i; for(i=0;i<size;++i)if(data[i]!=0xa5)return 0;return 1; }
struct allocation_state { void *pointer; uint64_t bytes; uint32_t alignment; unsigned allocations,frees,errors; int fail; };
static void *ARS_CALL host_allocate(void *user,uint64_t bytes,uint32_t alignment) {
 struct allocation_state *state=(struct allocation_state *)user;void *base;uintptr_t aligned;
 ++state->allocations;if(state->fail)return NULL;
 if(state->pointer || !alignment || (alignment&(alignment-1)) || bytes>SIZE_MAX-alignment-sizeof(void *)) { ++state->errors;return NULL; }
 base=malloc((size_t)bytes+alignment+sizeof(void *));if(!base)return NULL;
 aligned=((uintptr_t)base+sizeof(void *)+alignment-1)&~((uintptr_t)alignment-1);
 ((void **)aligned)[-1]=base;state->pointer=(void *)aligned;state->bytes=bytes;state->alignment=alignment;return state->pointer;
}
static void ARS_CALL host_free(void *user,void *pointer,uint64_t bytes,uint32_t alignment) {
 struct allocation_state *state=(struct allocation_state *)user;++state->frees;
 if(pointer!=state->pointer || bytes!=state->bytes || alignment!=state->alignment) { ++state->errors;return; }
 free(((void **)pointer)[-1]);state->pointer=NULL;
}
int main(void) {
 ars_runtime_desc desc; ars_runtime_h runtime={{0}},stale; ars_model_h model; ars_character_h character;
 ars_source_admin_h admin; ars_ingress_cap cap; ars_asset_source asset; ars_character_desc cd;
 ars_layer_change layer; ars_source_desc source; ars_motion_frame frame; ars_render_target target;
 ars_close_state close; ars_capabilities caps;
 INIT(desc);INIT(asset);INIT(cd);INIT(layer);INIT(source);INIT(frame);INIT(target);INIT(close);INIT(caps);
 CHECK(ars_get_abi_version()==ARS_ABI_VERSION);
 CHECK(ars_runtime_create(&desc,&runtime)==ARS_OK && runtime.id.runtime_uid!=0);
 CHECK(ars_runtime_get_capabilities(runtime,&caps)==ARS_OK && caps.feature_bits==0 && caps.max_active_sources==0 && caps.max_texture_edge==0);
 /* Every remaining feature-off operation is actually resolved and called. */
 memset(&model,0xa5,sizeof(model));CHECK(ars_model_load(runtime,&asset,&model)==ARS_E_UNSUPPORTED && iszero(&model,sizeof(model)));
 CHECK(ars_model_release(runtime,model)==ARS_E_UNSUPPORTED);
 memset(&character,0xa5,sizeof(character));CHECK(ars_character_create(runtime,model,&cd,&character)==ARS_E_UNSUPPORTED && iszero(&character,sizeof(character)));
 CHECK(ars_character_destroy(runtime,character)==ARS_E_UNSUPPORTED);
 CHECK(ars_character_set_mode(runtime,character,ARS_MODE_AUTO)==ARS_E_UNSUPPORTED);
 CHECK(ars_character_set_visual_layer(runtime,character,&layer)==ARS_E_UNSUPPORTED);
 memset(&admin,0xa5,sizeof(admin));memset(&cap,0xa5,sizeof(cap));
 CHECK(ars_source_register(runtime,character,&source,&admin,&cap)==ARS_E_UNSUPPORTED && iszero(&admin,sizeof(admin)) && iszero(&cap,sizeof(cap)));
 CHECK(ars_source_admin_release(runtime,admin)==ARS_E_UNSUPPORTED);
 memset(&cap,0xa5,sizeof(cap));CHECK(ars_source_rotate(runtime,admin,&cap)==ARS_E_UNSUPPORTED && iszero(&cap,sizeof(cap)));
 CHECK(ars_source_revoke(runtime,admin)==ARS_E_UNSUPPORTED);
 CHECK(ars_motion_submit(runtime,cap,&frame)==ARS_E_UNSUPPORTED);
 CHECK(ars_runtime_update(runtime,0)==ARS_E_UNSUPPORTED);
 CHECK(ars_runtime_render(runtime,&target)==ARS_E_UNSUPPORTED);
 CHECK(ars_runtime_drain_gpu(runtime)==ARS_E_UNSUPPORTED);
 stale=runtime;
 /* A truly short caller object cannot be read/written beyond its prefix. */
 {
  _Alignas(8) uint32_t short_record[2]={8,ARS_ABI_VERSION}; uint32_t before[2];memcpy(before,short_record,sizeof(before));
  CHECK(ars_runtime_create((const ars_runtime_desc *)(const void *)short_record,&runtime)==ARS_E_INVALID_ARGUMENT && iszero(&runtime,sizeof(runtime)));
  CHECK(ars_runtime_get_capabilities((ars_runtime_h){{0}},(ars_capabilities *)(void *)short_record)==ARS_E_INVALID_ARGUMENT);
  CHECK(ars_runtime_query_close((ars_runtime_h){{0}},(ars_close_state *)(void *)short_record)==ARS_E_INVALID_ARGUMENT);
  CHECK(ars_model_load(runtime,(const ars_asset_source *)(const void *)short_record,&model)==ARS_E_INVALID_ARGUMENT);
  CHECK(ars_character_create(runtime,model,(const ars_character_desc *)(const void *)short_record,&character)==ARS_E_INVALID_ARGUMENT);
  CHECK(ars_character_set_visual_layer(runtime,character,(const ars_layer_change *)(const void *)short_record)==ARS_E_INVALID_ARGUMENT);
  CHECK(ars_source_register(runtime,character,(const ars_source_desc *)(const void *)short_record,&admin,&cap)==ARS_E_INVALID_ARGUMENT);
  CHECK(ars_motion_submit(runtime,cap,(const ars_motion_frame *)(const void *)short_record)==ARS_E_INVALID_ARGUMENT);
  CHECK(ars_runtime_render(runtime,(const ars_render_target *)(const void *)short_record)==ARS_E_INVALID_ARGUMENT);
  CHECK(memcmp(before,short_record,sizeof(before))==0);
 }
 CHECK(ars_runtime_begin_close(stale)==ARS_OK);
 INIT(close);CHECK(ars_runtime_query_close(stale,&close)==ARS_OK && close.state==ARS_FENCE_COMPLETE && close.outstanding_gpu_retires==0);
 CHECK(ars_runtime_finish_close(stale)==ARS_OK);
 CHECK(ars_runtime_finish_close(stale)==ARS_E_STALE_HANDLE);
 {
  struct { ars_runtime_desc value; unsigned char tail[32]; } in;
  struct { ars_capabilities value; unsigned char tail[32]; } out;
  INIT(in.value);in.value.struct_size=sizeof(in);in.value.abi_version=ARS_ABI_VERSION+19;memset(in.tail,0xa5,sizeof(in.tail));
  CHECK(ars_runtime_create(&in.value,&runtime)==ARS_OK && canary(in.tail,sizeof(in.tail)));
  INIT(out.value);out.value.struct_size=sizeof(out);memset(out.tail,0xa5,sizeof(out.tail));
  CHECK(ars_runtime_get_capabilities(runtime,&out.value)==ARS_OK && out.value.struct_size==sizeof(ars_capabilities) && out.value.abi_version==ARS_ABI_VERSION && canary(out.tail,sizeof(out.tail)));
  INIT(close);CHECK(ars_runtime_query_close(runtime,&close)==ARS_OK && close.state==ARS_FENCE_PENDING);
  CHECK(ars_runtime_finish_close(runtime)==ARS_E_BUSY);
  CHECK(ars_runtime_begin_close(runtime)==ARS_OK && ars_runtime_begin_close(runtime)==ARS_E_CLOSED);
  INIT(caps);CHECK(ars_runtime_get_capabilities(runtime,&caps)==ARS_E_CLOSED && iszero(&caps,sizeof(caps)));
  CHECK(ars_runtime_finish_close(runtime)==ARS_OK);
  INIT(caps);CHECK(ars_runtime_get_capabilities(runtime,&caps)==ARS_E_STALE_HANDLE && iszero(&caps,sizeof(caps)));
 }
 INIT(desc);desc.abi_version=UINT32_C(2)<<16;memset(&runtime,0xa5,sizeof(runtime));
 CHECK(ars_runtime_create(&desc,&runtime)==ARS_E_INVALID_ARGUMENT && iszero(&runtime,sizeof(runtime)));
 INIT(desc);desc.reserved0=1;CHECK(ars_runtime_create(&desc,&runtime)==ARS_E_INVALID_ARGUMENT);
 INIT(asset);asset.reserved_flags=1;CHECK(ars_model_load(runtime,&asset,&model)==ARS_E_INVALID_ARGUMENT);
 INIT(cd);cd.reserved_flags=1;CHECK(ars_character_create(runtime,model,&cd,&character)==ARS_E_INVALID_ARGUMENT);
 INIT(layer);layer.reserved_flags=1;CHECK(ars_character_set_visual_layer(runtime,character,&layer)==ARS_E_INVALID_ARGUMENT);
 INIT(source);source.reserved0=1;CHECK(ars_source_register(runtime,character,&source,&admin,&cap)==ARS_E_INVALID_ARGUMENT);
 INIT(frame);frame.reserved_flags=1;CHECK(ars_motion_submit(runtime,cap,&frame)==ARS_E_INVALID_ARGUMENT);
 INIT(target);target.reserved0=1;CHECK(ars_runtime_render(runtime,&target)==ARS_E_INVALID_ARGUMENT);
 /* Negotiation is independent of feature enablement for every input record. */
 INIT(asset);asset.abi_version=UINT32_C(2)<<16;CHECK(ars_model_load(runtime,&asset,&model)==ARS_E_INVALID_ARGUMENT);
 INIT(cd);cd.abi_version=UINT32_C(2)<<16;CHECK(ars_character_create(runtime,model,&cd,&character)==ARS_E_INVALID_ARGUMENT);
 INIT(layer);layer.abi_version=UINT32_C(2)<<16;CHECK(ars_character_set_visual_layer(runtime,character,&layer)==ARS_E_INVALID_ARGUMENT);
 INIT(source);source.abi_version=UINT32_C(2)<<16;CHECK(ars_source_register(runtime,character,&source,&admin,&cap)==ARS_E_INVALID_ARGUMENT);
 INIT(frame);frame.abi_version=UINT32_C(2)<<16;CHECK(ars_motion_submit(runtime,cap,&frame)==ARS_E_INVALID_ARGUMENT);
 INIT(target);target.abi_version=UINT32_C(2)<<16;CHECK(ars_runtime_render(runtime,&target)==ARS_E_INVALID_ARGUMENT);
 INIT(asset);asset.abi_version+=17;CHECK(ars_model_load(runtime,&asset,&model)==ARS_E_UNSUPPORTED);
 INIT(cd);cd.abi_version+=17;CHECK(ars_character_create(runtime,model,&cd,&character)==ARS_E_UNSUPPORTED);
 INIT(layer);layer.abi_version+=17;CHECK(ars_character_set_visual_layer(runtime,character,&layer)==ARS_E_UNSUPPORTED);
 INIT(source);source.abi_version+=17;CHECK(ars_source_register(runtime,character,&source,&admin,&cap)==ARS_E_UNSUPPORTED);
 INIT(frame);frame.abi_version+=17;CHECK(ars_motion_submit(runtime,cap,&frame)==ARS_E_UNSUPPORTED);
 INIT(target);target.abi_version+=17;CHECK(ars_runtime_render(runtime,&target)==ARS_E_UNSUPPORTED);
 {
  struct allocation_state state={0};INIT(desc);desc.alloc=host_allocate;
  CHECK(ars_runtime_create(&desc,&runtime)==ARS_E_INVALID_ARGUMENT && state.allocations==0);
  desc.free_fn=host_free;desc.allocator_user_data=&state;state.fail=1;memset(&runtime,0xa5,sizeof(runtime));
  CHECK(ars_runtime_create(&desc,&runtime)==ARS_E_OUT_OF_BUDGET && iszero(&runtime,sizeof(runtime)) && state.allocations==1 && state.frees==0);
  state.fail=0;CHECK(ars_runtime_create(&desc,&runtime)==ARS_OK && state.pointer!=NULL);
  CHECK(ars_runtime_begin_close(runtime)==ARS_OK && ars_runtime_finish_close(runtime)==ARS_OK);
  CHECK(state.pointer==NULL && state.allocations==2 && state.frees==1 && state.errors==0);
 }
 {
  ars_capabilities before;INIT(caps);caps.abi_version=UINT32_C(2)<<16;before=caps;
  CHECK(ars_runtime_get_capabilities(runtime,&caps)==ARS_E_INVALID_ARGUMENT && memcmp(&caps,&before,sizeof(caps))==0);
  INIT(caps);caps.reserved_flags=1;before=caps;
  CHECK(ars_runtime_get_capabilities(runtime,&caps)==ARS_E_INVALID_ARGUMENT && memcmp(&caps,&before,sizeof(caps))==0);
 }
 {
  ars_close_state before;INIT(close);close.abi_version=UINT32_C(2)<<16;before=close;
  CHECK(ars_runtime_query_close(runtime,&close)==ARS_E_INVALID_ARGUMENT && memcmp(&close,&before,sizeof(close))==0);
  INIT(close);close.reserved0=1;before=close;
  CHECK(ars_runtime_query_close(runtime,&close)==ARS_E_INVALID_ARGUMENT && memcmp(&close,&before,sizeof(close))==0);
 }
 printf("INSTALLED_ABI20_C11_PASS checks=%u\n",checks);return 0;
}
