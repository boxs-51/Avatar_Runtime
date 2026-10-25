#include "../studio_rig_history.h"
#include <stdio.h>
#define REQ(x) do{if(!(x)){fprintf(stderr,"RIG_HISTORY_RED line=%d\n",__LINE__);return 1;}}while(0)
int main(void){
 StudioRig rig={0},before={0};StudioRigHistory h={0};uint64_t rev=3;
 REQ(studio_rig_init(&rig,0,42,10));
 studio_rig_history_reset(&h,99,rev,&rig);
 before=rig;REQ(studio_rig_toggle_anchor(&rig,2));rev++;
 REQ(studio_rig_history_record(&h,99,3,4,&before,&rig));
 REQ(h.undo_count==1&&h.redo_count==0);
 REQ(studio_rig_history_step(&h,&rig,&rev,99,99,4,0));
 REQ(rev==5&&rig.anchor_count==0&&h.redo_count==1);
 REQ(!studio_rig_history_step(&h,&rig,&rev,99,99,4,1));
 REQ(!studio_rig_history_step(&h,&rig,&rev,99,100,5,1));
 REQ(studio_rig_history_step(&h,&rig,&rev,99,99,5,1));
 REQ(rev==6&&rig.anchor_count==1);
 before=rig;REQ(studio_rig_toggle_anchor(&rig,5));rev++;
 REQ(studio_rig_history_record(&h,99,6,7,&before,&rig));
 REQ(h.redo_count==0);
 studio_rig_history_reset(&h,100,7,&rig);
 REQ(!studio_rig_history_step(&h,&rig,&rev,100,100,7,0));
 /* Ring capacity drops oldest snapshots rather than rejecting valid edits. */
 for(int i=0;i<20;i++){
  before=rig;uint64_t previous=rev;
  REQ(studio_rig_toggle_anchor(&rig,0));++rev;
  REQ(studio_rig_history_record(&h,100,previous,rev,&before,&rig));
 }
 REQ(h.undo_count==STUDIO_RIG_HISTORY_CAP);
 REQ(studio_rig_history_step(&h,&rig,&rev,100,100,rev,0));
 REQ(h.redo_count==1);
 before=rig;uint64_t old=rev;
 REQ(studio_rig_toggle_anchor(&rig,1));++rev;
 REQ(studio_rig_history_record(&h,100,old,rev,&before,&rig));
 REQ(h.redo_count==0);
 REQ(!studio_rig_history_step(&h,&rig,&rev,100,100,rev,1));
 REQ(h.undo_count==STUDIO_RIG_HISTORY_CAP);
 for(int i=0;i<STUDIO_RIG_HISTORY_CAP;i++){
  REQ(studio_rig_history_step(&h,&rig,&rev,100,100,rev,0));
 }
 REQ(h.undo_count==0);
 REQ(!studio_rig_history_step(&h,&rig,&rev,100,100,rev,0));
 puts("STUDIO_RIG_HISTORY_PASS cap=16 stale_fenced=1 no_cross_model=1");
 return 0;
}
