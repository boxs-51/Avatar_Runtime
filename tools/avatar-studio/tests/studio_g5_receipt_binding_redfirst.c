/* G5-ACK-05: negative recovery binding. Opt-in Native synthetic C11.
   Must not be treated as Windows PASS until physically compiled/executed. */
#include "../studio_g5_bus.h"
#include <stdio.h>
#include <string.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"G5_BINDING_RED line=%d\n",__LINE__);return 1;}}while(0)
static G5BusCommand cmd(G5BusOp op,uint64_t revision){
 G5BusCommand c={0};c.ingress=G5_AGENT;c.operation=op;
 c.session=99;c.generation=55;c.expected_revision=revision;
 return c;
}
static int committed(G5Bus *bus,const G5TrustedActor *actor,uint64_t request){
 G5BusResult out={0};G5BusCommand c=cmd(G5_BEGIN,7);
 if(g5_bus_submit_trusted(bus,actor,&c,&out)!=G5_OK)return 0;
 const uint64_t draft=out.draft_id;
 c=cmd(G5_ANCHOR,7);c.draft_id=draft;c.vertex=2;c.enabled=1;
 if(g5_bus_submit_trusted(bus,actor,&c,&out)!=G5_OK)return 0;
 c.operation=G5_VALIDATE;
 if(g5_bus_submit_trusted(bus,actor,&c,&out)!=G5_OK)return 0;
 c.operation=G5_COMMIT;c.request_id=request;
 return g5_bus_submit_trusted(bus,actor,&c,&out)==G5_OK&&out.document_revision==8;
}
int main(void){
 StudioRig rig={0};G5Bus bus={0};G5BusResult out={0};G5Receipt receipt={0};
 const G5TrustedActor alice={G5_AGENT,101};
 REQUIRE(studio_rig_init(&rig,0,42,10));
 REQUIRE(g5_bus_init(&bus,&rig,99,55,7)==G5_OK);
 REQUIRE(committed(&bus,&alice,700));
 G5BusCommand mismatched=cmd(G5_COMMIT,6);
 mismatched.draft_id=1;mismatched.request_id=700;
 REQUIRE(g5_bus_mark_unknown_command_trusted(&bus,&alice,&mismatched)==G5_OK);
 REQUIRE(g5_bus_reconcile_trusted(&bus,&alice,99,700)==G5_ID_CONFLICT);
 REQUIRE(g5_bus_recover_trusted(&bus,&alice,99,700,&receipt)==G5_OK);
 REQUIRE(receipt.operation==G5_COMMIT&&receipt.revision==8);
 G5BusCommand next=cmd(G5_BEGIN,8);
 REQUIRE(g5_bus_submit_trusted(&bus,&alice,&next,&out)==G5_BUSY);
 /* A false reconciliation must never clear a marker or replay a write. */
 REQUIRE(bus.txn.revision==8&&bus.actor_quarantine[0].request_id==700);

 /* Matching request number but different opcode is not equivalent. */
 G5Bus wrong_opcode={0};
 REQUIRE(g5_bus_init(&wrong_opcode,&rig,99,55,7)==G5_OK);
 REQUIRE(committed(&wrong_opcode,&alice,700));
 G5BusCommand undo_claim=cmd(G5_UNDO,7);
 undo_claim.request_id=700;
 REQUIRE(g5_bus_mark_unknown_command_trusted(&wrong_opcode,&alice,&undo_claim)==G5_OK);
 REQUIRE(g5_bus_reconcile_trusted(&wrong_opcode,&alice,99,700)==G5_ID_CONFLICT);
 REQUIRE(wrong_opcode.actor_quarantine[0].request_id==700);
 REQUIRE(wrong_opcode.txn.revision==8);

 G5Bus okay={0};REQUIRE(g5_bus_init(&okay,&rig,99,55,7)==G5_OK);
 REQUIRE(committed(&okay,&alice,700));
 G5BusCommand original=cmd(G5_COMMIT,7);
 original.draft_id=1;original.request_id=700;
 REQUIRE(g5_bus_mark_unknown_command_trusted(&okay,&alice,&original)==G5_OK);
 REQUIRE(g5_bus_reconcile_trusted(&okay,&alice,99,700)==G5_OK);
 REQUIRE(g5_bus_submit_trusted(&okay,&alice,&next,&out)==G5_OK);
 G5BusCommand stop=cmd(G5_ABORT,8);stop.draft_id=out.draft_id;
 REQUIRE(g5_bus_submit_trusted(&okay,&alice,&stop,&out)==G5_OK);

 /* A request without an immutable mutation receipt stays UNKNOWN. */
 G5Bus unknown={0};REQUIRE(g5_bus_init(&unknown,&rig,99,55,7)==G5_OK);
 G5BusCommand absent=cmd(G5_COMMIT,7);
 absent.draft_id=123;absent.request_id=999;
 REQUIRE(g5_bus_mark_unknown_command_trusted(&unknown,&alice,&absent)==G5_OK);
 REQUIRE(g5_bus_reconcile_trusted(&unknown,&alice,99,999)==G5_UNKNOWN);
 REQUIRE(unknown.actor_quarantine[0].request_id==999);
 REQUIRE(g5_bus_submit_trusted(&unknown,&alice,&next,&out)==G5_BUSY);

 /* Invalid provenance or non-mutating operation cannot create markers. */
 G5Bus invalid={0};REQUIRE(g5_bus_init(&invalid,&rig,99,55,7)==G5_OK);
 G5BusCommand bad=cmd(G5_BEGIN,7);bad.request_id=55;
 REQUIRE(g5_bus_mark_unknown_command_trusted(&invalid,&alice,&bad)==G5_INVALID_DRAFT);
 bad=cmd(G5_COMMIT,7);bad.request_id=55;bad.session=98;
 REQUIRE(g5_bus_mark_unknown_command_trusted(&invalid,&alice,&bad)==G5_STALE_SESSION);
 bad.session=99;bad.ingress=G5_UI;
 REQUIRE(g5_bus_mark_unknown_command_trusted(&invalid,&alice,&bad)==G5_STALE_SESSION);
 REQUIRE(invalid.actor_quarantine[0].request_id==0);
 puts("G5_RECEIPT_BINDING_PASS original_operation_payload_required=1");
 return 0;
}
