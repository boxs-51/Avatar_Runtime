/* ABI LINK TEST ONLY. Explicitly NONFUNCTIONAL. Not a registry or motion runtime. */
#define ARS_P0_BUILD_SHARED
#include "../../contracts/ars_p0_contract_draft.h"

ARS_P0_API uint32_t ARS_P0_CALL ars_get_abi_version_p0(void) { return ARS_P0_ABI_V1; }

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_create_p0(
    const ars_runtime_desc_v1 *desc, ars_runtime_h *out_runtime) {
    (void)desc;
    if (out_runtime) *out_runtime = (ars_runtime_h){0};
    return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_character_destroy_p0(
    ars_runtime_h runtime, ars_character_h character) {
    (void)runtime; (void)character; return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_model_release_p0(
    ars_runtime_h runtime, ars_model_h model) {
    (void)runtime; (void)model; return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_source_register_p0(
    ars_runtime_h runtime, ars_character_h character, const ars_source_desc_v1 *desc,
    ars_source_admin_h *out_admin, ars_ingress_cap_v1 *out_cap) {
    (void)runtime; (void)character; (void)desc;
    if (out_admin) *out_admin = (ars_source_admin_h){0};
    if (out_cap) *out_cap = (ars_ingress_cap_v1){0};
    return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_source_admin_release_p0(
    ars_runtime_h runtime, ars_source_admin_h admin) {
    (void)runtime; (void)admin; return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_source_rotate_p0(
    ars_runtime_h runtime, ars_source_admin_h admin, ars_ingress_cap_v1 *out_cap) {
    (void)runtime; (void)admin;
    if (out_cap) *out_cap = (ars_ingress_cap_v1){0};
    return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_source_revoke_p0(
    ars_runtime_h runtime, ars_source_admin_h admin) {
    (void)runtime; (void)admin; return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_motion_submit_p0(
    ars_runtime_h runtime, ars_ingress_cap_v1 cap, const ars_motion_frame_v1 *frame) {
    (void)runtime; (void)cap; (void)frame; return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_update_p0(
    ars_runtime_h runtime, uint64_t host_now_ns) {
    (void)runtime; (void)host_now_ns; return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_begin_close_p0(ars_runtime_h runtime) {
    (void)runtime; return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_drain_gpu_p0(ars_runtime_h runtime) {
    (void)runtime; return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_query_close_p0(
    ars_runtime_h runtime, ars_close_state_v1 *out_state) {
    (void)runtime;
    if (out_state) *out_state = (ars_close_state_v1){0};
    return ARS_E_UNSUPPORTED;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_finish_close_p0(ars_runtime_h runtime) {
    (void)runtime; return ARS_E_UNSUPPORTED;
}
