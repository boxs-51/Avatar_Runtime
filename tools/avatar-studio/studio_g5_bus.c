#include "studio_g5_bus.h"
#include <string.h>
static int valid_ingress(G5Ingress src){
 return src==G5_UI||src==G5_LEGACY||src==G5_AGENT;
}
G5Code g5_bus_init(G5Bus *bus,const StudioRig *rig,uint64_t session,
                    uint64_t generation,uint64_t revision){
 if(!bus)return G5_INVALID_DRAFT;
 memset(bus,0,sizeof *bus);
 return g5_txn_init(&bus->txn,rig,session,generation,revision);
}
static G5Code legacy_anchor(G5Bus *bus,const G5BusCommand *cmd,
                            G5BusResult *out){
 G5Txn *t=&bus->txn;
 if(t->has_draft)return G5_BUSY;
 uint64_t draft=0,revision=0;
 const uint64_t owner=(uint64_t)cmd->ingress;
 G5Code code=g5_txn_begin(t,cmd->session,cmd->generation,
                          cmd->expected_revision,owner,&draft);
 if(code!=G5_OK)return code;
 code=g5_txn_anchor(t,cmd->session,cmd->generation,owner,draft,
                    cmd->vertex,cmd->enabled);
 if(code==G5_OK)code=g5_txn_validate(t,cmd->session,cmd->generation,
                                    owner,draft);
 if(code==G5_OK)code=g5_txn_commit(t,cmd->session,cmd->generation,
                                  owner,draft,cmd->expected_revision,
                                  cmd->request_id,&revision);
 if(code!=G5_OK){
  (void)g5_txn_abort(t,cmd->session,cmd->generation,owner,draft);
  return code;
 }
 out->committed=1;out->document_revision=revision;
 return G5_OK;
}
G5Code g5_bus_submit(G5Bus *bus,const G5BusCommand *cmd,G5BusResult *out){
 if(!out)return G5_INVALID_DRAFT;
 memset(out,0,sizeof *out);
 if(!bus||!cmd||!valid_ingress(cmd->ingress)){
  out->code=G5_STALE_SESSION;return out->code;
 }
 if(bus->quarantined_request[(int)cmd->ingress]){
  out->code=G5_BUSY;return out->code;
 }
 const uint64_t owner=(uint64_t)cmd->ingress;
 G5Txn *t=&bus->txn;uint64_t revision=0,draft=0;
 G5Code code=G5_INVALID_DRAFT;
 switch(cmd->operation){
 case G5_BEGIN:
  code=g5_txn_begin(t,cmd->session,cmd->generation,
                    cmd->expected_revision,owner,&draft);
  if(code==G5_OK)out->draft_id=draft;
  break;
 case G5_ANCHOR:
  code=g5_txn_anchor(t,cmd->session,cmd->generation,owner,
                     cmd->draft_id,cmd->vertex,cmd->enabled);
  break;
 case G5_VALIDATE:
  code=g5_txn_validate(t,cmd->session,cmd->generation,owner,cmd->draft_id);
  break;
 case G5_COMMIT:
  code=g5_txn_commit(t,cmd->session,cmd->generation,owner,
                     cmd->draft_id,cmd->expected_revision,
                     cmd->request_id,&revision);
  if(code==G5_OK){out->committed=1;out->document_revision=revision;}
  break;
 case G5_ABORT:
  code=g5_txn_abort(t,cmd->session,cmd->generation,owner,cmd->draft_id);
  break;
 case G5_UNDO:
  code=g5_txn_undo(t,cmd->session,cmd->generation,cmd->expected_revision,
                   owner,cmd->request_id,&revision);
  if(code==G5_OK){out->committed=1;out->document_revision=revision;}
  break;
 case G5_REDO:
  code=g5_txn_redo(t,cmd->session,cmd->generation,cmd->expected_revision,
                   owner,cmd->request_id,&revision);
  if(code==G5_OK){out->committed=1;out->document_revision=revision;}
  break;
 case G5_LEGACY_ANCHOR:
  /* Explicit one-shot CAS compatibility; denies an existing UI/Agent lease. */
  if(cmd->ingress!=G5_LEGACY){code=G5_STALE_SESSION;break;}
  code=legacy_anchor(bus,cmd,out);
  break;
 default:
  code=G5_INVALID_DRAFT;
  break;
 }
 out->code=code;
 return code;
}
G5Code g5_bus_mark_unknown(G5Bus *bus,G5Ingress ingress,
                            uint64_t original_request){
 if(!bus||!valid_ingress(ingress)||!original_request)return G5_INVALID_DRAFT;
 uint64_t *slot=&bus->quarantined_request[(int)ingress];
 if(*slot)return G5_BUSY;
 *slot=original_request;return G5_OK;
}
G5Code g5_bus_recover(const G5Bus *bus,G5Ingress ingress,uint64_t session,
                      uint64_t original_request,G5Receipt *receipt){
 if(!bus||!valid_ingress(ingress)||!session||!original_request||!receipt)
  return G5_STALE_SESSION;
 return g5_txn_receipt(&bus->txn,session,(uint64_t)ingress,
                       original_request,receipt);
}
G5Code g5_bus_reconcile(G5Bus *bus,G5Ingress ingress,uint64_t session,
                        uint64_t original_request){
 if(!bus||!valid_ingress(ingress)||!session||!original_request)
  return G5_STALE_SESSION;
 if(bus->quarantined_request[(int)ingress]!=original_request)
  return G5_ID_CONFLICT;
 G5Receipt receipt={0};
 G5Code code=g5_bus_recover(bus,ingress,session,original_request,&receipt);
 if(code!=G5_OK)return code;
 /* The receipt comes from the authoritative Native owner and is terminal.
    Never clear quarantine based on absence or a caller-supplied verdict. */
 if(receipt.outcome!=G5_COMMITTED&&receipt.outcome!=G5_REJECTED&&
    receipt.outcome!=G5_ABORTED)return G5_UNKNOWN;
 bus->quarantined_request[(int)ingress]=0;
 return G5_OK;
}
