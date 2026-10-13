#include "../../contracts/ars_p0_contract_draft.h"
#include <stddef.h>
_Static_assert(sizeof(ars_handle_id_v1) == 16, "Handle is 16 bytes");
_Static_assert(sizeof(ars_ingress_cap_v1) == 16, "Capability is 16 bytes");
_Static_assert(sizeof(ars_runtime_h) == sizeof(ars_character_h), "Strong types wrap same layout");
_Static_assert(offsetof(ars_motion_frame_v1, struct_size) == 0, "Size header first");
_Static_assert(offsetof(ars_motion_frame_v1, abi_version) == 4, "ABI version second");

int main(void) {
    ars_motion_frame_v1 frame = {0};
    frame.struct_size = (uint32_t)sizeof(frame);
    frame.abi_version = ARS_P0_ABI_V1;
    return frame.abi_version != ARS_P0_ABI_V1;
}
