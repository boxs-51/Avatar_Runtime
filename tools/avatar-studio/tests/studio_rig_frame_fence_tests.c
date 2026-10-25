/* Model-free RED-first frame readiness fence. */
#include "../studio_rig_frame_fence.h"
#include <stdio.h>
#define REQ(x) do{if(!(x)){fprintf(stderr,"G5_FRAME_FENCE_RED %d\n",__LINE__);return 1;}}while(0)
int main(void){
 StudioRigFrameFence f={0};
 studio_rig_frame_reset(&f,99);
 REQ(!studio_rig_frame_is_current(&f,99,7));
 REQ(!studio_rig_frame_accept(&f,99,7,10,1)); /* no prepared */
 REQ(studio_rig_frame_bind(&f,99,7));
 REQ(studio_rig_frame_prepare(&f,99,7,10));
 REQ(!studio_rig_frame_accept(&f,99,7,10,0)); /* GPU rejected */
 REQ(!studio_rig_frame_is_current(&f,99,7));
 REQ(studio_rig_frame_prepare(&f,99,7,11));
 REQ(!studio_rig_frame_accept(&f,99,8,11,1)); /* Rig changed in-frame */
 REQ(studio_rig_frame_prepare(&f,99,7,12));
 REQ(studio_rig_frame_accept(&f,99,7,12,1));
 REQ(studio_rig_frame_is_current(&f,99,7));
 REQ(!studio_rig_frame_is_current(&f,99,8)); /* stale after edit */
 REQ(studio_rig_frame_prepare(&f,99,7,13));
 REQ(!studio_rig_frame_accept(&f,100,7,13,1)); /* generation changed */
 REQ(studio_rig_frame_is_current(&f,99,7));
 studio_rig_frame_unbind(&f);
 REQ(!studio_rig_frame_is_current(&f,99,7));
 studio_rig_frame_reset(&f,100);
 REQ(!studio_rig_frame_is_current(&f,100,7));
 puts("G5_LEGACY_FRAME_FENCE_PASS no_early_apply=1 stale=blocked");
 return 0;
}
