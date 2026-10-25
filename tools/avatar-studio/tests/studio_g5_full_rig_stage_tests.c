/* G5 T3 model-free Native full Rig candidate staged by Native owner only.
   RED-first: no public Studio/V2 writer or raw model IPC involved. */
#include "../studio_g5_bus.h"
#include <stdio.h>
#include <string.h>
#define REQ(x) do {if(!(x)){fprintf(stderr,"G5_NATIVE_CANDIDATE_RED line=%d\n",__LINE__);return 1;}}while(0)
static G5BusCommand make(G5BusOp op,uint64_t revision){
 G5BusCommand c={0};c.ingress=G5_UI;c.operation=op;c.session=99;
 c.generation=55;c.expected_revision=revision;return c;
}
int main(void){
 StudioRig original={0},candidate={0},invalid={0};
 G5Bus bus={0};G5BusResult out={0};
 const G5TrustedActor ui={G5_UI,1},agent={G5_AGENT,1};
 REQ(studio_rig_init(&original,0,42,10));
 REQ(g5_bus_init(&bus,&original,99,55,7)==G5_OK);
 G5BusCommand cmd=make(G5_BEGIN,7);
 REQ(g5_bus_submit_trusted(&bus,&ui,&cmd,&out)==G5_OK);
 uint64_t draft=out.draft_id;REQ(draft>0);
 candidate=original;
 REQ(studio_rig_set_role(&candidate,1));
 REQ(studio_rig_toggle_beam(&candidate,2,3));
 REQ(studio_rig_parent(&candidate,0,4,.8f,1));
 REQ(studio_rig_weight(&candidate,4,.25f));
 REQ(studio_rig_cross(&candidate,2,1,4,5,.5f,1));
 REQ(g5_bus_stage_native_candidate_trusted(&bus,&ui,99,55,draft,&candidate)==G5_OK);
 REQ(bus.txn.live.role==0&&bus.txn.live.beam_count==0);
 REQ(bus.txn.draft.role==1&&bus.txn.draft.beam_count==1);
 invalid=candidate;invalid.model_signature=777;
 REQ(g5_bus_stage_native_candidate_trusted(&bus,&ui,99,55,draft,&invalid)==G5_INVALID_DRAFT);
 REQ(bus.txn.draft.model_signature==42);
 REQ(g5_bus_stage_native_candidate_trusted(&bus,&agent,99,55,draft,&candidate)==G5_BUSY);
 REQ(g5_bus_stage_native_candidate_trusted(&bus,&ui,99,56,draft,&candidate)==G5_STALE_MODEL);
 cmd=make(G5_VALIDATE,7);cmd.draft_id=draft;
 REQ(g5_bus_submit_trusted(&bus,&ui,&cmd,&out)==G5_OK);
 cmd=make(G5_COMMIT,7);cmd.draft_id=draft;cmd.request_id=701;
 REQ(g5_bus_submit_trusted(&bus,&ui,&cmd,&out)==G5_OK);
 REQ(out.document_revision==8&&bus.txn.live.role==1);
 REQ(bus.txn.live.weight_count==1&&bus.txn.live.cross_count==1);
 cmd=make(G5_UNDO,8);cmd.request_id=702;
 REQ(g5_bus_submit_trusted(&bus,&ui,&cmd,&out)==G5_OK);
 REQ(out.document_revision==9&&bus.txn.live.role==0);
 cmd=make(G5_REDO,9);cmd.request_id=703;
 REQ(g5_bus_submit_trusted(&bus,&ui,&cmd,&out)==G5_OK);
 REQ(out.document_revision==10&&bus.txn.live.role==1);
 REQ(bus.txn.applied_revision==7); /* never fabricate XPBD acceptance */
 /* A matching request envelope with DIFFERENT Native candidate Rig must
    never have the same immutable COMMIT fingerprint. */
 G5Receipt first={0},different={0};
 REQ(g5_bus_recover_trusted(&bus,&ui,99,701,&first)==G5_OK);
 G5Bus another={0};
 REQ(g5_bus_init(&another,&original,99,55,7)==G5_OK);
 cmd=make(G5_BEGIN,7);
 REQ(g5_bus_submit_trusted(&another,&ui,&cmd,&out)==G5_OK);
 uint64_t other_draft=out.draft_id;
 REQ(other_draft>0);
 StudioRig other_candidate=original;
 REQ(studio_rig_set_role(&other_candidate,2));
 REQ(g5_bus_stage_native_candidate_trusted(&another,&ui,99,55,
                                           other_draft,&other_candidate)==G5_OK);
 cmd=make(G5_VALIDATE,7);cmd.draft_id=other_draft;
 REQ(g5_bus_submit_trusted(&another,&ui,&cmd,&out)==G5_OK);
 cmd=make(G5_COMMIT,7);cmd.draft_id=other_draft;cmd.request_id=701;
 REQ(g5_bus_submit_trusted(&another,&ui,&cmd,&out)==G5_OK);
 REQ(g5_bus_recover_trusted(&another,&ui,99,701,&different)==G5_OK);
 REQ(memcmp(first.payload_sha256,different.payload_sha256,32)==0);
 REQ(memcmp(first.native_candidate_sha256,different.native_candidate_sha256,32)!=0);
 unsigned nonzero=0;
 for(unsigned i=0;i<32;i++)nonzero|=first.native_candidate_sha256[i];
 REQ(nonzero!=0);
 puts("G5_NATIVE_FULL_RIG_STAGE_PASS multi_operation=1 undo_redo=1 frame_hold=1");
 return 0;
}
