#pragma once
/* Local-only XPBD/render accepted-frame witness for LEGACY Rig revision.
   Not a G5 document APPLIED receipt and not a public SDK symbol. */
#include <stdint.h>
#include <stddef.h>
typedef struct {
 uint64_t model_generation,bound_revision,accepted_revision;
 uint64_t prepared_frame,accepted_frame;
 unsigned bound,prepared,accepted;
} StudioRigFrameFence;
void studio_rig_frame_reset(StudioRigFrameFence *f,uint64_t generation);
int studio_rig_frame_bind(StudioRigFrameFence *f,uint64_t generation,uint64_t rig_revision);
int studio_rig_frame_prepare(StudioRigFrameFence *f,uint64_t generation,
                     uint64_t rig_revision,uint64_t frame);
int studio_rig_frame_accept(StudioRigFrameFence *f,uint64_t generation,
                    uint64_t rig_revision,uint64_t frame,int gpu_accepted);
void studio_rig_frame_unbind(StudioRigFrameFence *f);
int studio_rig_frame_is_current(const StudioRigFrameFence *f,
                                uint64_t generation,uint64_t rig_revision);
/* LOCAL_PRIVATE legacy solver/render witness; never G5 APPLIED. */
int studio_rig_frame_format(const StudioRigFrameFence *f,char *out,size_t cap,
                            uint64_t generation,uint64_t rig_revision);
