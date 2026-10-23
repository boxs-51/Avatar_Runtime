/* Adversarial G5 Native standalone suite; synthetic-only rig. */
#include "../studio_g5_txn.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"G5_TXN_ADVERSARIAL_RED line=%d\n",__LINE__);return 1;}}while(0)
static int make(G5Txn *t,StudioRig *r){
 if(!studio_rig_init(r,0,42,8))return 0;
 return g5_txn_init(t,r,99,55,7)==G5_OK;
}
int main(void){
 G5Txn t={0};StudioRig r={0};uint64_t draft=0,rev=0;
 G5Receipt rec={0};
 CHECK(make(&t,&r));
 /* Unknown receipt is never a positive NOT_APPLIED assertion. */
 CHECK(g5_txn_receipt(&t,99,111,888,&rec)==G5_UNKNOWN);
 CHECK(g5_txn_begin(&t,99,55,7,111,&draft)==G5_OK);
 CHECK(g5_txn_anchor(&t,99,54,111,draft,2,1)==G5_STALE_MODEL);
 CHECK(g5_txn_anchor(&t,98,55,111,draft,2,1)==G5_STALE_SESSION);
 CHECK(g5_txn_validate(&t,99,54,111,draft)==G5_STALE_MODEL);
 CHECK(g5_txn_commit(&t,98,55,111,draft,7,17,&rev)==G5_STALE_SESSION);
 CHECK(g5_txn_anchor(&t,99,55,111,draft,2,1)==G5_OK);
 CHECK(g5_txn_commit(&t,99,55,111,draft,7,17,&rev)==G5_INVALID_DRAFT);
 CHECK(t.live.anchor_count==0&&t.revision==7&&t.receipt_count==0);
 CHECK(g5_txn_validate(&t,99,55,111,draft)==G5_OK);
 CHECK(g5_txn_commit(&t,99,55,111,draft,8,17,&rev)==G5_REVISION_CONFLICT);
 CHECK(t.revision==7&&t.receipt_count==0);
 CHECK(g5_txn_commit(&t,99,55,111,draft,7,17,&rev)==G5_OK);
 CHECK(t.revision==8&&t.live.anchor_count==1);
 CHECK(g5_txn_receipt(&t,99,111,17,&rec)==G5_OK&&rec.revision==8);
 CHECK(g5_txn_receipt(&t,99,222,17,&rec)==G5_STALE_SESSION);
 CHECK(g5_txn_commit(&t,99,55,111,draft,7,17,&rev)==G5_ID_CONFLICT);
 CHECK(g5_txn_undo(&t,99,55,8,111,18,&rev)==G5_OK);
 CHECK(rev==9&&t.live.anchor_count==0);
 CHECK(g5_txn_begin(&t,99,55,9,111,&draft)==G5_OK);
 CHECK(g5_txn_anchor(&t,99,55,111,draft,4,1)==G5_OK);
 CHECK(g5_txn_validate(&t,99,55,111,draft)==G5_OK);
 CHECK(g5_txn_commit(&t,99,55,111,draft,9,19,&rev)==G5_OK);
 CHECK(rev==10&&t.live.anchors[0]==4);
 CHECK(g5_txn_redo(&t,99,55,10,111,20,&rev)==G5_REVISION_CONFLICT);
 CHECK(t.revision==10);
 /* A new session invalidates old receipt owner/epoch and stale drafts. */
 G5Txn fresh={0};
 CHECK(g5_txn_init(&fresh,&t.live,100,56,1)==G5_OK);
 CHECK(g5_txn_receipt(&fresh,99,111,19,&rec)==G5_STALE_SESSION);
 CHECK(g5_txn_receipt(&fresh,100,111,19,&rec)==G5_UNKNOWN);
 /* Exact budget rejection must leave immutable history/revision intact. */
 CHECK(make(&t,&r));
 for(int i=0;i<G5_HISTORY_CAP;i++){
  CHECK(g5_txn_begin(&t,99,55,t.revision,111,&draft)==G5_OK);
  CHECK(g5_txn_anchor(&t,99,55,111,draft,1,i%2)==G5_OK);
  CHECK(g5_txn_validate(&t,99,55,111,draft)==G5_OK);
  CHECK(g5_txn_commit(&t,99,55,111,draft,t.revision,100+i,&rev)==G5_OK);
 }
 CHECK(t.undo_count==G5_HISTORY_CAP&&t.revision==7+G5_HISTORY_CAP);
 CHECK(g5_txn_begin(&t,99,55,t.revision,111,&draft)==G5_OK);
 CHECK(g5_txn_validate(&t,99,55,111,draft)==G5_OK);
 StudioRig old=t.live;uint64_t before=t.revision;unsigned receipt_count=t.receipt_count;
 CHECK(g5_txn_commit(&t,99,55,111,draft,t.revision,900,&rev)==G5_OUT_OF_BUDGET);
 CHECK(t.revision==before&&t.receipt_count==receipt_count);
 CHECK(memcmp(&t.live,&old,sizeof old)==0);
 CHECK(g5_txn_abort(&t,99,55,111,draft)==G5_OK);
 CHECK(g5_txn_undo(&t,99,55,t.revision,111,901,&rev)==G5_OK);
 /* No mutation on malformed/invalid source rig initial state. */
 StudioRig invalid=r;
 invalid.vertex_count=3;invalid.anchor_count=4;
 CHECK(g5_txn_init(&fresh,&invalid,101,55,7)==G5_INVALID_DRAFT);
 invalid=r;
 invalid.parent_count=2;invalid.parents[0]=(StudioRigParent){0,1,.5f};
 invalid.parents[1]=(StudioRigParent){1,0,.5f};
 CHECK(g5_txn_init(&fresh,&invalid,101,55,7)==G5_INVALID_DRAFT);
 /* Receipt index exhaustion must prevent even a valid redo. */
 G5Txn limited={0};
 CHECK(make(&limited,&r));
 for(int i=0;i<G5_HISTORY_CAP;i++){
  CHECK(g5_txn_begin(&limited,99,55,limited.revision,111,&draft)==G5_OK);
  CHECK(g5_txn_validate(&limited,99,55,111,draft)==G5_OK);
  CHECK(g5_txn_commit(&limited,99,55,111,draft,limited.revision,
                      500+i,&rev)==G5_OK);
 }
 for(int i=0;i<G5_HISTORY_CAP;i++)
  CHECK(g5_txn_undo(&limited,99,55,limited.revision,111,600+i,&rev)==G5_OK);
 CHECK(limited.receipt_count==G5_RECEIPT_CAP);
 uint64_t full_rev=limited.revision;
 CHECK(g5_txn_redo(&limited,99,55,full_rev,111,700,&rev)==G5_OUT_OF_BUDGET);
 CHECK(limited.revision==full_rev&&limited.redo_count==G5_HISTORY_CAP);
 /* Revision overflow cannot wrap and resurrect stale CAS versions. */
 G5Txn near_overflow={0};
 CHECK(g5_txn_init(&near_overflow,&r,99,55,
                   UINT64_C(0x7ffffffffffffffe))==G5_OK);
 CHECK(g5_txn_begin(&near_overflow,99,55,near_overflow.revision,111,&draft)==G5_OK);
 CHECK(g5_txn_validate(&near_overflow,99,55,111,draft)==G5_OK);
 CHECK(g5_txn_commit(&near_overflow,99,55,111,draft,
                     near_overflow.revision,800,&rev)==G5_OK);
 CHECK(rev==UINT64_C(0x7fffffffffffffff));
 CHECK(g5_txn_begin(&near_overflow,99,55,rev,111,&draft)==G5_OK);
 CHECK(g5_txn_validate(&near_overflow,99,55,111,draft)==G5_OK);
 CHECK(g5_txn_commit(&near_overflow,99,55,111,draft,rev,801,&rev)==G5_OUT_OF_BUDGET);
 CHECK(near_overflow.revision==UINT64_C(0x7fffffffffffffff));
 puts("G5_NATIVE_ADVERSARIAL_PASS leases=exclusive receipt=scoped ABA=guarded capacity=fail_closed");
 return 0;
}
