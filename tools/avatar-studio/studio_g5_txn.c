#include "studio_g5_txn.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#define G5_REV_MAX UINT64_C(0x7fffffffffffffff)

/* No paths, IPC or model allocation: all state owned by one Native caller. */
static int rig_valid(const StudioRig *r){
 if(!r||r->model_signature==0||r->drawable<0||
    r->vertex_count<3||r->vertex_count>2048||
    r->role<0||r->role>3||
    r->anchor_count<0||r->anchor_count>STUDIO_RIG_MAX_ANCHORS||
    r->anchor_count>r->vertex_count||
    r->beam_count<0||r->beam_count>STUDIO_RIG_MAX_BEAMS||
    r->beam_count>r->vertex_count*(r->vertex_count-1)/2||
    r->parent_count<0||r->parent_count>STUDIO_RIG_MAX_PARENTS||
    r->parent_count>=r->vertex_count||
    r->weight_count<0||r->weight_count>STUDIO_RIG_MAX_WEIGHTS||
    r->weight_count>r->vertex_count||
    r->cross_count<0||r->cross_count>STUDIO_RIG_MAX_CROSS)return 0;
 for(int i=0;i<r->anchor_count;i++){
  int v=r->anchors[i];if(v<0||v>=r->vertex_count)return 0;
  for(int j=0;j<i;j++)if(r->anchors[j]==v)return 0;
 }
 for(int i=0;i<r->beam_count;i++){
  int a=r->beams[i].a,b=r->beams[i].b;
  if(a<0||b<0||a>=b||b>=r->vertex_count)return 0;
  for(int j=0;j<i;j++)
   if(r->beams[j].a==a&&r->beams[j].b==b)return 0;
 }
 for(int i=0;i<r->parent_count;i++){
  int a=r->parents[i].parent,b=r->parents[i].child;
  float strength=r->parents[i].stiffness;
  if(a<0||b<0||a==b||a>=r->vertex_count||b>=r->vertex_count||
     !isfinite(strength)||strength<0||strength>1)return 0;
  int cursor=a;
  for(int hop=0;hop<=r->parent_count;hop++){
   if(cursor==b)return 0;
   int next=-1;
   for(int j=0;j<r->parent_count;j++)
    if(r->parents[j].child==cursor){next=r->parents[j].parent;break;}
   if(next<0)break;
   cursor=next;
   if(hop==r->parent_count)return 0;
  }
  for(int j=0;j<i;j++)if(r->parents[j].child==b)return 0;
 }
 for(int i=0;i<r->weight_count;i++){
  int v=r->weights[i].vertex;float w=r->weights[i].weight;
  if(v<0||v>=r->vertex_count||!isfinite(w)||w<0||w>1)return 0;
  for(int j=0;j<i;j++)if(r->weights[j].vertex==v)return 0;
 }
 for(int i=0;i<r->cross_count;i++){
  const StudioRigCross *c=&r->cross_links[i];
  if(c->parent<0||c->parent>=r->vertex_count||c->drawable<0||
     c->drawable==r->drawable||c->drawable>4095||c->child<0||
     !isfinite(c->stiffness)||c->stiffness<0||c->stiffness>1)return 0;
  for(int j=0;j<i;j++)
   if(r->cross_links[j].drawable!=c->drawable||
      r->cross_links[j].child==c->child)return 0;
  /* Target child upper bound requires model snapshot at Native integration. */
 }
 return 1;
}
static G5Code identity(const G5Txn *t,uint64_t session,uint64_t generation){
 if(!t||!session||session!=t->session_epoch)return G5_STALE_SESSION;
 if(!generation||generation!=t->model_generation)return G5_STALE_MODEL;
 return G5_OK;
}
static G5Code fence(const G5Txn *t,uint64_t session,uint64_t generation,uint64_t expected){
 G5Code c=identity(t,session,generation);
 if(c!=G5_OK)return c;
 return expected==t->revision?G5_OK:G5_REVISION_CONFLICT;
}
static G5Code claim(const G5Txn *t,uint64_t owner,uint64_t draft){
 if(!t||!t->has_draft||draft==0||draft!=t->draft_id)return G5_INVALID_DRAFT;
 if(!owner||t->lease_owner!=owner)return G5_BUSY;
 return G5_OK;
}
static G5Code receipt_available(const G5Txn *t,uint64_t request_id){
 if(!request_id)return G5_INVALID_DRAFT;
 for(unsigned i=0;i<t->receipt_count;i++)
  if(t->receipts[i].request_id==request_id)return G5_ID_CONFLICT;
 if(t->receipt_count==G5_RECEIPT_CAP||t->revision>=G5_REV_MAX)
  return G5_OUT_OF_BUDGET;
 return G5_OK;
}
static void publish_receipt(G5Txn *t,uint64_t owner,uint64_t request_id){
 G5Receipt receipt={owner,request_id,t->revision,G5_COMMITTED};
 t->receipts[t->receipt_count++]=receipt;
}
G5Code g5_txn_init(G5Txn *t,const StudioRig *rig,uint64_t session,
                    uint64_t generation,uint64_t revision){
 if(!t||!rig||!session||!generation||!revision||revision>=G5_REV_MAX||
    !rig_valid(rig))return G5_INVALID_DRAFT;
 memset(t,0,sizeof *t);
 t->session_epoch=session;t->model_generation=generation;
 t->revision=revision;t->applied_revision=revision;
 t->live=*rig;
 return G5_OK;
}
G5Code g5_txn_begin(G5Txn *t,uint64_t session,uint64_t generation,
                     uint64_t expected,uint64_t owner,uint64_t *draft_id){
 G5Code c=fence(t,session,generation,expected);if(c!=G5_OK)return c;
 if(t->has_draft)return G5_BUSY;
 if(!owner||!draft_id||t->draft_serial==G5_REV_MAX)return G5_OUT_OF_BUDGET;
 t->draft=t->live;t->base_revision=t->revision;t->lease_owner=owner;
 t->draft_id=++t->draft_serial;t->has_draft=1;t->validated=0;
 *draft_id=t->draft_id;
 return G5_OK;
}
G5Code g5_txn_anchor(G5Txn *t,uint64_t session,uint64_t generation,
                     uint64_t owner,uint64_t draft,int vertex,int enabled){
 G5Code c=identity(t,session,generation);if(c!=G5_OK)return c;
 c=claim(t,owner,draft);if(c!=G5_OK)return c;
 if(vertex<0||vertex>=t->draft.vertex_count||(enabled!=0&&enabled!=1))
  return G5_INVALID_DRAFT;
 int found=0;
 for(int i=0;i<t->draft.anchor_count;i++)if(t->draft.anchors[i]==vertex)found=1;
 if(found!=(enabled!=0)){
  if(!studio_rig_toggle_anchor(&t->draft,vertex))return G5_OUT_OF_BUDGET;
 }
 t->validated=0;
 return G5_OK;
}
static G5Code stage_owner(G5Txn *t,uint64_t session,uint64_t generation,
                          uint64_t owner,uint64_t draft){
 G5Code c=identity(t,session,generation);if(c!=G5_OK)return c;
 c=claim(t,owner,draft);if(c!=G5_OK)return c;
 return t->base_revision==t->revision?G5_OK:G5_REVISION_CONFLICT;
}
G5Code g5_txn_beam(G5Txn *t,uint64_t session,uint64_t generation,
                   uint64_t owner,uint64_t draft,int a,int b,int enabled){
 G5Code c=stage_owner(t,session,generation,owner,draft);if(c!=G5_OK)return c;
 if(enabled!=0&&enabled!=1)return G5_INVALID_DRAFT;
 StudioRig next=t->draft;
 int exists=0,lo=a<b?a:b,hi=a<b?b:a;
 for(int i=0;i<next.beam_count;i++)
  if(next.beams[i].a==lo&&next.beams[i].b==hi)exists=1;
 if(exists!=(enabled!=0)&&!studio_rig_toggle_beam(&next,a,b))
  return G5_INVALID_DRAFT;
 t->draft=next;t->validated=0;return G5_OK;
}
G5Code g5_txn_parent(G5Txn *t,uint64_t session,uint64_t generation,
                     uint64_t owner,uint64_t draft,int parent,int child,
                     float stiffness,int enabled){
 G5Code c=stage_owner(t,session,generation,owner,draft);if(c!=G5_OK)return c;
 StudioRig next=t->draft;
 if(!studio_rig_parent(&next,parent,child,stiffness,enabled))
  return G5_INVALID_DRAFT;
 t->draft=next;t->validated=0;return G5_OK;
}
G5Code g5_txn_weight(G5Txn *t,uint64_t session,uint64_t generation,
                     uint64_t owner,uint64_t draft,int vertex,float value){
 G5Code c=stage_owner(t,session,generation,owner,draft);if(c!=G5_OK)return c;
 StudioRig next=t->draft;
 if(!studio_rig_weight(&next,vertex,value))return G5_INVALID_DRAFT;
 t->draft=next;t->validated=0;return G5_OK;
}
G5Code g5_txn_cross(G5Txn *t,uint64_t session,uint64_t generation,
                    uint64_t owner,uint64_t draft,int parent,int target,
                    int child,int target_vertices,float stiffness,int enabled){
 G5Code c=stage_owner(t,session,generation,owner,draft);if(c!=G5_OK)return c;
 StudioRig next=t->draft;
 /* Target vertex count MUST originate from a generation-fenced Native model. */
 if(!studio_rig_cross(&next,parent,target,child,target_vertices,stiffness,enabled))
  return G5_INVALID_DRAFT;
 t->draft=next;t->validated=0;return G5_OK;
}
G5Code g5_txn_validate(G5Txn *t,uint64_t session,uint64_t generation,
                        uint64_t owner,uint64_t draft){
 G5Code c=identity(t,session,generation);if(c!=G5_OK)return c;
 c=claim(t,owner,draft);if(c!=G5_OK)return c;
 if(!rig_valid(&t->draft)||t->draft.model_signature!=t->live.model_signature||
    t->draft.drawable!=t->live.drawable||
    t->draft.vertex_count!=t->live.vertex_count)return G5_INVALID_DRAFT;
 t->validated=1;
 return G5_OK;
}
G5Code g5_txn_commit(G5Txn *t,uint64_t session,uint64_t generation,
                     uint64_t owner,uint64_t draft,uint64_t expected,
                     uint64_t request_id,uint64_t *revision){
 G5Code c=identity(t,session,generation);if(c!=G5_OK)return c;
 c=receipt_available(t,request_id);if(c!=G5_OK)return c;
 c=claim(t,owner,draft);if(c!=G5_OK)return c;
 if(expected!=t->revision||expected!=t->base_revision)return G5_REVISION_CONFLICT;
 if(!revision||!t->validated||!rig_valid(&t->draft))return G5_INVALID_DRAFT;
 if(t->undo_count==G5_HISTORY_CAP)return G5_OUT_OF_BUDGET;
 /* One owner-thread linearization: all capacity checks precede effect. */
 t->undo[t->undo_count++]=t->live;t->redo_count=0;
 t->live=t->draft;t->revision++;
 publish_receipt(t,owner,request_id);
 t->has_draft=0;t->validated=0;t->lease_owner=0;t->draft_id=0;
 *revision=t->revision;
 return G5_OK;
}
G5Code g5_txn_abort(G5Txn *t,uint64_t session,uint64_t generation,
                    uint64_t owner,uint64_t draft){
 G5Code c=identity(t,session,generation);if(c!=G5_OK)return c;
 c=claim(t,owner,draft);if(c!=G5_OK)return c;
 t->has_draft=0;t->validated=0;t->lease_owner=0;t->draft_id=0;
 return G5_OK;
}
static G5Code navigate(G5Txn *t,uint64_t session,uint64_t generation,
                       uint64_t expected,uint64_t owner,uint64_t request_id,
                       uint64_t *revision,int undo){
 G5Code c=fence(t,session,generation,expected);if(c!=G5_OK)return c;
 if(t->has_draft)return G5_BUSY;
 if(!owner||!revision)return G5_INVALID_DRAFT;
 c=receipt_available(t,request_id);if(c!=G5_OK)return c;
 if(undo){
  if(!t->undo_count)return G5_INVALID_DRAFT;
  if(t->redo_count==G5_HISTORY_CAP)return G5_OUT_OF_BUDGET;
  t->redo[t->redo_count++]=t->live;
  t->live=t->undo[--t->undo_count];
 }else{
  if(!t->redo_count)return G5_REVISION_CONFLICT;
  if(t->undo_count==G5_HISTORY_CAP)return G5_OUT_OF_BUDGET;
  t->undo[t->undo_count++]=t->live;
  t->live=t->redo[--t->redo_count];
 }
 t->revision++;publish_receipt(t,owner,request_id);
 *revision=t->revision;return G5_OK;
}
G5Code g5_txn_undo(G5Txn *t,uint64_t session,uint64_t generation,
                    uint64_t expected,uint64_t owner,uint64_t request_id,
                    uint64_t *revision){
 return navigate(t,session,generation,expected,owner,request_id,revision,1);
}
G5Code g5_txn_redo(G5Txn *t,uint64_t session,uint64_t generation,
                    uint64_t expected,uint64_t owner,uint64_t request_id,
                    uint64_t *revision){
 return navigate(t,session,generation,expected,owner,request_id,revision,0);
}
G5Code g5_txn_receipt(const G5Txn *t,uint64_t session,uint64_t owner,
                       uint64_t request_id,G5Receipt *receipt){
 if(!t||!session||session!=t->session_epoch||!owner||!receipt)
  return G5_STALE_SESSION;
 for(unsigned i=0;i<t->receipt_count;i++)
  if(t->receipts[i].request_id==request_id){
   if(t->receipts[i].owner!=owner)return G5_STALE_SESSION;
   *receipt=t->receipts[i];return G5_OK;
  }
 return G5_UNKNOWN; /* Absence never authorizes replay. */
}
G5Code g5_txn_apply(G5Txn *t,uint64_t doc,uint64_t solver,uint64_t frame,int accepted){
 if(!t||!accepted||doc!=t->revision||!solver||!frame||
    frame<=t->accepted_frame||solver<=t->solver_revision)return G5_INVALID_DRAFT;
 /* Caller must be trusted FrameCoordinator with genuine accepted frame proof.
    This core is NOT wired to GPU/Studio; do not expose via V2 without it. */
 t->applied_revision=doc;t->solver_revision=solver;t->accepted_frame=frame;
 return G5_OK;
}
