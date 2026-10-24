#include "studio_g5_bus.h"
#include <string.h>
#include "studio_g5_sha256.h"
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


/* Actor ID is a trusted local-session label; the high nibble binds ingress.
   A JSON field containing this ID is never an authorization credential. */
static G5Code trusted_owner(const G5TrustedActor *actor,uint64_t *key){
 if(!actor||!key||!valid_ingress(actor->ingress)||
    !actor->owner||actor->owner>=UINT64_C(0x1000000000000000))
  return G5_STALE_SESSION;
 *key=((uint64_t)actor->ingress<<60)|actor->owner;
 return G5_OK;
}
static void canonical_digest(const G5TrustedActor *actor,
                             const G5BusCommand *cmd,unsigned char digest[32]){
 /* Versioned canonical semantic envelope. No padding or raw C floats/geometry.
    Big-endian integers make the encoding compiler/architecture independent. */
 static const char domain[]="ARS-G5-REQUEST-v1";
 const uint64_t fields[]={
   (uint64_t)actor->ingress,actor->owner,(uint64_t)cmd->operation,
   cmd->session,cmd->generation,cmd->expected_revision,
   cmd->draft_id,cmd->request_id,
   (cmd->operation==G5_ANCHOR||cmd->operation==G5_LEGACY_ANCHOR)
      ?(uint64_t)(int64_t)cmd->vertex:0,
   (cmd->operation==G5_ANCHOR||cmd->operation==G5_LEGACY_ANCHOR)
      ?(uint64_t)(int64_t)cmd->enabled:0
 };
 G5Sha256 hash;g5_sha_init(&hash);
 g5_sha_update(&hash,domain,sizeof domain-1);
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++){
  unsigned char bytes[8];
  for(unsigned j=0;j<8;j++)bytes[j]=(unsigned char)(fields[i]>>(56-8*j));
  g5_sha_update(&hash,bytes,sizeof bytes);
 }
 g5_sha_final(&hash,digest);
}
static void bind_receipt(G5Txn *txn,uint64_t owner,
                         const G5TrustedActor *actor,const G5BusCommand *cmd){
 /* Called synchronously on the one Native owner thread after publication,
    before external ACK or another command can be observed. */
 G5Receipt *r=&txn->receipts[txn->receipt_count-1];
 if(r->owner==owner&&r->request_id==cmd->request_id){
  r->operation=(uint32_t)cmd->operation;
  canonical_digest(actor,cmd,r->payload_sha256);
 }
}
static G5ActorQuarantine *actor_slot(G5Bus *bus,
                                     const G5TrustedActor *actor){
 for(unsigned i=0;i<G5_ACTOR_QUARANTINE_CAP;i++)
  if(bus->actor_quarantine[i].request_id&&
     bus->actor_quarantine[i].ingress==actor->ingress&&
     bus->actor_quarantine[i].owner==actor->owner)
   return &bus->actor_quarantine[i];
 return NULL;
}
G5Code g5_bus_submit_trusted(G5Bus *bus,const G5TrustedActor *actor,
                             const G5BusCommand *cmd,G5BusResult *out){
 if(!out)return G5_INVALID_DRAFT;
 memset(out,0,sizeof *out);
 uint64_t owner=0;
 G5Code code=trusted_owner(actor,&owner);
 if(!bus||!cmd||code!=G5_OK||cmd->ingress!=actor->ingress){
  out->code=G5_STALE_SESSION;return out->code;
 }
 if(actor_slot(bus,actor)){out->code=G5_BUSY;return out->code;}
 G5Txn *txn=&bus->txn;uint64_t revision=0,draft=0;
 code=G5_INVALID_DRAFT;
 switch(cmd->operation){
 case G5_BEGIN:
  code=g5_txn_begin(txn,cmd->session,cmd->generation,
                    cmd->expected_revision,owner,&draft);
  if(code==G5_OK)out->draft_id=draft;
  break;
 case G5_ANCHOR:
  code=g5_txn_anchor(txn,cmd->session,cmd->generation,owner,
                     cmd->draft_id,cmd->vertex,cmd->enabled);
  break;
 case G5_VALIDATE:
  code=g5_txn_validate(txn,cmd->session,cmd->generation,owner,cmd->draft_id);
  break;
 case G5_COMMIT:
  code=g5_txn_commit(txn,cmd->session,cmd->generation,owner,
                     cmd->draft_id,cmd->expected_revision,
                     cmd->request_id,&revision);
  if(code==G5_OK){out->committed=1;out->document_revision=revision;}
  break;
 case G5_ABORT:
  code=g5_txn_abort(txn,cmd->session,cmd->generation,owner,cmd->draft_id);
  break;
 case G5_UNDO:
  code=g5_txn_undo(txn,cmd->session,cmd->generation,
                   cmd->expected_revision,owner,cmd->request_id,&revision);
  if(code==G5_OK){out->committed=1;out->document_revision=revision;}
  break;
 case G5_REDO:
  code=g5_txn_redo(txn,cmd->session,cmd->generation,
                   cmd->expected_revision,owner,cmd->request_id,&revision);
  if(code==G5_OK){out->committed=1;out->document_revision=revision;}
  break;
 case G5_LEGACY_ANCHOR: {
  if(actor->ingress!=G5_LEGACY){code=G5_STALE_SESSION;break;}
  /* One-shot native compatibility transaction, no active lease bypass. */
  if(txn->has_draft){code=G5_BUSY;break;}
  code=g5_txn_begin(txn,cmd->session,cmd->generation,
                    cmd->expected_revision,owner,&draft);
  if(code!=G5_OK)break;
  code=g5_txn_anchor(txn,cmd->session,cmd->generation,owner,draft,
                     cmd->vertex,cmd->enabled);
  if(code==G5_OK)
   code=g5_txn_validate(txn,cmd->session,cmd->generation,owner,draft);
  if(code==G5_OK)
   code=g5_txn_commit(txn,cmd->session,cmd->generation,owner,draft,
                      cmd->expected_revision,cmd->request_id,&revision);
  if(code==G5_OK){out->committed=1;out->document_revision=revision;}
  else (void)g5_txn_abort(txn,cmd->session,cmd->generation,owner,draft);
  break;
 }
 default:code=G5_INVALID_DRAFT;break;
 }
 if(code==G5_OK&&out->committed)
  bind_receipt(txn,owner,actor,cmd);
 out->code=code;
 return code;
}
G5Code g5_bus_mark_unknown_trusted(G5Bus *bus,
                                   const G5TrustedActor *actor,uint64_t request){
 uint64_t owner=0;
 if(!bus||trusted_owner(actor,&owner)!=G5_OK||!request)
  return G5_INVALID_DRAFT;
 if(actor_slot(bus,actor))return G5_BUSY;
 for(unsigned i=0;i<G5_ACTOR_QUARANTINE_CAP;i++){
  G5ActorQuarantine *slot=&bus->actor_quarantine[i];
  if(!slot->request_id){
   slot->ingress=actor->ingress;slot->owner=actor->owner;
   slot->request_id=request;return G5_OK;
  }
 }
 return G5_OUT_OF_BUDGET;
}
G5Code g5_bus_recover_trusted(const G5Bus *bus,
                              const G5TrustedActor *actor,uint64_t session,
                              uint64_t request,G5Receipt *receipt){
 uint64_t owner=0;
 if(!bus||trusted_owner(actor,&owner)!=G5_OK||!receipt||!request)
  return G5_STALE_SESSION;
 return g5_txn_receipt(&bus->txn,session,owner,request,receipt);
}
G5Code g5_bus_reconcile_trusted(G5Bus *bus,const G5TrustedActor *actor,
                                uint64_t session,uint64_t request){
 uint64_t owner=0;
 if(!bus||trusted_owner(actor,&owner)!=G5_OK||!request)return G5_STALE_SESSION;
 G5ActorQuarantine *slot=actor_slot(bus,actor);
 if(!slot||slot->request_id!=request){
  for(unsigned i=0;i<G5_ACTOR_QUARANTINE_CAP;i++)
   if(bus->actor_quarantine[i].request_id==request)
    return G5_STALE_SESSION;
  return G5_ID_CONFLICT;
 }
 G5Receipt receipt={0};
 G5Code code=g5_bus_recover_trusted(bus,actor,session,request,&receipt);
 if(code!=G5_OK)return code;
 if(receipt.outcome!=G5_COMMITTED&&receipt.outcome!=G5_REJECTED&&
    receipt.outcome!=G5_ABORTED)return G5_UNKNOWN;
 memset(slot,0,sizeof *slot);
 return G5_OK;
}
