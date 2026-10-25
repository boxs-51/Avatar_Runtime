#include "studio_rig_history.h"
#include <string.h>

static int same_document(const StudioRigHistory *h,const StudioRig *r){
 return h&&r&&r->model_signature&&
   r->model_signature==h->signature&&r->drawable==h->drawable&&
   r->vertex_count==h->vertices;
}
static void push(StudioRig *entries,unsigned *count,const StudioRig *value){
 if(*count==STUDIO_RIG_HISTORY_CAP){
  memmove(entries,entries+1,(STUDIO_RIG_HISTORY_CAP-1)*sizeof *entries);
  --*count;
 }
 entries[(*count)++]=*value;
}
void studio_rig_history_reset(StudioRigHistory *h,uint64_t generation,
                              uint64_t revision,const StudioRig *r){
 if(!h)return;
 memset(h,0,sizeof *h);
 if(generation&&r&&r->model_signature&&r->vertex_count>0){
  h->generation=generation;h->revision=revision;
  h->signature=r->model_signature;h->drawable=r->drawable;
  h->vertices=r->vertex_count;
 }
}
int studio_rig_history_record(StudioRigHistory *h,uint64_t generation,
     uint64_t before_revision,uint64_t after_revision,
     const StudioRig *before,const StudioRig *after){
 if(!h||!generation||generation!=h->generation||
    before_revision!=h->revision||before_revision>=UINT64_C(0x7fffffffffffffff)||
    after_revision!=before_revision+1||
    !same_document(h,before)||!same_document(h,after))return 0;
 push(h->undo,&h->undo_count,before);
 h->redo_count=0;
 h->revision=after_revision;
 return 1;
}
int studio_rig_history_step(StudioRigHistory *h,StudioRig *live,
 uint64_t *revision,uint64_t generation,uint64_t expected_generation,
 uint64_t expected_revision,int redo){
 if(!h||!live||!revision||!generation||generation!=expected_generation||
    generation!=h->generation||*revision!=expected_revision||
    *revision!=h->revision||*revision>=UINT64_C(0x7fffffffffffffff)||
    (redo!=0&&redo!=1)||!same_document(h,live))return 0;
 StudioRig *stack=redo?h->redo:h->undo;
 unsigned *count=redo?&h->redo_count:&h->undo_count;
 if(!*count||!same_document(h,&stack[*count-1]))return 0;
 StudioRig next=stack[*count-1];
 push(redo?h->undo:h->redo,redo?&h->undo_count:&h->redo_count,live);
 --*count;
 *live=next;
 ++*revision;
 h->revision=*revision;
 return 1;
}
