#ifndef ARS_PUBLIC_C_ABI_V1_DESIGN_H
#define ARS_PUBLIC_C_ABI_V1_DESIGN_H
/*
 * M0 G00 DECLARATION-ONLY REVIEW CANDIDATE. NOT APPROVED OR SHIPPING.
 * There is NO SDK implementation behind these declarations.
 * Native pointers are valid/aligned for the synchronous call; serialized
 * hostile input must pass a trusted bounded adapter. No C++ ABI at boundary.
 */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#if defined(_WIN32)
# if defined(ARS_BUILD_SHARED)
#  define ARS_API __declspec(dllexport)
# elif defined(ARS_USE_SHARED)
#  define ARS_API __declspec(dllimport)
# else
#  define ARS_API
# endif
# define ARS_CALL __cdecl
#else
# if defined(ARS_BUILD_SHARED)
#  define ARS_API __attribute__((visibility("default")))
# else
#  define ARS_API
# endif
# define ARS_CALL
#endif
#define ARS_ABI_MAJOR UINT32_C(1)
#define ARS_ABI_MINOR UINT32_C(0)
#define ARS_ABI_VERSION ((ARS_ABI_MAJOR << 16) | ARS_ABI_MINOR)
#define ARS_MAX_CHANNELS_PER_FRAME UINT32_C(256)
/* Error code VALUES are fixed across the C ABI, independent of C enum width. */
typedef int32_t ars_status;
enum {
  ARS_OK=0, ARS_E_INVALID_ARGUMENT=1, ARS_E_UNSUPPORTED=2,
  ARS_E_BAD_ASSET=3, ARS_E_OUT_OF_BUDGET=4, ARS_E_WRONG_THREAD=5,
  ARS_E_DEVICE_LOST=6, ARS_E_STALE_HANDLE=7, ARS_E_BUSY=8,
  ARS_E_INTERNAL=9, ARS_E_CLOSED=10, ARS_E_REVOKED=11,
  ARS_E_SCOPE_DENIED=12, ARS_E_INVALID_FRAME=13,
  ARS_E_CLOCK_REGRESSION=14, ARS_E_HANDLE_EXHAUSTED=15,
  ARS_E_CLOCK_FAILURE=16, ARS_E_RATE_LIMIT=17,
  ARS_E_QUEUE_FULL=18, ARS_E_SOURCE_EXPIRED=19,
  ARS_E_SECURITY_CLOCK_FAULT=20
};
/* By-value process-local registry IDs; never dereference as pointers. */
typedef struct ars_handle_id {
  uint64_t runtime_uid; uint32_t slot; uint32_t generation;
} ars_handle_id;
typedef struct { ars_handle_id id; } ars_runtime_h;
typedef struct { ars_handle_id id; } ars_model_h;
typedef struct { ars_handle_id id; } ars_character_h;
typedef struct { ars_handle_id id; } ars_source_admin_h;
typedef struct { uint64_t lo; uint64_t hi; } ars_ingress_cap;
typedef struct { const char *ptr; uint32_t byte_count; } ars_utf8_view;
/* No callback may throw across C or run while runtime authority/GPU locks held. */
typedef void * (ARS_CALL *ars_host_alloc_fn)(void *user_data, uint64_t bytes, uint32_t alignment);
typedef void   (ARS_CALL *ars_host_free_fn)(void *user_data, void *ptr, uint64_t bytes, uint32_t alignment);
typedef uint64_t (ARS_CALL *ars_simulation_now_fn)(void *user_data);
typedef struct {
  uint32_t struct_size, abi_version;
  ars_host_alloc_fn alloc; ars_host_free_fn free_fn; void *allocator_user_data;
  ars_simulation_now_fn simulation_now_ns; void *clock_user_data;
  uint32_t reserved_flags, reserved0;
} ars_runtime_desc;
/* Capability bits describe IMPLEMENTED features only; a stub returns zero. */
#define ARS_CAP_D3D11_HOST_DEVICE (UINT64_C(1) << 0)
#define ARS_CAP_NATIVE_2D_PROFILE_B (UINT64_C(1) << 1)
#define ARS_CAP_GLTF2_GLB_SUBSET (UINT64_C(1) << 2)
#define ARS_CAP_DUAL_MODE (UINT64_C(1) << 3)
#define ARS_CAP_AUTHENTICATED_MOTION_INGRESS (UINT64_C(1) << 4)
typedef struct {
  uint32_t struct_size, abi_version;
  uint64_t feature_bits;
  uint32_t max_active_sources, max_channels_per_frame;
  uint32_t max_texture_edge, reserved_flags;
} ars_capabilities;
enum { ARS_ASSET_MEMORY_BUNDLE=1 };
typedef struct {
  uint32_t struct_size, abi_version;
  uint32_t kind, reserved_flags;
  const uint8_t *bytes; uint64_t byte_count;
} ars_asset_source;
enum { ARS_MODE_2D=1, ARS_MODE_3D=2, ARS_MODE_AUTO=3 };
typedef struct {
  uint32_t struct_size, abi_version;
  uint32_t initial_mode, reserved_flags;
} ars_character_desc;
typedef struct {
  uint32_t struct_size, abi_version;
  ars_utf8_view layer_id;
  uint32_t visible; float opacity; int32_t z_index;
  uint32_t reserved_flags;
} ars_layer_change;
typedef struct {
  uint32_t struct_size, abi_version;
  ars_utf8_view diagnostic_label;
  const ars_utf8_view *allowed_channel_ids;
  uint32_t allowed_channel_count, max_channels_per_frame;
  int32_t fixed_priority; uint32_t max_frames_per_second;
  uint64_t lease_ttl_ns;
  uint32_t reserved_flags, reserved0;
} ars_source_desc;
enum {
  ARS_VALUE_SCALAR=1, ARS_VALUE_BOOL=2, ARS_VALUE_VEC2=3,
  ARS_VALUE_VEC3=4, ARS_VALUE_QUAT_XYZW=5, ARS_VALUE_TRIGGER=6
};
typedef struct {
  ars_utf8_view semantic_id;
  uint32_t kind, reserved0;
  float value[4];
} ars_motion_channel;
enum { ARS_TIMESTAMP_HOST_MONOTONIC=1, ARS_TIMESTAMP_ON_ARRIVAL=2 };
typedef struct {
  uint32_t struct_size, abi_version;
  uint64_t sequence, timestamp_ns;
  uint32_t timestamp_mode, channel_count;
  float confidence; uint32_t reserved_flags;
  const ars_motion_channel *channels;
} ars_motion_frame;
enum { ARS_FENCE_PENDING=1, ARS_FENCE_COMPLETE=2 };
typedef struct {
  uint32_t struct_size, abi_version;
  uint32_t state, outstanding_pins;
  uint32_t outstanding_gpu_retires, outstanding_render_snapshot_pins;
  uint32_t outstanding_ingress_pins, outstanding_close_management_pins;
  uint32_t reserved0;
} ars_close_state;
enum { ARS_COLOR_LINEAR=1, ARS_COLOR_SRGB=2, ARS_ALPHA_PREMULTIPLIED=1 };
/* Windows D3D11 HOST_DEVICE only: pointers are borrowed. SDK retains its own
 * COM refs for submitted in-flight work and releases on the R thread AFTER
 * the nominated completion mechanism. Never use RTV after host invalidation.
 * All device/context/target refs must belong to the same device generation.
 * Linux compiles the header; it does not promise Linux GPU support.
 */
typedef struct {
  uint32_t struct_size, abi_version;
  void *d3d11_device, *d3d11_immediate_context, *d3d11_rtv;
  uint64_t device_generation;
  uint32_t width_px, height_px, dxgi_format, sample_count;
  uint32_t color_space, alpha_mode, reserved_flags, reserved0;
} ars_render_target;
/* Every success output is fully initialized; every failure handle/cap is zeroed.
 * struct_size is checked before optional fields; caller buffers never overrun.
 * Callback ownership and error status are deterministic and documented.
 * ADMIN/revoke is trusted-host only; frame data never selects authorization.
 * close/query/drain/finish are management exceptions to normal ingress denial.
 */
ARS_API uint32_t ARS_CALL ars_get_abi_version(void);
ARS_API ars_status ARS_CALL ars_runtime_create(const ars_runtime_desc *, ars_runtime_h *);
ARS_API ars_status ARS_CALL ars_runtime_get_capabilities(ars_runtime_h, ars_capabilities *);
ARS_API ars_status ARS_CALL ars_model_load(ars_runtime_h, const ars_asset_source *, ars_model_h *);
ARS_API ars_status ARS_CALL ars_model_release(ars_runtime_h, ars_model_h);
ARS_API ars_status ARS_CALL ars_character_create(ars_runtime_h, ars_model_h, const ars_character_desc *, ars_character_h *);
ARS_API ars_status ARS_CALL ars_character_destroy(ars_runtime_h, ars_character_h);
ARS_API ars_status ARS_CALL ars_character_set_mode(ars_runtime_h, ars_character_h, uint32_t);
ARS_API ars_status ARS_CALL ars_character_set_visual_layer(ars_runtime_h, ars_character_h, const ars_layer_change *);
ARS_API ars_status ARS_CALL ars_source_register(ars_runtime_h, ars_character_h, const ars_source_desc *, ars_source_admin_h *, ars_ingress_cap *);
ARS_API ars_status ARS_CALL ars_source_admin_release(ars_runtime_h, ars_source_admin_h);
ARS_API ars_status ARS_CALL ars_source_rotate(ars_runtime_h, ars_source_admin_h, ars_ingress_cap *);
ARS_API ars_status ARS_CALL ars_source_revoke(ars_runtime_h, ars_source_admin_h);
ARS_API ars_status ARS_CALL ars_motion_submit(ars_runtime_h, ars_ingress_cap, const ars_motion_frame *);
ARS_API ars_status ARS_CALL ars_runtime_update(ars_runtime_h, uint64_t);
ARS_API ars_status ARS_CALL ars_runtime_render(ars_runtime_h, const ars_render_target *);
ARS_API ars_status ARS_CALL ars_runtime_begin_close(ars_runtime_h);
ARS_API ars_status ARS_CALL ars_runtime_drain_gpu(ars_runtime_h);
ARS_API ars_status ARS_CALL ars_runtime_query_close(ars_runtime_h, ars_close_state *);
ARS_API ars_status ARS_CALL ars_runtime_finish_close(ars_runtime_h);
/* Bounded caller-owned diagnostic buffer. No secret tokens or asset bytes.
 * out_required reports required bytes incl NUL; NULL+0 capacity queries size.
 * This is a proposed optional surface: ratify before freezing this ABI.
 */
ARS_API ars_status ARS_CALL ars_get_last_error_info(ars_runtime_h, char *, uint32_t, uint32_t *);
#ifdef __cplusplus
}
#endif
#endif /* ARS_PUBLIC_C_ABI_V1_DESIGN_H */
