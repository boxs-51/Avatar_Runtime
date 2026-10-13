#ifndef ARS_P0_CONTRACT_DRAFT_H
#define ARS_P0_CONTRACT_DRAFT_H
/* DESIGN-ONLY ABI contract for independent review. No implementation or ABI guarantee. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define ARS_P0_ABI_MAJOR UINT32_C(1)
#define ARS_P0_ABI_MINOR UINT32_C(0)
#define ARS_P0_ABI_V1 ((ARS_P0_ABI_MAJOR << 16) | ARS_P0_ABI_MINOR)
/* C ABI visibility and calling convention. Not a stable shipping API. */
#if defined(_WIN32)
# if defined(ARS_P0_BUILD_SHARED)
#  define ARS_P0_API __declspec(dllexport)
# elif defined(ARS_P0_USE_SHARED)
#  define ARS_P0_API __declspec(dllimport)
# else
#  define ARS_P0_API
# endif
# define ARS_P0_CALL __cdecl
#else
# if defined(ARS_P0_BUILD_SHARED) && (defined(__GNUC__) || defined(__clang__))
#  define ARS_P0_API __attribute__((visibility("default")))
# else
#  define ARS_P0_API
# endif
# define ARS_P0_CALL
#endif
/* All public extensible structs have uint32_t struct_size / abi_version prefix.
 * Reject major mismatch, unsupported minor fields and nonzero reserved flags.
 * Never read beyond caller-advertised struct_size; no forced packing pragma.
 */
#define ARS_P0_MAX_CHANNELS_PER_FRAME UINT32_C(256)

/* Tokens are passed BY VALUE; never reinterpret as heap pointers. */
typedef struct ars_handle_id_v1 {
    uint64_t runtime_uid;
    uint32_t slot;
    uint32_t generation;
} ars_handle_id_v1;
typedef struct { ars_handle_id_v1 id; } ars_runtime_h;
typedef struct { ars_handle_id_v1 id; } ars_model_h;
typedef struct { ars_handle_id_v1 id; } ars_character_h;
typedef struct { ars_handle_id_v1 id; } ars_source_admin_h;

/* A 128-bit CSPRNG-minted bearer capability. Never log or serialize. */
typedef struct ars_ingress_cap_v1 { uint64_t lo; uint64_t hi; } ars_ingress_cap_v1;
typedef struct ars_utf8_view_v1 { const char *ptr; uint32_t byte_count; } ars_utf8_view_v1;
typedef uint64_t (*ars_clock_now_ns_fn)(void *user_data);

typedef int32_t ars_status_v1;
/* Status values are constants; public function return width is fixed int32_t. */
enum ars_status_codes_v1 {
    ARS_OK = 0,
    ARS_E_INVALID_ARGUMENT = 1,
    ARS_E_UNSUPPORTED = 2,
    ARS_E_BAD_ASSET = 3,
    ARS_E_OUT_OF_BUDGET = 4,
    ARS_E_WRONG_THREAD = 5,
    ARS_E_DEVICE_LOST = 6,
    ARS_E_STALE_HANDLE = 7,
    ARS_E_BUSY = 8,
    ARS_E_INTERNAL = 9,
    ARS_E_CLOSED = 10,
    ARS_E_REVOKED = 11,
    ARS_E_SCOPE_DENIED = 12,
    ARS_E_INVALID_FRAME = 13,
    ARS_E_CLOCK_REGRESSION = 14,
    ARS_E_HANDLE_EXHAUSTED = 15,
    ARS_E_CLOCK_FAILURE = 16,
    ARS_E_RATE_LIMIT = 17,
    ARS_E_QUEUE_FULL = 18
};

typedef struct ars_runtime_desc_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    ars_clock_now_ns_fn clock_now_ns; /* OPTIONAL simulation time adapter only; never used for security TTL */
    void *clock_user_data;
    uint32_t reserved_flags; /* MUST be zero */
    uint32_t reserved0;
} ars_runtime_desc_v1;

typedef struct ars_source_desc_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    ars_utf8_view_v1 diagnostic_label; /* label is NOT authorization */
    const ars_utf8_view_v1 *allowed_channel_ids; /* copied during call */
    uint32_t allowed_channel_count;
    uint32_t max_channels_per_frame;
    int32_t fixed_motion_priority; /* chosen by trusted host */
    uint32_t max_frames_per_second; /* checked against trusted ingress clock */
    uint64_t lease_ttl_ns; /* 0 = policy default, bounded */
    uint32_t reserved_flags;
    uint32_t reserved0;
} ars_source_desc_v1;

typedef enum ars_value_kind_v1 {
    ARS_VALUE_SCALAR = 1,
    ARS_VALUE_BOOL = 2,
    ARS_VALUE_VEC2 = 3,
    ARS_VALUE_VEC3 = 4,
    ARS_VALUE_QUAT_XYZW = 5,
    ARS_VALUE_TRIGGER = 6
} ars_value_kind_v1;

typedef struct ars_motion_channel_v1 {
    ars_utf8_view_v1 semantic_id;
    uint32_t value_kind;
    uint32_t reserved0;
    float value[4]; /* NaN, Inf, malformed bool/quat rejected */
} ars_motion_channel_v1;

typedef enum ars_timestamp_mode_v1 {
    ARS_TIMESTAMP_HOST_MONOTONIC = 1,
    ARS_TIMESTAMP_ON_ARRIVAL = 2
} ars_timestamp_mode_v1;

typedef struct ars_motion_frame_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t sequence; /* strict increase per ingress capability */
    uint64_t timestamp_ns;
    uint32_t timestamp_mode;
    uint32_t channel_count;
    float confidence;
    uint32_t reserved_flags;
    const ars_motion_channel_v1 *channels; /* copied; never retained */
} ars_motion_frame_v1;

typedef enum ars_fence_state_v1 {
    ARS_FENCE_PENDING = 1,
    ARS_FENCE_COMPLETE = 2
} ars_fence_state_v1;

typedef struct ars_close_state_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t state; /* ARS_FENCE_PENDING/COMPLETE */
    uint32_t outstanding_pins; /* total: ingress + control + close-management + snapshots */
    uint32_t outstanding_gpu_retires;
    uint32_t outstanding_render_snapshot_pins;
    uint32_t outstanding_ingress_pins;
    uint32_t outstanding_close_management_pins;
    uint32_t reserved0;
} ars_close_state_v1;

/* Native callers guarantee pointer validity, alignment and lifetime during sync calls.
 * Validate NULL/count/overflow; arbitrary non-NULL invalid pointers are not safe.
 * Untrusted serialized inputs require a separate bounded adapter/IPC decoder.
 * Capability rotation revokes old token FIRST; any failure leaves source REVOKED.
 * Source ingress uses per-source mutex for atomic epoch/sequence/quota/queue commit.
 * Revoke and pose publish synchronize on one character publish gate, validating
 * transitive provenance of persistent motion states as well as current frames.
 */
/* Host-control-thread calls, serialized with structural mutation unless noted. */
ARS_P0_API uint32_t ARS_P0_CALL ars_get_abi_version_p0(void);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_create_p0(const ars_runtime_desc_v1 *desc, ars_runtime_h *out_runtime);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_character_destroy_p0(ars_runtime_h runtime, ars_character_h character);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_model_release_p0(ars_runtime_h runtime, ars_model_h model);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_source_register_p0(ars_runtime_h runtime, ars_character_h character,
                                      const ars_source_desc_v1 *desc,
                                      ars_source_admin_h *out_admin,
                                      ars_ingress_cap_v1 *out_cap);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_source_admin_release_p0(ars_runtime_h runtime, ars_source_admin_h admin);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_source_rotate_p0(ars_runtime_h runtime, ars_source_admin_h admin,
                                    ars_ingress_cap_v1 *out_new_cap);

/* Security exception: revoke MAY race simulation; synchronized at pose-publish gate.
   Returns when old generation can no longer enter a future pose commit; does not
   imply historical GPU frames are already retired. */
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_source_revoke_p0(ars_runtime_h runtime, ars_source_admin_h admin);

/* Only ingress mutator callable concurrently with itself. Copies complete frame. */
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_motion_submit_p0(ars_runtime_h runtime, ars_ingress_cap_v1 cap,
                                    const ars_motion_frame_v1 *frame);

/* Simulation owner: no overlap with structural mutation (except revoke). */
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_update_p0(ars_runtime_h runtime, uint64_t host_now_ns);

/* Close protocol: begin on control thread; drain on GPU owner thread; finish only
   after all pins, update/render operations and GPU retires reach zero. */
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_begin_close_p0(ars_runtime_h runtime);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_drain_gpu_p0(ars_runtime_h runtime);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_query_close_p0(ars_runtime_h runtime, ars_close_state_v1 *out_state);
ARS_P0_API ars_status_v1 ARS_P0_CALL ars_runtime_finish_close_p0(ars_runtime_h runtime);

#ifdef __cplusplus
}
#endif
#endif /* ARS_P0_CONTRACT_DRAFT_H */
