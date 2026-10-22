#pragma once
#include "PurismCore.h"
#include "studio_rig.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int first,second,joint; float gravity,wind,compliance,collision,mass,damping,contact_compliance,dt; int iterations,pin,substeps; float pose_compliance,source_motion_transfer,bending_compliance,surface_thickness; int anchor_count; } StudioXpbdOptions;
typedef struct StudioXpbdMetrics { float max_step,max_acceleration,pin_drift,current_step; int accepted_frames; } StudioXpbdMetrics;
int studio_xpbd_metrics(void *,StudioXpbdMetrics *);
void *studio_xpbd_bind(csmModel *, StudioXpbdOptions);
void *studio_xpbd_bind_rig(csmModel *, StudioXpbdOptions, const StudioRig *);
int studio_xpbd_prepare(void *, csmModel *);
int studio_xpbd_accept(void *);
void studio_xpbd_set_interaction(void *,float,float);
void studio_xpbd_disable(void *);
const csmVector2 **studio_xpbd_positions(csmModel *);
#ifdef __cplusplus
}
#endif
