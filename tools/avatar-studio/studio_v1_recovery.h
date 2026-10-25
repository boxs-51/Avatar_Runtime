#pragma once
/* Private owner-thread V1 receipt metadata, not G5 commits or model data.
   Registry is process-local: UNKNOWN after crash/restart is not proof of no edit. */
#include <stddef.h>
#define STUDIO_V1_RECOVERY_CAP 64
typedef struct {char token[25];unsigned char outcome;} StudioV1Receipt;
typedef struct {
 StudioV1Receipt entries[STUDIO_V1_RECOVERY_CAP];
 unsigned next;
} StudioV1Recovery;
int studio_v1_recovery_store(StudioV1Recovery*,const char *token,int ok);
int studio_v1_recovery_lookup(const StudioV1Recovery*,const char *token);
int studio_v1_recovery_valid_token(const char *token);
typedef int (*StudioRecoveryPublish)(const char*,const char*);
void studio_v1_recovery_poll(const StudioV1Recovery*,const char *folder,
                              StudioRecoveryPublish publish);
