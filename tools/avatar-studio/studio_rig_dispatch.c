#include "studio_rig_dispatch.h"
#include <stdint.h>
/* Compare only active, meaningful Rig fields. Inactive array tails and
   structure padding are not edits and must not advance the CAS revision. */
static int valid_shape(const StudioRig *r){
 return r&&r->vertex_count>0&&r->vertex_count<=2048&&r->model_signature&&
   r->drawable>=0&&r->role>=0&&r->role<=3&&
   r->anchor_count>=0&&r->anchor_count<=STUDIO_RIG_MAX_ANCHORS&&
   r->anchor_count<=r->vertex_count&&
   r->beam_count>=0&&r->beam_count<=STUDIO_RIG_MAX_BEAMS&&
   r->parent_count>=0&&r->parent_count<=STUDIO_RIG_MAX_PARENTS&&
   r->weight_count>=0&&r->weight_count<=STUDIO_RIG_MAX_WEIGHTS&&
   r->cross_count>=0&&r->cross_count<=STUDIO_RIG_MAX_CROSS;
}
static int same_rig(const StudioRig *a,const StudioRig *b){
 if(a->model_signature!=b->model_signature||a->drawable!=b->drawable||
    a->vertex_count!=b->vertex_count||a->role!=b->role||
    a->anchor_count!=b->anchor_count||a->beam_count!=b->beam_count||
    a->parent_count!=b->parent_count||a->weight_count!=b->weight_count||
    a->cross_count!=b->cross_count)return 0;
 for(int i=0;i<a->anchor_count;i++)
  if(a->anchors[i]!=b->anchors[i])return 0;
 for(int i=0;i<a->beam_count;i++)
  if(a->beams[i].a!=b->beams[i].a||
     a->beams[i].b!=b->beams[i].b)return 0;
 for(int i=0;i<a->parent_count;i++)
  if(a->parents[i].parent!=b->parents[i].parent||
     a->parents[i].child!=b->parents[i].child||
     a->parents[i].stiffness!=b->parents[i].stiffness)return 0;
 for(int i=0;i<a->weight_count;i++)
  if(a->weights[i].vertex!=b->weights[i].vertex||
     a->weights[i].weight!=b->weights[i].weight)return 0;
 for(int i=0;i<a->cross_count;i++)
  if(a->cross_links[i].parent!=b->cross_links[i].parent||
     a->cross_links[i].drawable!=b->cross_links[i].drawable||
     a->cross_links[i].child!=b->cross_links[i].child||
     a->cross_links[i].stiffness!=b->cross_links[i].stiffness)return 0;
 return 1;
}
StudioRigDispatchCode studio_rig_dispatch(
 StudioRig *live,uint64_t *revision,uint64_t generation,
 uint64_t expected_generation,uint64_t expected_revision,
 StudioRigCommand command,void *argument){
 if(!live||!revision||!command)return STUDIO_RIG_DISPATCH_REJECT;
 if(!generation||generation!=expected_generation||
    *revision!=expected_revision)return STUDIO_RIG_DISPATCH_STALE;
 StudioRig candidate=*live; /* no partial live writes from an invalid command */
 if(!command(&candidate,argument))return STUDIO_RIG_DISPATCH_REJECT;
 /* A logical edit must never change the bound model identity. A malformed
    count must not reach active-array comparison or be committed. */
 if(!valid_shape(&candidate)||candidate.model_signature!=live->model_signature||
    candidate.drawable!=live->drawable||
    candidate.vertex_count!=live->vertex_count)
  return STUDIO_RIG_DISPATCH_REJECT;
 if(same_rig(live,&candidate))
  return STUDIO_RIG_DISPATCH_UNCHANGED;
 if(*revision>=UINT64_C(0x7fffffffffffffff))
  return STUDIO_RIG_DISPATCH_REJECT;
 *live=candidate;
 ++*revision; /* exactly one accepted logical edit */
 return STUDIO_RIG_DISPATCH_OK;
}

StudioRigDispatchCode studio_rig_dispatch_select(
 StudioRig *live,StudioRigBank *bank,uint64_t *revision,
 uint64_t generation,uint64_t expected_generation,uint64_t expected_revision,
 int drawable,uint64_t signature,int vertices){
 if(!live||!bank||!revision||!generation||
    generation!=expected_generation||*revision!=expected_revision)
  return STUDIO_RIG_DISPATCH_STALE;
 if(!signature||drawable<0||drawable>4095||vertices<1||vertices>2048||
    (live->vertex_count>0&&live->model_signature!=signature))
  return STUDIO_RIG_DISPATCH_REJECT;
 StudioRig next=*live;
 StudioRigBank next_bank=*bank;
 if(!studio_rig_bank_select(&next_bank,&next,drawable,signature,vertices))
  return STUDIO_RIG_DISPATCH_REJECT;
 if(next.model_signature!=signature||next.drawable!=drawable||
    next.vertex_count!=vertices)
  return STUDIO_RIG_DISPATCH_REJECT;
 if(live->vertex_count>0&&same_rig(live,&next)){
  *bank=next_bank;
  return STUDIO_RIG_DISPATCH_UNCHANGED;
 }
 if(*revision>=UINT64_C(0x7fffffffffffffff))
  return STUDIO_RIG_DISPATCH_REJECT;
 *bank=next_bank;
 *live=next;
 ++*revision;
 return STUDIO_RIG_DISPATCH_OK;
}

int studio_rig_selectable_next(const int *counts,int count,
                               int current,int direction){
 if(!counts||count<1||count>4096||current<0||current>=count||
    (direction!=1&&direction!=-1))return -1;
 for(int offset=1;offset<=count;offset++){
  int index=(current+count+direction*offset)%count;
  if(counts[index]>=1&&counts[index]<=2048)return index;
 }
 return -1;
}
