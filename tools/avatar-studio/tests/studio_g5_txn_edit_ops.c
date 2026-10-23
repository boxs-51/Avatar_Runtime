/* G5 staged Rig V4 operations, Native model-free contract. */
#include "../studio_g5_txn.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"G5_EDIT_OPS_RED line=%d\n",__LINE__);return 1;}}while(0)
int main(void){
 StudioRig r={0};G5Txn t={0};uint64_t d=0,rev=0;
 CHECK(studio_rig_init(&r,0,123,10));
 CHECK(g5_txn_init(&t,&r,81,55,7)==G5_OK);
 CHECK(g5_txn_begin(&t,81,55,7,11,&d)==G5_OK);
 CHECK(g5_txn_beam(&t,81,55,11,d,1,3,1)==G5_OK);
 CHECK(g5_txn_parent(&t,81,55,11,d,0,4,.75f,1)==G5_OK);
 CHECK(g5_txn_weight(&t,81,55,11,d,4,.25f)==G5_OK);
 CHECK(g5_txn_cross(&t,81,55,11,d,2,1,4,5,.75f,1)==G5_OK);
 CHECK(t.live.beam_count==0&&t.live.parent_count==0&&t.live.cross_count==0);
 CHECK(g5_txn_parent(&t,81,55,11,d,4,0,.5f,1)==G5_INVALID_DRAFT);
 CHECK(g5_txn_cross(&t,81,55,11,d,2,1,5,5,.75f,1)==G5_INVALID_DRAFT);
 CHECK(g5_txn_beam(&t,81,55,22,d,4,5,1)==G5_BUSY);
 CHECK(g5_txn_weight(&t,81,54,11,d,3,.5f)==G5_STALE_MODEL);
 CHECK(g5_txn_validate(&t,81,55,11,d)==G5_OK);
 CHECK(g5_txn_commit(&t,81,55,11,d,7,701,&rev)==G5_OK);
 CHECK(rev==8&&t.live.beam_count==1&&t.live.parent_count==1);
 CHECK(t.live.weight_count==1&&t.live.cross_count==1);
 CHECK(g5_txn_undo(&t,81,55,8,11,702,&rev)==G5_OK);
 CHECK(rev==9&&t.live.beam_count==0&&t.live.parent_count==0);
 CHECK(g5_txn_redo(&t,81,55,9,11,703,&rev)==G5_OK);
 CHECK(rev==10&&t.live.cross_count==1);
 puts("G5_NATIVE_EDIT_OPS_PASS stage=private controls=bounded undo_redo=restored");
 return 0;
}
