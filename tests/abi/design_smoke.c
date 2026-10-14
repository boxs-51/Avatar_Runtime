/* G00 COMPLETE ABI C11 symbol/link smoke; NO SDK behavior tested. */
#define ARS_USE_SHARED 1
#include "ars/ars.h"
#include <stddef.h>
_Static_assert(sizeof(ars_handle_id)==16, "by-value ID layout");
_Static_assert(sizeof(ars_runtime_h)==16, "by-value runtime layout");
_Static_assert(sizeof(ars_model_h)==16, "by-value model layout");
_Static_assert(sizeof(ars_character_h)==16, "by-value character layout");
_Static_assert(sizeof(ars_source_admin_h)==16, "by-value admin layout");
_Static_assert(sizeof(ars_ingress_cap)==16, "128 bit capability");
_Static_assert(sizeof(ars_status)==4, "fixed width status");
_Static_assert(offsetof(ars_runtime_desc,struct_size)==0, "size prefix");
_Static_assert(offsetof(ars_runtime_desc,abi_version)==4, "version prefix");
int main(void){
 ars_runtime_h r={{0}}; ars_model_h m={{0}};ars_character_h ch={{0}};
 ars_source_admin_h a={{0}};ars_ingress_cap key={0};
 ars_runtime_desc runtime={0};ars_asset_source asset={0};
 ars_character_desc character={0};ars_layer_change layer={0};ars_source_desc source={0};
 ars_motion_frame frame={0};ars_render_target target={0};
 ars_capabilities caps={0};ars_close_state close={0};
 uint32_t required=0;char buf[1]={0};
 if(ars_get_abi_version()!=ARS_ABI_VERSION)return 1;
 if(ars_runtime_create(&runtime,&r)!=ARS_E_UNSUPPORTED)return 2;
 if(ars_runtime_get_capabilities(r,&caps)!=ARS_E_UNSUPPORTED)return 3;
 if(ars_model_load(r,&asset,&m)!=ARS_E_UNSUPPORTED)return 4;
 if(ars_model_release(r,m)!=ARS_E_UNSUPPORTED)return 5;
 if(ars_character_create(r,m,&character,&ch)!=ARS_E_UNSUPPORTED)return 6;
 if(ars_character_destroy(r,ch)!=ARS_E_UNSUPPORTED)return 7;
 if(ars_character_set_mode(r,ch,ARS_MODE_2D)!=ARS_E_UNSUPPORTED)return 8;
 if(ars_character_set_visual_layer(r,ch,&layer)!=ARS_E_UNSUPPORTED)return 9;
 if(ars_source_register(r,ch,&source,&a,&key)!=ARS_E_UNSUPPORTED)return 10;
 if(ars_source_admin_release(r,a)!=ARS_E_UNSUPPORTED)return 11;
 if(ars_source_rotate(r,a,&key)!=ARS_E_UNSUPPORTED)return 12;
 if(ars_source_revoke(r,a)!=ARS_E_UNSUPPORTED)return 13;
 if(ars_motion_submit(r,key,&frame)!=ARS_E_UNSUPPORTED)return 14;
 if(ars_runtime_update(r,0)!=ARS_E_UNSUPPORTED)return 15;
 if(ars_runtime_render(r,&target)!=ARS_E_UNSUPPORTED)return 16;
 if(ars_runtime_begin_close(r)!=ARS_E_UNSUPPORTED)return 17;
 if(ars_runtime_drain_gpu(r)!=ARS_E_UNSUPPORTED)return 18;
 if(ars_runtime_query_close(r,&close)!=ARS_E_UNSUPPORTED)return 19;
 if(ars_runtime_finish_close(r)!=ARS_E_UNSUPPORTED)return 20;
 if(ars_get_last_error_info(r,buf,1,&required)!=ARS_E_UNSUPPORTED)return 21;
 return 0;
}
