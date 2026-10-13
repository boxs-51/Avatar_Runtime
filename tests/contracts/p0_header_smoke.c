#include "../../contracts/ars_p0_contract_draft.h"
#include <stddef.h>
_Static_assert(sizeof(ars_handle_id_v1) == 16, "Handle is 16 bytes");
_Static_assert(sizeof(ars_status_v1) == 4, "ABI fixed 4-byte status");
_Static_assert(sizeof(ars_ingress_cap_v1) == 16, "Capability is 16 bytes");
_Static_assert(sizeof(ars_runtime_h) == sizeof(ars_character_h), "Strong types wrap same layout");
_Static_assert(offsetof(ars_motion_frame_v1, struct_size) == 0, "Size header first");
_Static_assert(offsetof(ars_motion_frame_v1, abi_version) == 4, "ABI version second");

_Static_assert(offsetof(ars_source_desc_v1, struct_size) == 0, "Source size first");
_Static_assert(offsetof(ars_runtime_desc_v1, abi_version) == 4, "Runtime version second");
_Static_assert(offsetof(ars_close_state_v1, abi_version) == 4, "Close version second");
_Static_assert(sizeof(ars_utf8_view_v1) == 16, "x64 ABI: ptr and byte length");

int main(void) {
    ars_motion_frame_v1 frame = {0};
    frame.struct_size = (uint32_t)sizeof(frame);
    frame.abi_version = ARS_P0_ABI_V1;
    ars_runtime_h runtime = {0};
    ars_source_admin_h admin = {0};
    if (frame.abi_version != ARS_P0_ABI_V1) return 1;
    if (ars_get_abi_version_p0() != ARS_P0_ABI_V1) return 2;
    if (ars_source_revoke_p0(runtime, admin) != ARS_E_UNSUPPORTED) return 3;
    ars_character_h character = {0};
    ars_model_h model = {0};
    ars_ingress_cap_v1 cap = {0};
    ars_close_state_v1 close_state = {0};
    if (ars_runtime_create_p0(0, &runtime) != ARS_E_UNSUPPORTED) return 4;
    if (ars_character_destroy_p0(runtime, character) != ARS_E_UNSUPPORTED) return 5;
    if (ars_model_release_p0(runtime, model) != ARS_E_UNSUPPORTED) return 6;
    if (ars_source_register_p0(runtime, character, 0, &admin, &cap) != ARS_E_UNSUPPORTED) return 7;
    if (ars_source_admin_release_p0(runtime, admin) != ARS_E_UNSUPPORTED) return 8;
    if (ars_source_rotate_p0(runtime, admin, &cap) != ARS_E_UNSUPPORTED) return 9;
    if (ars_motion_submit_p0(runtime, cap, &frame) != ARS_E_UNSUPPORTED) return 10;
    if (ars_runtime_update_p0(runtime, 0) != ARS_E_UNSUPPORTED) return 11;
    if (ars_runtime_begin_close_p0(runtime) != ARS_E_UNSUPPORTED) return 12;
    if (ars_runtime_drain_gpu_p0(runtime) != ARS_E_UNSUPPORTED) return 13;
    if (ars_runtime_query_close_p0(runtime, &close_state) != ARS_E_UNSUPPORTED) return 14;
    if (ars_runtime_finish_close_p0(runtime) != ARS_E_UNSUPPORTED) return 15;
    if (runtime.id.runtime_uid != 0 || cap.hi != 0 || cap.lo != 0) return 16;
    return 0;
}
