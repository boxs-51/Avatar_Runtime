/* G5 RED-first internal routing + read-only status, NOT Studio IPC E2E. */
#include "../studio_g5_bus.h"
#include <stdio.h>
#define CHECK(expr) do {if(!(expr)){fprintf(stderr,"G5_BUS_RED line=%d\n",__LINE__);return 1;}}while(0)
static G5BusCommand cmd(G5Ingress from,G5BusOp op,uint64_t revision){
 G5BusCommand c={0};
 c.ingress=from;c.operation=op;c.session=99;c.generation=55;
 c.expected_revision=revision;
 return c;
}
int main(void){
 StudioRig rig={0};G5Bus bus={0};G5BusResult out={0};G5Receipt receipt={0};
 CHECK(studio_rig_init(&rig,0,123,12));
 CHECK(g5_bus_init(&bus,&rig,99,55,7)==G5_OK);
 G5BusCommand ui=cmd(G5_UI,G5_BEGIN,7);
 CHECK(g5_bus_submit(&bus,&ui,&out)==G5_OK);
 uint64_t ui_draft=out.draft_id;
 CHECK(ui_draft!=0&&bus.txn.revision==7);
 G5BusCommand agent=cmd(G5_AGENT,G5_BEGIN,7);
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_BUSY);
 G5BusCommand legacy=cmd(G5_LEGACY,G5_LEGACY_ANCHOR,7);
 legacy.vertex=2;legacy.enabled=1;legacy.request_id=10;
 CHECK(g5_bus_submit(&bus,&legacy,&out)==G5_BUSY);
 ui=cmd(G5_UI,G5_ANCHOR,7);ui.draft_id=ui_draft;
 ui.vertex=2;ui.enabled=1;
 CHECK(g5_bus_submit(&bus,&ui,&out)==G5_OK);
 ui.operation=G5_VALIDATE;
 CHECK(g5_bus_submit(&bus,&ui,&out)==G5_OK);
 ui.operation=G5_COMMIT;ui.request_id=11;
 CHECK(g5_bus_submit(&bus,&ui,&out)==G5_OK);
 CHECK(out.document_revision==8&&bus.txn.live.anchor_count==1);
 CHECK(bus.txn.applied_revision==7);
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_REVISION_CONFLICT);
 agent=cmd(G5_AGENT,G5_BEGIN,8);
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_OK);
 uint64_t agent_draft=out.draft_id;
 agent=cmd(G5_AGENT,G5_ANCHOR,8);agent.draft_id=agent_draft;
 agent.vertex=4;agent.enabled=1;
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_OK);
 agent.operation=G5_VALIDATE;
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_OK);
 agent.operation=G5_COMMIT;agent.request_id=14;
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_OK);
 CHECK(out.document_revision==9);
 /* Fault injection: caller lost the ACK AFTER owner committed. */
 CHECK(g5_bus_mark_unknown(&bus,G5_AGENT,14)==G5_OK);
 agent=cmd(G5_AGENT,G5_BEGIN,9);
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_BUSY);
 CHECK(g5_bus_recover(&bus,G5_AGENT,99,14,&receipt)==G5_OK);
 CHECK(receipt.request_id==14&&receipt.revision==9&&
       receipt.outcome==G5_COMMITTED);
 CHECK(g5_bus_recover(&bus,G5_UI,99,14,&receipt)==G5_STALE_SESSION);
 CHECK(g5_bus_recover(&bus,G5_AGENT,99,999,&receipt)==G5_UNKNOWN);
 CHECK(g5_bus_recover(&bus,G5_AGENT,98,14,&receipt)==G5_STALE_SESSION);
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_BUSY); /* read did not unblock */
 CHECK(g5_bus_reconcile(&bus,G5_AGENT,99,999)==G5_ID_CONFLICT);
 CHECK(g5_bus_reconcile(&bus,G5_AGENT,99,14)==G5_OK);
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_OK);
 agent=cmd(G5_AGENT,G5_ABORT,9);agent.draft_id=out.draft_id;
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_OK);
 legacy=cmd(G5_LEGACY,G5_LEGACY_ANCHOR,9);
 legacy.vertex=5;legacy.enabled=1;legacy.request_id=15;
 CHECK(g5_bus_submit(&bus,&legacy,&out)==G5_OK);
 CHECK(out.document_revision==10&&bus.txn.live.anchor_count==3);
 ui=cmd(G5_UI,G5_BEGIN,9);
 CHECK(g5_bus_submit(&bus,&ui,&out)==G5_REVISION_CONFLICT);
 /* No arbitrary client ingress enum or read exposure. */
 agent=cmd((G5Ingress)42,G5_BEGIN,10);
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_STALE_SESSION);
 CHECK(g5_bus_recover(&bus,(G5Ingress)42,99,14,&receipt)==G5_STALE_SESSION);
 CHECK(g5_bus_mark_unknown(&bus,G5_AGENT,999)==G5_OK);
 CHECK(g5_bus_reconcile(&bus,G5_AGENT,99,999)==G5_UNKNOWN);
 agent=cmd(G5_AGENT,G5_BEGIN,10);
 CHECK(g5_bus_submit(&bus,&agent,&out)==G5_BUSY);
 puts("G5_OWNER_BUS_RECOVERY_PASS shared_lease=1 unknown=quarantined no_replay=1");
 return 0;
}
