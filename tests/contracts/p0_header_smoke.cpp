#include "../../contracts/ars_p0_contract_draft.h"
#include <type_traits>
static_assert(sizeof(ars_handle_id_v1) == 16);
static_assert(sizeof(ars_status_v1) == 4);
static_assert(sizeof(ars_ingress_cap_v1) == 16);
static_assert(!std::is_same_v<ars_character_h, ars_source_admin_h>);
int main() {
    ars_source_admin_h admin{};
    ars_runtime_h runtime{};
    if (ars_get_abi_version_p0() != ARS_P0_ABI_V1) return 1;
    if (ars_source_revoke_p0(runtime, admin) != ARS_E_UNSUPPORTED) return 2;
    return 0;
}
