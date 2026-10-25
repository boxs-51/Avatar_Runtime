#include "studio_rig_frame_fence.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
void studio_rig_frame_reset(StudioRigFrameFence *f,uint64_t generation){
 if(!f)return;
 memset(f,0,sizeof *f);
 f->model_generation=generation;
}
void studio_rig_frame_unbind(StudioRigFrameFence *f){
 if(!f)return;
 f->bound=0;f->prepared=0;f->accepted=0;
 f->bound_revision=0;f->prepared_frame=0;
 /* Preserve historical counters only as local diagnostic metadata. */
}
int studio_rig_frame_bind(StudioRigFrameFence *f,uint64_t generation,uint64_t rev){
 if(!f||!generation||generation!=f->model_generation||
    rev>=UINT64_C(0x8000000000000000))return 0;
 studio_rig_frame_unbind(f);
 f->bound=1;f->bound_revision=rev;
 return 1;
}
int studio_rig_frame_prepare(StudioRigFrameFence *f,uint64_t generation,
                              uint64_t rev,uint64_t frame){
 if(!f||!f->bound||!frame||generation!=f->model_generation||
    rev!=f->bound_revision||frame<=f->accepted_frame)return 0;
 f->prepared_frame=frame;f->prepared=1;
 return 1;
}
int studio_rig_frame_accept(StudioRigFrameFence *f,uint64_t generation,
                            uint64_t rev,uint64_t frame,int gpu_accepted){
 if(!f)return 0;
 int accepted=f->bound&&f->prepared&&gpu_accepted==1&&
       generation==f->model_generation&&rev==f->bound_revision&&
       frame==f->prepared_frame&&frame>f->accepted_frame;
 f->prepared=0;
 if(!accepted)return 0;
 f->accepted=1;f->accepted_revision=rev;f->accepted_frame=frame;
 return 1;
}
int studio_rig_frame_is_current(const StudioRigFrameFence *f,
                                 uint64_t generation,uint64_t revision){
 return f&&f->bound&&f->accepted&&generation==f->model_generation&&
        f->bound_revision==revision&&f->accepted_revision==revision;
}

int studio_rig_frame_format(const StudioRigFrameFence *f,char *out,size_t cap,
                             uint64_t generation,uint64_t rig_revision){
 if(!out||!cap)return 0;
 out[0]=0;
 if(!f||generation>=UINT64_C(0x8000000000000000)||
    rig_revision>=UINT64_C(0x8000000000000000))return 0;
 int n=snprintf(out,cap,
   "writer=HOLD frame=LEGACY_ONLY gen=%" PRIu64
   " rigrev=%" PRIu64 " bound=%" PRIu64
   " accepted=%" PRIu64 " serial=%" PRIu64 " current=%d",
   generation,rig_revision,f->bound?f->bound_revision:0,
   f->accepted?f->accepted_revision:0,f->accepted_frame,
   studio_rig_frame_is_current(f,generation,rig_revision));
 if(n<0||(size_t)n>=cap){out[0]=0;return 0;}
 return 1;
}
