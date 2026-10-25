#include "studio_rig_authority.h"
#include <string.h>

static G5Ingress ingress(StudioRigSource source){
 switch(source){
 case STUDIO_RIG_UI:return G5_UI;
 case STUDIO_RIG_V1:return G5_LEGACY;
 case STUDIO_RIG_AGENT:return G5_AGENT;
 default:return 0;
 }
}
static StudioRigAuthorityCode translate(G5Code code){
 switch(code){
 case G5_OK:return STUDIO_RIG_AUTH_OK;
 case G5_BUSY:return STUDIO_RIG_AUTH_BUSY;
 case G5_STALE_MODEL:
 case G5_STALE_SESSION:
 case G5_REVISION_CONFLICT:return STUDIO_RIG_AUTH_STALE;
 case G5_UNKNOWN:return STUDIO_RIG_AUTH_UNKNOWN;
 default:return STUDIO_RIG_AUTH_REJECT;
 }
}
G5Code studio_rig_authority_init(StudioRigAuthority *a,
 const StudioRig *initial,uint64_t session,uint64_t generation,uint64_t rev){
 if(!a)return G5_INVALID_DRAFT;
 return g5_bus_init(&a->bus,initial,session,generation,rev);
}
StudioRigAuthorityCode studio_rig_authority_submit(
 StudioRigAuthority *a,StudioRigSource source,
 uint64_t session,uint64_t generation,uint64_t expected,
 uint64_t request_id,StudioRigCommand command,void *argument,
 StudioRigAuthorityResult *out){
 if(!out)return STUDIO_RIG_AUTH_REJECT;
 memset(out,0,sizeof *out);
 out->code=STUDIO_RIG_AUTH_REJECT;
 if(!a||!command||!request_id||!ingress(source))return out->code;
 G5Txn *txn=&a->bus.txn;
 if(session!=txn->session_epoch||generation!=txn->model_generation||
    expected!=txn->revision){
  out->code=STUDIO_RIG_AUTH_STALE;return out->code;
 }
 if(txn->has_draft){out->code=STUDIO_RIG_AUTH_BUSY;return out->code;}
 /* IDs are unique across all ingresses, including unknown-outcome recovery. */
 for(unsigned i=0;i<txn->receipt_count;i++){
  if(txn->receipts[i].request_id==request_id)return out->code;
 }
 if(txn->receipt_count>=G5_RECEIPT_CAP||
    txn->undo_count>=G5_HISTORY_CAP)return out->code;
 StudioRig candidate=txn->live;
 uint64_t temp_revision=txn->revision;
 StudioRigDispatchCode edited=studio_rig_dispatch(&candidate,&temp_revision,
   generation,generation,expected,command,argument);
 if(edited==STUDIO_RIG_DISPATCH_UNCHANGED){
  out->revision=txn->revision;out->code=STUDIO_RIG_AUTH_UNCHANGED;
  return out->code;
 }
 if(edited!=STUDIO_RIG_DISPATCH_OK)return out->code;
 const G5TrustedActor actor={ingress(source),1};
 G5BusCommand cmd={0};
 cmd.ingress=actor.ingress;cmd.operation=G5_BEGIN;
 cmd.session=session;cmd.generation=generation;cmd.expected_revision=expected;
 G5BusResult result={0};
 G5Code code=g5_bus_submit_trusted(&a->bus,&actor,&cmd,&result);
 if(code!=G5_OK){out->code=translate(code);return out->code;}
 uint64_t draft_id=result.draft_id;
 code=g5_bus_stage_native_candidate_trusted(&a->bus,&actor,
      session,generation,draft_id,&candidate);
 if(code==G5_OK){
  cmd.operation=G5_VALIDATE;cmd.draft_id=draft_id;
  code=g5_bus_submit_trusted(&a->bus,&actor,&cmd,&result);
 }
 if(code==G5_OK){
  cmd.operation=G5_COMMIT;cmd.request_id=request_id;
  code=g5_bus_submit_trusted(&a->bus,&actor,&cmd,&result);
 }
 if(code!=G5_OK){
  /* Do not abort on uncertain post-effect results. */
  if(code!=G5_UNKNOWN&&a->bus.txn.has_draft){
   cmd.operation=G5_ABORT;cmd.request_id=0;
   (void)g5_bus_submit_trusted(&a->bus,&actor,&cmd,&result);
  }
  out->code=translate(code);return out->code;
 }
 if(!result.committed||result.document_revision!=temp_revision){
  out->code=STUDIO_RIG_AUTH_UNKNOWN;return out->code;
 }
 out->revision=result.document_revision;
 out->code=STUDIO_RIG_AUTH_OK;
 return out->code;
}
