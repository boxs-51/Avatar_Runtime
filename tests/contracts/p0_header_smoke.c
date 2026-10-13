#include "../../contracts/ars_p0_contract_draft.h"
#include <stddef.h>
_Static_assert(sizeof(ars_handle_id_v1) == 16, "Handle is 16 bytes");
_Static_assert(sizeof(ars_status_v1) == 4, "ABI fixed 4-byte status");
_Static_assert(sizeof(ars_ingress_cap_v1) == 16, "Capability is 16 bytes");
_Static_assert(sizeof(ars_runtime_h) == sizeof(ars_character_h), "Strong types wrap same layout");
_Static_assert(offsetof(ars_motion_frame_v1, struct_size) == 0, "Size header first");
_Static_assert(offsetof(ars_motion_frame_v1, abi_version) == 4, "ABI version second");

int main(void) {
    ars_motion_frame_v1 frame = {0};
    frame.struct_size = (uint32_t)sizeof(frame);
    frame.abi_version = ARS_P0_ABI_V1;
    ars_runtime_h runtime = {0};
    ars_source_admin_h admin = {0};
    if (frame.abi_version != ARS_P0_ABI_V1) return 1;
    if (ars_get_abi_version_p0() != ARS_P0_ABI_V1) return 2;
    if (ars_source_revoke_p0(runtime, admin) != ARS_E_UNSUPPORTED) return 3;
    return 0;
}
