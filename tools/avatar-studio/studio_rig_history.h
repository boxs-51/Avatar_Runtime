#pragma once
#include <stdint.h>
#include "studio_rig.h"

#define STUDIO_RIG_HISTORY_CAP 16
typedef struct {
 uint64_t generation,revision,signature;
 int drawable,vertices;
 unsigned undo_count,redo_count;
 StudioRig undo[STUDIO_RIG_HISTORY_CAP],redo[STUDIO_RIG_HISTORY_CAP];
} StudioRigHistory;
void studio_rig_history_reset(StudioRigHistory*,uint64_t generation,
 uint64_t revision,const StudioRig*);
int studio_rig_history_record(StudioRigHistory*,uint64_t generation,
 uint64_t before_revision,uint64_t after_revision,
 const StudioRig *before,const StudioRig *after);
int studio_rig_history_step(StudioRigHistory*,StudioRig*,uint64_t *revision,
 uint64_t generation,uint64_t expected_generation,
 uint64_t expected_revision,int redo);
