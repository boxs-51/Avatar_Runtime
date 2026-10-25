#pragma once
/* Owner-thread Rig command dispatcher with revision and generation fences. */
#include <stdint.h>
#include "studio_rig.h"
typedef int (*StudioRigCommand)(StudioRig *draft,void *argument);
typedef enum {
 STUDIO_RIG_DISPATCH_OK=0,
 STUDIO_RIG_DISPATCH_UNCHANGED=1,
 STUDIO_RIG_DISPATCH_REJECT=2,
 STUDIO_RIG_DISPATCH_STALE=3
} StudioRigDispatchCode;
StudioRigDispatchCode studio_rig_dispatch(
 StudioRig *live,uint64_t *revision,uint64_t generation,
 uint64_t expected_generation,uint64_t expected_revision,
 StudioRigCommand command,void *argument);
/* Atomic drawable selection commits RigBank and live Rig together. */
StudioRigDispatchCode studio_rig_dispatch_select(
 StudioRig *live,StudioRigBank *bank,uint64_t *revision,
 uint64_t generation,uint64_t expected_generation,uint64_t expected_revision,
 int drawable,uint64_t signature,int vertices);
/* Walk only Rig-capable drawables; never select invalid or oversized geometry. */
int studio_rig_selectable_next(const int *vertex_counts,int drawable_count,
                               int current,int direction);
