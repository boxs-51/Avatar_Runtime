/* ABI LINK TEST ONLY — deliberately no registry, capability or security implementation. */
#define ARS_P0_BUILD_SHARED
#include "../../contracts/ars_p0_contract_draft.h"

ARS_P0_API uint32_t ARS_P0_CALL ars_get_abi_version_p0(void) {
    return ARS_P0_ABI_V1;
}

ARS_P0_API ars_status_v1 ARS_P0_CALL
ars_source_revoke_p0(ars_runtime_h runtime, ars_source_admin_h admin) {
    (void)runtime; (void)admin;
    return ARS_E_UNSUPPORTED;
}
