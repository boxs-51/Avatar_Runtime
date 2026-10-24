#include "../studio_g5_bus.h"
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#define G5_TEST_EXPORT __declspec(dllexport)
#else
#define G5_TEST_EXPORT
#endif
/* TEST-ONLY synthetic owner. Exposes no real model/Host capability. */
static G5Bus owner_bus;
static uint64_t fixture_epoch=0;
static const G5TrustedActor owner_actor={G5_AGENT,101};
static G5BusCommand original_command;
static int has_original=0;
static G5BusCommand command(G5BusOp op,uint64_t revision){
 G5BusCommand cmd={0};
 cmd.ingress=G5_AGENT;cmd.operation=op;cmd.session=fixture_epoch;
 cmd.generation=55;cmd.expected_revision=revision;
 return cmd;
}
G5_TEST_EXPORT int g5_fixture_init(uint64_t session_epoch){
 if(!session_epoch||session_epoch>=UINT64_C(0x8000000000000000))return -1;
 fixture_epoch=session_epoch;
 StudioRig rig={0};has_original=0;memset(&owner_bus,0,sizeof owner_bus);
 if(!studio_rig_init(&rig,0,42,10))return -1;
 return g5_bus_init(&owner_bus,&rig,fixture_epoch,55,7);
}
G5_TEST_EXPORT int g5_fixture_commit_without_ack(uint64_t request){
 G5BusResult result={0};
 if(!request||has_original)return -1;
 G5BusCommand c=command(G5_BEGIN,7);
 if(g5_bus_submit_trusted(&owner_bus,&owner_actor,&c,&result)!=G5_OK)return -2;
 const uint64_t draft=result.draft_id;
 c=command(G5_ANCHOR,7);c.draft_id=draft;c.vertex=2;c.enabled=1;
 if(g5_bus_submit_trusted(&owner_bus,&owner_actor,&c,&result)!=G5_OK)return -3;
 c.operation=G5_VALIDATE;
 if(g5_bus_submit_trusted(&owner_bus,&owner_actor,&c,&result)!=G5_OK)return -4;
 c.operation=G5_COMMIT;c.request_id=request;
 if(g5_bus_submit_trusted(&owner_bus,&owner_actor,&c,&result)!=G5_OK)return -5;
 if(result.document_revision!=8)return -6;
 original_command=c;has_original=1;
 /* The Host has committed and generated a receipt, but outbound ACK is lost. */
 return g5_bus_mark_unknown_command_trusted(&owner_bus,&owner_actor,&c);
}
/* A request has become uncertain before owner commit; no receipt exists. */
G5_TEST_EXPORT int g5_fixture_hold_before_commit(uint64_t request){
 if(!request||has_original)return -1;
 G5BusCommand c=command(G5_COMMIT,7);
 c.draft_id=1;c.request_id=request;
 has_original=1;original_command=c;
 return g5_bus_mark_unknown_command_trusted(&owner_bus,&owner_actor,&c);
}
G5_TEST_EXPORT int g5_fixture_read_only_status(uint64_t request,uint64_t *revision){
 G5Receipt receipt={0};if(revision)*revision=0;
 const G5Code code=g5_bus_recover_trusted(&owner_bus,&owner_actor,fixture_epoch,request,&receipt);
 if(code==G5_OK){
  if(!revision||receipt.outcome!=G5_COMMITTED||receipt.operation!=G5_COMMIT)
   return -1;
  *revision=receipt.revision;
 }
 return code;
}
G5_TEST_EXPORT int g5_fixture_try_new_begin(void){
 G5BusResult result={0};
 G5BusCommand c=command(G5_BEGIN,owner_bus.txn.revision);
 return g5_bus_submit_trusted(&owner_bus,&owner_actor,&c,&result);
}
G5_TEST_EXPORT int g5_fixture_reconcile(uint64_t request){
 return g5_bus_reconcile_trusted(&owner_bus,&owner_actor,fixture_epoch,request);
}
G5_TEST_EXPORT uint64_t g5_fixture_revision(void){return owner_bus.txn.revision;}
