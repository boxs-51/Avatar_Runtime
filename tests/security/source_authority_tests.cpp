#include "runtime_test_support.hpp"
#include <array>
#include <cstring>

int main() {
    ars::core::RuntimeRouter router;
    ars_runtime_desc desc{}; desc.struct_size=sizeof(desc); desc.abi_version=ARS_ABI_VERSION;
    ars_runtime_h runtime{}; ars_character_h character{};
    test::status(ARS_OK,router.create(desc,&runtime),"B2-real-runtime-create");
    std::array<ars::security::SemanticChannel,1> catalog{};
    std::memcpy(catalog[0].id.data(),"mouth.open",11);
    test::status(ARS_OK,router.bind_character(runtime,catalog,&character),"B2-validated-private-CPU-binding");
    test::check(character.id.runtime_uid==runtime.id.runtime_uid,"B2-binding-owned-by-real-runtime");
    test::status(ARS_OK,router.begin_close(runtime),"B2-close");
    test::status(ARS_OK,router.finish_close(runtime),"B2-finish");
}
