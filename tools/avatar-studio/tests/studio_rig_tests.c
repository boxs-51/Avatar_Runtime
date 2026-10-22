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
 assert(studio_rig_set_role(&a,1));
 assert(!studio_rig_set_role(&a,9));
 assert(studio_rig_save(&a,"build/studio-user/rig-test-local.txt"));
 assert(studio_rig_load(&b,"build/studio-user/rig-test-local.txt",42,20));
 assert(b.anchor_count==2&&b.beam_count==1&&b.anchors[1]==2);
 assert(b.role==1);
 assert(!studio_rig_load(&b,"build/studio-user/rig-test-local.txt",43,20));
 assert(b.anchor_count==2&&b.beam_count==1);
 assert(studio_rig_toggle_anchor(&b,2));
 assert(b.anchor_count==1);
 StudioRigBank bank={0};
 StudioRig current={0};
 assert(studio_rig_bank_select(&bank,&current,0,300,12));
 assert(studio_rig_set_role(&current,2));
 assert(studio_rig_toggle_anchor(&current,2));
 assert(studio_rig_toggle_beam(&current,3,4));
 assert(studio_rig_bank_select(&bank,&current,1,300,15));
 assert(current.drawable==1&&current.anchor_count==0&&current.beam_count==0);
 assert(studio_rig_toggle_anchor(&current,6));
 assert(studio_rig_bank_select(&bank,&current,0,300,12));
 assert(current.anchor_count==1&&current.anchors[0]==2&&current.beam_count==1);
 assert(current.role==2);
 assert(studio_rig_bank_select(&bank,&current,1,300,15));
 assert(current.anchor_count==1&&current.anchors[0]==6);
 assert(studio_rig_bank_select(&bank,&current,0,301,12));
 assert(current.model_signature==301&&current.anchor_count==0&&current.beam_count==0);
 assert(bank.count<=1);
 puts("RIG_BANK_SWAP_PASS two_layer_cache=1 model_replacement_fence=1");
 FILE *legacy=fopen("build/studio-user/rig-legacy-test.txt","w");
 assert(legacy!=NULL);
 assert(fprintf(legacy,"STUDIO_RIG_1 42 0 20 1 0\nA 3\n")>0);
 assert(fclose(legacy)==0);
 StudioRig old={0};
 assert(studio_rig_load(&old,"build/studio-user/rig-legacy-test.txt",42,20));
 assert(old.role==0&&old.anchor_count==1&&old.anchors[0]==3);
 puts("RIG_FORMAT_MIGRATION_PASS v1_read=1 v2_role=1");
 puts("RIG_EDITOR_TEST_PASS anchors=2 beams=1 duplicate=guard signature=guard undo=toggle");
 return 0;
}
