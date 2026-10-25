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


/* G5 functional Agent tools test surface: never linked into shipping Studio.
   Numeric receipts only; no paths, geometry export, or network service. */
G5_TEST_EXPORT int g5_fixture_tool_submit(unsigned opcode,uint64_t session,
 uint64_t generation,uint64_t expected,uint64_t draft,uint64_t request,
 int vertex,int enabled,uint64_t *out_draft,uint64_t *out_revision){
 if(out_draft)*out_draft=0;
 if(out_revision)*out_revision=0;
 if(!out_draft||!out_revision||opcode<G5_BEGIN||opcode>G5_REDO)
  return G5_INVALID_DRAFT;
 G5BusCommand cmd=command((G5BusOp)opcode,expected);
 cmd.session=session;cmd.generation=generation;
 cmd.draft_id=draft;cmd.request_id=request;
 cmd.vertex=vertex;cmd.enabled=enabled;
 G5BusResult result={0};
 G5Code code=g5_bus_submit_trusted(&owner_bus,&owner_actor,&cmd,&result);
 if(code==G5_OK){
  *out_draft=result.draft_id;
  *out_revision=result.document_revision;
 }
 return code;
}
G5_TEST_EXPORT int g5_fixture_tool_status(uint64_t session,
 uint64_t generation,uint64_t *out,unsigned capacity){
 if(!out||capacity<7)return G5_INVALID_DRAFT;
 for(unsigned i=0;i<7;i++)out[i]=0; /* fixed ABI: never write arbitrary capacity */
 if(!session||session!=owner_bus.txn.session_epoch)return G5_STALE_SESSION;
 if(!generation||generation!=owner_bus.txn.model_generation)
  return G5_STALE_MODEL;
 out[0]=owner_bus.txn.revision;
 out[1]=owner_bus.txn.applied_revision;
 out[2]=owner_bus.txn.has_draft?1:0;
 out[3]=owner_bus.txn.has_draft?owner_bus.txn.draft_id:0;
 out[4]=owner_bus.txn.has_draft?owner_bus.txn.base_revision:0;
 out[5]=owner_bus.txn.undo_count;
 out[6]=owner_bus.txn.redo_count;
 return G5_OK;
}
G5_TEST_EXPORT int g5_fixture_tool_receipt(uint64_t session,
 uint64_t generation,uint64_t request,uint64_t *revision,unsigned *op){
 if(revision)*revision=0;
 if(op)*op=0;
 if(!revision||!op||!request)return G5_INVALID_DRAFT;
 if(session!=owner_bus.txn.session_epoch)return G5_STALE_SESSION;
 if(generation!=owner_bus.txn.model_generation)return G5_STALE_MODEL;
 G5Receipt receipt={0};
 G5Code code=g5_bus_recover_trusted(&owner_bus,&owner_actor,session,request,&receipt);
 if(code==G5_OK){
  if(receipt.outcome!=G5_COMMITTED)return G5_UNKNOWN;
  *revision=receipt.revision;*op=receipt.operation;
 }
 return code;
}

/* Synthetic post-effect ACK-loss injection: bind the exact COMMIT envelope
   to its original Native receipt. This is NOT a Studio writer API. */
G5_TEST_EXPORT int g5_fixture_tool_mark_unknown_commit(uint64_t session,
 uint64_t generation,uint64_t expected,uint64_t draft,uint64_t request){
 if(!session||session!=owner_bus.txn.session_epoch)return G5_STALE_SESSION;
 if(!generation||generation!=owner_bus.txn.model_generation)return G5_STALE_MODEL;
 if(!request||!draft)return G5_INVALID_DRAFT;
 G5BusCommand cmd=command(G5_COMMIT,expected);
 cmd.session=session;cmd.generation=generation;
 cmd.draft_id=draft;cmd.request_id=request;
 return g5_bus_mark_unknown_command_trusted(&owner_bus,&owner_actor,&cmd);
}
G5_TEST_EXPORT int g5_fixture_tool_reconcile(uint64_t session,
 uint64_t generation,uint64_t request){
 if(!session||session!=owner_bus.txn.session_epoch)return G5_STALE_SESSION;
 if(!generation||generation!=owner_bus.txn.model_generation)return G5_STALE_MODEL;
 if(!request)return G5_INVALID_DRAFT;
 return g5_bus_reconcile_trusted(&owner_bus,&owner_actor,session,request);
}
