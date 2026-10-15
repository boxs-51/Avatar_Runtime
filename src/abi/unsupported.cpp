/* Bounded build foundation only. No runtime IDs, authority, assets or GPU work. */
#include "api_validation.hpp"
#include <cstring>

using namespace ars::abi;

extern "C" {
#ifndef ARS_RUNTIME_CORE
ARS_API ars_status ARS_CALL ars_runtime_create(const ars_runtime_desc *desc, ars_runtime_h *out) {
    clear(out);
    return out && valid(desc) ? ARS_E_UNSUPPORTED : ARS_E_INVALID_ARGUMENT;
}
ARS_API ars_status ARS_CALL ars_runtime_get_capabilities(ars_runtime_h runtime, ars_capabilities *out) {
    (void)runtime;
    if (!prefix(out) || out->reserved_flags) return ARS_E_INVALID_ARGUMENT;
    clear(out);
    return ARS_E_UNSUPPORTED;
}
#endif
ARS_API ars_status ARS_CALL ars_model_load(ars_runtime_h runtime, const ars_asset_source *desc, ars_model_h *out) {
    (void)runtime; clear(out);
    return out && valid(desc) ? ARS_E_UNSUPPORTED : ARS_E_INVALID_ARGUMENT;
}
ARS_API ars_status ARS_CALL ars_model_release(ars_runtime_h runtime, ars_model_h model) {
    (void)runtime; (void)model; return ARS_E_UNSUPPORTED;
}
ARS_API ars_status ARS_CALL ars_character_create(ars_runtime_h runtime, ars_model_h model,
                                               const ars_character_desc *desc, ars_character_h *out) {
    (void)runtime; (void)model; clear(out);
    return out && valid(desc) ? ARS_E_UNSUPPORTED : ARS_E_INVALID_ARGUMENT;
}
ARS_API ars_status ARS_CALL ars_character_destroy(ars_runtime_h runtime, ars_character_h character) {
    (void)runtime; (void)character; return ARS_E_UNSUPPORTED;
}
ARS_API ars_status ARS_CALL ars_character_set_mode(ars_runtime_h runtime, ars_character_h character, uint32_t mode) {
    (void)runtime; (void)character; (void)mode; return ARS_E_UNSUPPORTED;
}
ARS_API ars_status ARS_CALL ars_character_set_visual_layer(ars_runtime_h runtime, ars_character_h character,
                                                        const ars_layer_change *desc) {
    (void)runtime; (void)character;
    return valid(desc) ? ARS_E_UNSUPPORTED : ARS_E_INVALID_ARGUMENT;
}
ARS_API ars_status ARS_CALL ars_source_register(ars_runtime_h runtime, ars_character_h character,
                                             const ars_source_desc *desc, ars_source_admin_h *out,
                                             ars_ingress_cap *cap) {
    (void)runtime; (void)character; clear(out); clear(cap);
    return out && cap && valid(desc) ? ARS_E_UNSUPPORTED : ARS_E_INVALID_ARGUMENT;
}
ARS_API ars_status ARS_CALL ars_source_admin_release(ars_runtime_h runtime, ars_source_admin_h admin) {
    (void)runtime; (void)admin; return ARS_E_UNSUPPORTED;
}
ARS_API ars_status ARS_CALL ars_source_rotate(ars_runtime_h runtime, ars_source_admin_h admin, ars_ingress_cap *out) {
    (void)runtime; (void)admin; clear(out);
    return out ? ARS_E_UNSUPPORTED : ARS_E_INVALID_ARGUMENT;
}
ARS_API ars_status ARS_CALL ars_source_revoke(ars_runtime_h runtime, ars_source_admin_h admin) {
    (void)runtime; (void)admin; return ARS_E_UNSUPPORTED;
}
ARS_API ars_status ARS_CALL ars_motion_submit(ars_runtime_h runtime, ars_ingress_cap cap, const ars_motion_frame *frame) {
    (void)runtime; (void)cap;
    return valid(frame) ? ARS_E_UNSUPPORTED : ARS_E_INVALID_ARGUMENT;
}
ARS_API ars_status ARS_CALL ars_runtime_update(ars_runtime_h runtime, uint64_t time_ns) {
    (void)runtime; (void)time_ns; return ARS_E_UNSUPPORTED;
}
ARS_API ars_status ARS_CALL ars_runtime_render(ars_runtime_h runtime, const ars_render_target *target) {
    (void)runtime;
    return valid(target) ? ARS_E_UNSUPPORTED : ARS_E_INVALID_ARGUMENT;
}
#ifndef ARS_RUNTIME_CORE
ARS_API ars_status ARS_CALL ars_runtime_begin_close(ars_runtime_h runtime) {
    (void)runtime; return ARS_E_UNSUPPORTED;
}
#endif
ARS_API ars_status ARS_CALL ars_runtime_drain_gpu(ars_runtime_h runtime) {
    (void)runtime; return ARS_E_UNSUPPORTED;
}
#ifndef ARS_RUNTIME_CORE
ARS_API ars_status ARS_CALL ars_runtime_query_close(ars_runtime_h runtime, ars_close_state *out) {
    (void)runtime;
    if (!prefix(out) || out->reserved0) return ARS_E_INVALID_ARGUMENT;
    clear(out);
    return ARS_E_UNSUPPORTED;
}
ARS_API ars_status ARS_CALL ars_runtime_finish_close(ars_runtime_h runtime) {
    (void)runtime; return ARS_E_UNSUPPORTED;
}
#endif
}
