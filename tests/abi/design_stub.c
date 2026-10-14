/* NONSHIPPING G00 DESIGN SYMBOL STUB: NEVER LINK INTO PRODUCTION. */
#define ARS_BUILD_SHARED 1
#include "ars/ars.h"
#include <string.h>
#define U(x) (void)(x)
#define ZERO(p) do { if ((p)!=0) memset((p),0,sizeof(*(p))); } while(0)
ARS_API uint32_t ARS_CALL ars_get_abi_version(void){return ARS_ABI_VERSION;}
ARS_API ars_status ARS_CALL ars_runtime_create(const ars_runtime_desc *d, ars_runtime_h *o){U(d);ZERO(o);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_runtime_get_capabilities(ars_runtime_h r, ars_capabilities *o){U(r);if(o){uint32_t n=o->struct_size;memset(o,0,(n<sizeof(*o))?n:sizeof(*o));}return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_model_load(ars_runtime_h r, const ars_asset_source *d, ars_model_h *o){U(r);U(d);ZERO(o);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_model_release(ars_runtime_h r, ars_model_h m){U(r);U(m);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_character_create(ars_runtime_h r, ars_model_h m, const ars_character_desc *d, ars_character_h *o){U(r);U(m);U(d);ZERO(o);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_character_destroy(ars_runtime_h r, ars_character_h c){U(r);U(c);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_character_set_mode(ars_runtime_h r, ars_character_h c, uint32_t mode){U(r);U(c);U(mode);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_character_set_visual_layer(ars_runtime_h r, ars_character_h c, const ars_layer_change *d){U(r);U(c);U(d);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_source_register(ars_runtime_h r, ars_character_h c, const ars_source_desc *d, ars_source_admin_h *o, ars_ingress_cap *k){U(r);U(c);U(d);ZERO(o);ZERO(k);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_source_admin_release(ars_runtime_h r, ars_source_admin_h a){U(r);U(a);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_source_rotate(ars_runtime_h r, ars_source_admin_h a, ars_ingress_cap *k){U(r);U(a);ZERO(k);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_source_revoke(ars_runtime_h r, ars_source_admin_h a){U(r);U(a);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_motion_submit(ars_runtime_h r, ars_ingress_cap k, const ars_motion_frame *f){U(r);U(k);U(f);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_runtime_update(ars_runtime_h r, uint64_t t){U(r);U(t);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_runtime_render(ars_runtime_h r, const ars_render_target *d){U(r);U(d);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_runtime_begin_close(ars_runtime_h r){U(r);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_runtime_drain_gpu(ars_runtime_h r){U(r);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_runtime_query_close(ars_runtime_h r, ars_close_state *o){U(r);ZERO(o);return ARS_E_UNSUPPORTED;}
ARS_API ars_status ARS_CALL ars_runtime_finish_close(ars_runtime_h r){U(r);return ARS_E_UNSUPPORTED;}
