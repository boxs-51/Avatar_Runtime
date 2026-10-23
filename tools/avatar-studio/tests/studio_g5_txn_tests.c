/* Native, model-free G5 transaction contract. No Studio GUI and no private assets. */
#include "../studio_g5_txn.h"
#include <stdio.h>
#define CHECK(expr) do {if(!(expr)){fprintf(stderr,"G5_NATIVE_RED line=%d\n",__LINE__);return 1;}}while(0)
int main(void){
  StudioRig rig={0};G5Txn t={0};uint64_t draft=0,out=0;
  G5Receipt receipt={0};
  CHECK(studio_rig_init(&rig,0,42,8));
  CHECK(studio_rig_toggle_anchor(&rig,0));
  CHECK(g5_txn_init(&t,&rig,99,55,7)==G5_OK);
  CHECK(g5_txn_begin(&t,99,55,7,101,&draft)==G5_OK);
  CHECK(g5_txn_begin(&t,99,55,7,202,&out)==G5_BUSY);
  CHECK(g5_txn_anchor(&t,99,55,202,draft,2,1)==G5_BUSY);
  CHECK(g5_txn_anchor(&t,99,55,101,draft,2,1)==G5_OK);
  CHECK(t.live.anchor_count==1);
  CHECK(g5_txn_validate(&t,99,55,101,draft)==G5_OK);
  CHECK(g5_txn_commit(&t,99,55,101,draft,7,301,&out)==G5_OK);
  CHECK(out==8&&t.revision==8&&t.live.anchor_count==2);
  CHECK(t.applied_revision==7);
  CHECK(g5_txn_receipt(&t,99,101,301,&receipt)==G5_OK);
  CHECK(receipt.outcome==G5_COMMITTED&&receipt.revision==8);
  CHECK(g5_txn_commit(&t,99,55,101,draft,7,301,&out)==G5_ID_CONFLICT);
  CHECK(t.revision==8);
  CHECK(g5_txn_begin(&t,99,55,7,101,&draft)==G5_REVISION_CONFLICT);
  CHECK(g5_txn_begin(&t,99,54,8,101,&draft)==G5_STALE_MODEL);
  CHECK(g5_txn_begin(&t,98,55,8,101,&draft)==G5_STALE_SESSION);
  CHECK(g5_txn_undo(&t,99,55,8,101,302,&out)==G5_OK);
  CHECK(out==9&&t.live.anchor_count==1);
  CHECK(g5_txn_begin(&t,99,55,7,101,&draft)==G5_REVISION_CONFLICT); /* ABA */
  CHECK(g5_txn_redo(&t,99,55,9,101,303,&out)==G5_OK);
  CHECK(out==10&&t.live.anchor_count==2);
  CHECK(g5_txn_apply(&t,10,3,44,0)==G5_INVALID_DRAFT);
  CHECK(t.applied_revision==7);
  CHECK(g5_txn_apply(&t,10,3,44,1)==G5_OK);
  CHECK(t.applied_revision==10&&t.accepted_frame==44);
  CHECK(g5_txn_begin(&t,99,55,10,101,&draft)==G5_OK);
  CHECK(g5_txn_anchor(&t,99,55,101,draft,99,1)==G5_INVALID_DRAFT);
  CHECK(g5_txn_commit(&t,99,55,101,draft,10,304,&out)==G5_INVALID_DRAFT);
  CHECK(g5_txn_abort(&t,99,55,101,draft)==G5_OK);
  CHECK(t.revision==10);
  CHECK(g5_txn_receipt(&t,99,101,999,&receipt)==G5_UNKNOWN);
  CHECK(g5_txn_receipt(&t,99,202,301,&receipt)==G5_STALE_SESSION);
  puts("G5_NATIVE_TXN_PASS cases=25 model_private=false");
  return 0;
}
