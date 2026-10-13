#include "../../contracts/ars_p0_contract_draft.h"
#include <type_traits>
static_assert(sizeof(ars_handle_id_v1) == 16);
static_assert(sizeof(ars_ingress_cap_v1) == 16);
static_assert(!std::is_same_v<ars_character_h, ars_source_admin_h>);
int main() {
    ars_source_admin_h admin{};
    return admin.id.slot != 0;
}
