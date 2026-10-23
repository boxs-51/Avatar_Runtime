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
 StudioRig hierarchy={0},loaded={0};
 assert(studio_rig_init(&hierarchy,4,999,32));
 assert(studio_rig_toggle_anchor(&hierarchy,0));
 assert(studio_rig_parent(&hierarchy,0,2,.9f,1));
 assert(studio_rig_parent(&hierarchy,2,4,.5f,1));
 assert(!studio_rig_parent(&hierarchy,4,0,.8f,1)); /* cycle */
 assert(!studio_rig_parent(&hierarchy,2,2,.8f,1)); /* self */
 assert(!studio_rig_parent(&hierarchy,0,4,.8f,1)); /* multiple parent */
 assert(!studio_rig_parent(&hierarchy,0,32,.8f,1));
 assert(studio_rig_weight(&hierarchy,4,.25f));
 assert(studio_rig_weight(&hierarchy,3,0.f));
 assert(!studio_rig_weight(&hierarchy,3,1.1f));
 assert(studio_rig_parent(&hierarchy,0,2,.9f,1)); /* idempotent */
 assert(hierarchy.parent_count==2&&hierarchy.weight_count==2);
 assert(studio_rig_save(&hierarchy,"build/studio-user/rig-hierarchy-test.txt"));
 assert(studio_rig_load(&loaded,"build/studio-user/rig-hierarchy-test.txt",999,32));
 assert(loaded.parent_count==2&&loaded.weight_count==2);
 assert(loaded.weights[0].vertex==4&&loaded.weights[0].weight==.25f);
 assert(studio_rig_parent(&hierarchy,2,4,.5f,0));
 assert(hierarchy.parent_count==1);
 FILE *adversarial=fopen("build/studio-user/rig-v3-adversarial.txt","w");
 assert(adversarial!=NULL);
 assert(fprintf(adversarial,"STUDIO_RIG_3 999 4 32 0 0 0 2 0\nP 0 2 0.9\nP 2 0 0.9\n")>0);
 assert(fclose(adversarial)==0);
 StudioRig preserved=loaded;
 assert(!studio_rig_load(&loaded,"build/studio-user/rig-v3-adversarial.txt",999,32));
 assert(loaded.parent_count==preserved.parent_count&&loaded.weight_count==preserved.weight_count);
 FILE *prior=fopen("build/studio-user/rig-v2-compat.txt","w");
 assert(prior!=NULL);
 assert(fprintf(prior,"STUDIO_RIG_2 999 4 32 1 0 2\nA 0\n")>0);
 assert(fclose(prior)==0);
 StudioRig version2={0};
 assert(studio_rig_load(&version2,"build/studio-user/rig-v2-compat.txt",999,32));
 assert(version2.role==2&&version2.parent_count==0&&version2.weight_count==0);
 puts("RIG_V3_SECURITY_PASS cyclic_file=reject no_partial_change=1 v2_read=1");
 puts("HIERARCHY_RIG_PASS cycles=reject weights=bounded v3=roundtrip");
 StudioRigBank browsing={0};StudioRig visitor={0};
 for(int i=0;i<130;++i)
  assert(studio_rig_bank_select(&browsing,&visitor,i,4000,12));
 assert(browsing.count==0);
 puts("RIG_NAVIGATION_PASS empty_rigs_not_cached=130");
 StudioRig cross={0},cross_saved={0};
 assert(studio_rig_init(&cross,7,9000,10));
 assert(studio_rig_cross(&cross,0,9,2,8,.85f,1));
 assert(!studio_rig_cross(&cross,0,7,2,8,.7f,1)); /* self drawable */
 assert(!studio_rig_cross(&cross,0,9,2,8,1.5f,1));
 assert(!studio_rig_cross(&cross,3,9,2,8,.85f,1)); /* second parent */
 assert(!studio_rig_cross(&cross,0,10,2,8,.85f,1)); /* target count */
 assert(studio_rig_cross(&cross,1,9,3,8,.75f,1));
 assert(cross.cross_count==2);
 assert(studio_rig_save(&cross,"build/studio-user/rig-cross-v4-test.txt"));
 assert(studio_rig_load(&cross_saved,"build/studio-user/rig-cross-v4-test.txt",9000,10));
 assert(cross_saved.cross_count==2&&cross_saved.cross_links[1].child==3);
 assert(studio_rig_cross(&cross,0,9,2,8,.85f,0)&&cross.cross_count==1);
 const StudioRigPoint points[]={{0,0},{1,0},{2,0},{3,0}};
 StudioRig brush={0};assert(studio_rig_init(&brush,0,1234,4));
 int touched=0;
 assert(studio_rig_brush(&brush,points,4,0,2.f,0.f,&touched)&&touched==3);
 assert(studio_rig_weight_get(&brush,0)==0.f);
 assert(studio_rig_weight_get(&brush,1)>0.f&&studio_rig_weight_get(&brush,1)<1.f);
 assert(studio_rig_weight_get(&brush,2)==1.f);
 StudioRig stable=brush;
 assert(!studio_rig_brush(&brush,points,4,0,-1.f,.5f,&touched));
 assert(!studio_rig_brush(&brush,points,4,0,2.f,1.2f,&touched));
 assert(brush.weight_count==stable.weight_count);
 StudioRigBank edited_cross_bank={0};
 StudioRig switching={0};
 assert(studio_rig_bank_select(&edited_cross_bank,&switching,0,9900,10));
 assert(studio_rig_cross(&switching,1,1,2,10,.9f,1));
 assert(studio_rig_bank_select(&edited_cross_bank,&switching,1,9900,10));
 assert(studio_rig_bank_select(&edited_cross_bank,&switching,0,9900,10));
 assert(switching.cross_count==1&&switching.cross_links[0].drawable==1);
 puts("CROSS_BANK_RETENTION_PASS cross_link_persisted=1");
 FILE *v3=fopen("build/studio-user/rig-valid-v3.txt","w");
 assert(v3!=NULL);
 assert(fprintf(v3,"STUDIO_RIG_3 9000 7 10 0 0 0 1 1\nP 0 2 0.9\nW 3 0.25\n")>0);
 assert(fclose(v3)==0);
 StudioRig old3={0};
 assert(studio_rig_load(&old3,"build/studio-user/rig-valid-v3.txt",9000,10));
 assert(old3.parent_count==1&&old3.weight_count==1&&old3.cross_count==0);
 FILE *bad4=fopen("build/studio-user/rig-bad-v4.txt","w");
 assert(bad4!=NULL);
 assert(fprintf(bad4,"STUDIO_RIG_4 9000 7 10 0 0 0 0 0 2\nC 0 1 1 0.8\nC 0 2 1 0.8\n")>0);
 assert(fclose(bad4)==0);
 StudioRig keep=cross_saved;
 assert(!studio_rig_load(&cross_saved,"build/studio-user/rig-bad-v4.txt",9000,10));
 assert(cross_saved.cross_count==keep.cross_count);
 puts("RIG_V4_NEGATIVE_PASS old_v3=read conflicting_targets=reject");
 puts("CROSS_BRUSH_TEST_PASS cross_v4=1 radial_weight=1 rejection=atomic");
 puts("RIG_EDITOR_TEST_PASS anchors=2 beams=1 duplicate=guard signature=guard undo=toggle");
 return 0;
}
