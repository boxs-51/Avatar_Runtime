#pragma once
#include "studio_g5_bus.h"
#include "studio_rig_dispatch.h"

/* Trusted local owner-thread routing; no caller-supplied actor identity. */
typedef enum {
 STUDIO_RIG_UI=1,
 STUDIO_RIG_V1=2,
 STUDIO_RIG_AGENT=3
} StudioRigSource;
typedef enum {
 STUDIO_RIG_AUTH_OK=0,
 STUDIO_RIG_AUTH_UNCHANGED,
 STUDIO_RIG_AUTH_REJECT,
 STUDIO_RIG_AUTH_STALE,
 STUDIO_RIG_AUTH_BUSY,
 STUDIO_RIG_AUTH_UNKNOWN
} StudioRigAuthorityCode;
typedef struct {
 G5Bus bus;
} StudioRigAuthority;
typedef struct {
 uint64_t revision;
 StudioRigAuthorityCode code;
} StudioRigAuthorityResult;
G5Code studio_rig_authority_init(
 StudioRigAuthority *authority,const StudioRig *initial,
 uint64_t session,uint64_t generation,uint64_t revision);
StudioRigAuthorityCode studio_rig_authority_submit(
 StudioRigAuthority *authority,StudioRigSource source,
 uint64_t session,uint64_t generation,uint64_t expected_revision,
 uint64_t request_id,StudioRigCommand command,void *argument,
 StudioRigAuthorityResult *result);
