/* G5 recovery parser RED-first: strict read-only Native message contract. */
#include "../studio_g5_recovery_wire.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"G5_RECOVERY_WIRE_RED line=%d\n",__LINE__);return 1;}}while(0)
#define QUERY(s,owner,session,out,cap) g5_recovery_handle(&bus,owner,session,(const unsigned char*)(s),sizeof(s)-1,out,cap)
int main(void){
 StudioRig rig={0};G5Bus bus={0};G5BusResult r={0};char reply[120]={0};
 CHECK(studio_rig_init(&rig,0,42,10));
 CHECK(g5_bus_init(&bus,&rig,99,55,7)==G5_OK);
 G5BusCommand c={0};
 c.ingress=G5_AGENT;c.operation=G5_BEGIN;c.session=99;
 c.generation=55;c.expected_revision=7;
 CHECK(g5_bus_submit(&bus,&c,&r)==G5_OK);
 uint64_t draft=r.draft_id;
 c.operation=G5_ANCHOR;c.draft_id=draft;c.vertex=2;c.enabled=1;
 CHECK(g5_bus_submit(&bus,&c,&r)==G5_OK);
 c.operation=G5_VALIDATE;
 CHECK(g5_bus_submit(&bus,&c,&r)==G5_OK);
 c.operation=G5_COMMIT;c.request_id=700;
 CHECK(g5_bus_submit(&bus,&c,&r)==G5_OK);
 CHECK(g5_bus_mark_unknown(&bus,G5_AGENT,700)==G5_OK);
 CHECK(QUERY("STATUS 700",G5_AGENT,99,reply,sizeof reply)==G5_OK);
 CHECK(strcmp(reply,"outcome=COMMITTED request=700 revision=8")==0);
 CHECK(bus.quarantined_request[G5_AGENT]==700);
 CHECK(QUERY("STATUS 700",G5_UI,99,reply,sizeof reply)==G5_STALE_SESSION);
 CHECK(QUERY("STATUS 700",G5_AGENT,98,reply,sizeof reply)==G5_STALE_SESSION);
 CHECK(QUERY("STATUS 701",G5_AGENT,99,reply,sizeof reply)==G5_UNKNOWN);
 CHECK(strcmp(reply,"outcome=UNKNOWN request=701 revision=0")==0);
 CHECK(QUERY("STATUS 700\n",G5_AGENT,99,reply,sizeof reply)==G5_INVALID_DRAFT);
 CHECK(QUERY("STATUS 700 extra",G5_AGENT,99,reply,sizeof reply)==G5_INVALID_DRAFT);
 CHECK(QUERY("STATUS 000700",G5_AGENT,99,reply,sizeof reply)==G5_INVALID_DRAFT);
 CHECK(QUERY("COMMIT 700",G5_AGENT,99,reply,sizeof reply)==G5_INVALID_DRAFT);
 CHECK(QUERY("STATUS -1",G5_AGENT,99,reply,sizeof reply)==G5_INVALID_DRAFT);
 CHECK(QUERY("STATUS 9223372036854775808",G5_AGENT,99,reply,sizeof reply)==G5_INVALID_DRAFT);
 CHECK(QUERY("STATUS 700",G5_AGENT,99,reply,10)==G5_OUT_OF_BUDGET);
 const unsigned char zero_byte[]={'S','T','A','T','U','S',' ', '7','0','0',0,'8'};
 CHECK(g5_recovery_handle(&bus,G5_AGENT,99,zero_byte,sizeof zero_byte,
                          reply,sizeof reply)==G5_INVALID_DRAFT);
 CHECK(bus.txn.revision==8&&bus.txn.live.anchor_count==1);
 CHECK(bus.quarantined_request[G5_AGENT]==700);
 puts("G5_RECOVERY_WIRE_PASS read_only=1 strict_bound=1 no_mutation=1");
 return 0;
}
