#include "ars/ars.h"
#include <cstddef>
#include <cstdio>
#include <type_traits>

static_assert(sizeof(void *) == 8, "x64 foundation only");
static_assert(sizeof(ars_status) == 4);
static_assert(sizeof(ars_handle_id) == 16 && alignof(ars_handle_id) == 8);
static_assert(sizeof(ars_runtime_h) == 16 && sizeof(ars_model_h) == 16);
static_assert(sizeof(ars_character_h) == 16 && sizeof(ars_source_admin_h) == 16);
static_assert(sizeof(ars_ingress_cap) == 16);
#define LAYOUT(type, size) static_assert(sizeof(type) == size); \
    static_assert(offsetof(type, struct_size) == 0 && offsetof(type, abi_version) == 4); \
    static_assert(std::is_standard_layout_v<type> && std::is_trivially_copyable_v<type>)
LAYOUT(ars_runtime_desc, 56);
LAYOUT(ars_capabilities, 32);
LAYOUT(ars_asset_source, 32);
LAYOUT(ars_character_desc, 16);
LAYOUT(ars_layer_change, 40);
LAYOUT(ars_source_desc, 64);
LAYOUT(ars_motion_frame, 48);
LAYOUT(ars_close_state, 36);
LAYOUT(ars_render_target, 72);
static_assert(sizeof(ars_motion_channel) == 40);
#define SIGNATURE(name, result, ...) static_assert(std::is_same_v<decltype(&name), result (ARS_CALL *)(__VA_ARGS__)>)
SIGNATURE(ars_get_abi_version, uint32_t, void);
SIGNATURE(ars_runtime_create, ars_status, const ars_runtime_desc *, ars_runtime_h *);
SIGNATURE(ars_runtime_get_capabilities, ars_status, ars_runtime_h, ars_capabilities *);
SIGNATURE(ars_model_load, ars_status, ars_runtime_h, const ars_asset_source *, ars_model_h *);
SIGNATURE(ars_model_release, ars_status, ars_runtime_h, ars_model_h);
SIGNATURE(ars_character_create, ars_status, ars_runtime_h, ars_model_h, const ars_character_desc *, ars_character_h *);
SIGNATURE(ars_character_destroy, ars_status, ars_runtime_h, ars_character_h);
SIGNATURE(ars_character_set_mode, ars_status, ars_runtime_h, ars_character_h, uint32_t);
SIGNATURE(ars_character_set_visual_layer, ars_status, ars_runtime_h, ars_character_h, const ars_layer_change *);
SIGNATURE(ars_source_register, ars_status, ars_runtime_h, ars_character_h, const ars_source_desc *, ars_source_admin_h *, ars_ingress_cap *);
SIGNATURE(ars_source_admin_release, ars_status, ars_runtime_h, ars_source_admin_h);
SIGNATURE(ars_source_rotate, ars_status, ars_runtime_h, ars_source_admin_h, ars_ingress_cap *);
SIGNATURE(ars_source_revoke, ars_status, ars_runtime_h, ars_source_admin_h);
SIGNATURE(ars_motion_submit, ars_status, ars_runtime_h, ars_ingress_cap, const ars_motion_frame *);
SIGNATURE(ars_runtime_update, ars_status, ars_runtime_h, uint64_t);
SIGNATURE(ars_runtime_render, ars_status, ars_runtime_h, const ars_render_target *);
SIGNATURE(ars_runtime_begin_close, ars_status, ars_runtime_h);
SIGNATURE(ars_runtime_drain_gpu, ars_status, ars_runtime_h);
SIGNATURE(ars_runtime_query_close, ars_status, ars_runtime_h, ars_close_state *);
SIGNATURE(ars_runtime_finish_close, ars_status, ars_runtime_h);

int main() {
    ars_runtime_desc desc{};
    desc.struct_size = sizeof(desc); desc.abi_version = ARS_ABI_VERSION;
    ars_runtime_h runtime{{1, 2, 3}};
    if (ars_get_abi_version() != ARS_ABI_VERSION ||
        ars_runtime_create(&desc, &runtime) != ARS_E_UNSUPPORTED ||
        runtime.id.runtime_uid || runtime.id.slot || runtime.id.generation) return 1;
    std::puts("PASS ABI-03 C++20 x64 layouts, 20 signatures and foundation linkage");
    return 0;
}
