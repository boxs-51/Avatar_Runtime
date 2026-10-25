#include "../studio_rig_dispatch.h"
#include <stdio.h>
#include <string.h>
#define REQ(x) do{if(!(x)){fprintf(stderr,"RIG_DISPATCH_RED line=%d\n",__LINE__);return 1;}}while(0)
static int anchor(StudioRig*r,void*arg){return studio_rig_toggle_anchor(r,*(int*)arg);}
static int no_change(StudioRig*r,void*arg){(void)r;(void)arg;return 1;}
static int reject_after_write(StudioRig*r,void*arg){r->role=*(int*)arg;return 0;}
/* Regression: unused tail bytes do not represent a logical Rig edit. */
static int mutate_inactive(StudioRig*r,void*arg){
 (void)arg;r->anchors[STUDIO_RIG_MAX_ANCHORS-1]=123;return 1;
}
/* An owner-thread command may not silently change the model identity. */
static int corrupt_identity(StudioRig*r,void*arg){
 (void)arg;r->model_signature++;return 1;
}
/* Reject malformed cardinality before comparing any active elements. */
static int corrupt_count(StudioRig*r,void*arg){
 (void)arg;r->anchor_count=STUDIO_RIG_MAX_ANCHORS+1;return 1;
}
int main(void){
 StudioRig rig={0};uint64_t rev=7;int index=2,role=3;
 REQ(studio_rig_init(&rig,0,42,10));
 REQ(studio_rig_dispatch(&rig,&rev,99,99,7,anchor,&index)==STUDIO_RIG_DISPATCH_OK);
 REQ(rev==8&&rig.anchor_count==1&&rig.anchors[0]==2);
 REQ(studio_rig_dispatch(&rig,&rev,99,99,7,anchor,&index)==STUDIO_RIG_DISPATCH_STALE);
 REQ(studio_rig_dispatch(&rig,&rev,99,100,8,anchor,&index)==STUDIO_RIG_DISPATCH_STALE);
 REQ(rev==8&&rig.anchor_count==1);
 REQ(studio_rig_dispatch(&rig,&rev,99,99,8,reject_after_write,&role)==STUDIO_RIG_DISPATCH_REJECT);
 REQ(rev==8&&rig.role!=3);
 REQ(studio_rig_dispatch(&rig,&rev,99,99,8,no_change,NULL)==STUDIO_RIG_DISPATCH_UNCHANGED);
 REQ(rev==8);
 REQ(studio_rig_dispatch(&rig,&rev,99,99,8,anchor,&index)==STUDIO_RIG_DISPATCH_OK);
 REQ(rev==9&&rig.anchor_count==0);
 REQ(studio_rig_dispatch(&rig,&rev,99,99,9,mutate_inactive,NULL)==STUDIO_RIG_DISPATCH_UNCHANGED);
 REQ(rev==9&&rig.anchors[STUDIO_RIG_MAX_ANCHORS-1]==0);
 REQ(studio_rig_dispatch(&rig,&rev,99,99,9,corrupt_identity,NULL)==STUDIO_RIG_DISPATCH_REJECT);
 REQ(rev==9&&rig.model_signature==42);
 REQ(studio_rig_dispatch(&rig,&rev,99,99,9,corrupt_count,NULL)==STUDIO_RIG_DISPATCH_REJECT);
 REQ(rev==9&&rig.anchor_count==0);
 StudioRigBank bank={0};
 REQ(studio_rig_dispatch_select(&rig,&bank,&rev,99,99,9,1,42,8)==STUDIO_RIG_DISPATCH_OK);
 REQ(rev==10&&rig.drawable==1&&rig.vertex_count==8&&rig.model_signature==42);
 REQ(studio_rig_dispatch_select(&rig,&bank,&rev,99,99,10,0,42,10)==STUDIO_RIG_DISPATCH_OK);
 REQ(rev==11&&rig.drawable==0&&rig.anchor_count==0);
 REQ(studio_rig_dispatch_select(&rig,&bank,&rev,99,99,10,1,42,8)==STUDIO_RIG_DISPATCH_STALE);
 REQ(studio_rig_dispatch_select(&rig,&bank,&rev,99,99,11,1,43,8)==STUDIO_RIG_DISPATCH_REJECT);
 REQ(rev==11&&rig.drawable==0);
 const int counts[]={0,10,2200,8,0,3};
 REQ(studio_rig_selectable_next(counts,6,1,1)==3);
 REQ(studio_rig_selectable_next(counts,6,3,-1)==1);
 REQ(studio_rig_selectable_next(counts,6,5,1)==1);
 REQ(studio_rig_selectable_next(counts,6,3,1)==5);
 REQ(studio_rig_selectable_next(counts,6,1,0)==-1);
 REQ(studio_rig_selectable_next(NULL,6,1,1)==-1);
 const int none[]={0,2200,-1};
 REQ(studio_rig_selectable_next(none,3,0,1)==-1);
 puts("RIG_OWNER_DISPATCH_PASS CAS=1 atomic=1 semantic_noop=1 bank_select=1 bounded_cycle=1");
 return 0;
}
