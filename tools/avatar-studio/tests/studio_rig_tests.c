#include "../studio_rig.h"
#include <stdlib.h>
#define assert(expr) do { if(!(expr)){fprintf(stderr,"RIG_CHECK_FAIL line=%d\n",__LINE__);return 1;} } while(0)
#include <stdio.h>
#include <string.h>
int main(void){
 StudioRig a={0}, b={0};
 assert(studio_rig_init(&a,0,42,20));
 assert(studio_rig_toggle_anchor(&a,0));
 assert(studio_rig_toggle_anchor(&a,2));
 assert(a.anchor_count==2);
 assert(studio_rig_toggle_beam(&a,1,3));
 assert(a.beam_count==1);
 assert(!studio_rig_toggle_beam(&a,1,1));
 assert(!studio_rig_toggle_anchor(&a,20));
 assert(!studio_rig_toggle_beam(&a,2,20));
 assert(studio_rig_save(&a,"build/studio-user/rig-test-local.txt"));
 assert(studio_rig_load(&b,"build/studio-user/rig-test-local.txt",42,20));
 assert(b.anchor_count==2&&b.beam_count==1&&b.anchors[1]==2);
 assert(!studio_rig_load(&b,"build/studio-user/rig-test-local.txt",43,20));
 assert(b.anchor_count==2&&b.beam_count==1);
 assert(studio_rig_toggle_anchor(&b,2));
 assert(b.anchor_count==1);
 puts("RIG_EDITOR_TEST_PASS anchors=2 beams=1 duplicate=guard signature=guard undo=toggle");
 return 0;
}
