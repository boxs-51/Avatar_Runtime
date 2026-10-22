#include "studio_rig.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static int valid(const StudioRig *r,int i){return r&&i>=0&&i<r->vertex_count;}
int studio_rig_init(StudioRig *r,int d,uint64_t sig,int v){
 if(!r||d<0||!sig||v<1||v>2048)return 0;
 memset(r,0,sizeof(*r));r->drawable=d;r->model_signature=sig;r->vertex_count=v;return 1;
}
int studio_rig_set_role(StudioRig *r,int role){
 if(!r||r->vertex_count<=0||role<0||role>3)return 0;
 r->role=role;return 1;
}
int studio_rig_toggle_anchor(StudioRig *r,int v){
 if(!valid(r,v))return 0;
 for(int i=0;i<r->anchor_count;++i)if(r->anchors[i]==v){
  for(int k=i+1;k<r->anchor_count;++k)r->anchors[k-1]=r->anchors[k];
  --r->anchor_count;return 1;
 }
 if(r->anchor_count>=STUDIO_RIG_MAX_ANCHORS)return 0;
 r->anchors[r->anchor_count++]=v;return 1;
}
int studio_rig_toggle_beam(StudioRig *r,int a,int b){
 if(!valid(r,a)||!valid(r,b)||a==b)return 0;
 if(a>b){int t=a;a=b;b=t;}
 for(int i=0;i<r->beam_count;++i)if(r->beams[i].a==a&&r->beams[i].b==b){
  for(int k=i+1;k<r->beam_count;++k)r->beams[k-1]=r->beams[k];
  --r->beam_count;return 1;
 }
 if(r->beam_count>=STUDIO_RIG_MAX_BEAMS)return 0;
 r->beams[r->beam_count++]=(StudioBeam){a,b};return 1;
}
int studio_rig_parent(StudioRig *r,int parent,int child,float stiffness,int enabled){
 if(!valid(r,parent)||!valid(r,child)||parent==child||
    !isfinite(stiffness)||stiffness<0||stiffness>1||
    (enabled!=0&&enabled!=1))return 0;
 int existing=-1;
 for(int i=0;i<r->parent_count;++i)
   if(r->parents[i].child==child){existing=i;break;}
 if(!enabled){
  if(existing>=0&&r->parents[existing].parent==parent){
   for(int i=existing+1;i<r->parent_count;++i)
     r->parents[i-1]=r->parents[i];
   --r->parent_count;
  }
  return 1;
 }
 if(existing>=0){
  if(r->parents[existing].parent!=parent)return 0;
  r->parents[existing].stiffness=stiffness;
  return 1;
 }
 if(r->parent_count>=STUDIO_RIG_MAX_PARENTS)return 0;
 int walk=parent;
 for(int i=0;i<=r->parent_count;++i){
  if(walk==child)return 0;  /* cycle */
  int up=-1;
  for(int j=0;j<r->parent_count;++j)
   if(r->parents[j].child==walk){up=r->parents[j].parent;break;}
  if(up<0)break;
  walk=up;
  if(i==r->parent_count)return 0;
 }
 r->parents[r->parent_count++]=(StudioRigParent){parent,child,stiffness};
 return 1;
}
float studio_rig_weight_get(const StudioRig *r,int vertex){
 if(!valid(r,vertex))return -1;
 for(int i=0;i<r->weight_count;++i)
  if(r->weights[i].vertex==vertex)return r->weights[i].weight;
 return 1.f;
}
int studio_rig_weight(StudioRig *r,int vertex,float weight){
 if(!valid(r,vertex)||!isfinite(weight)||weight<0||weight>1)return 0;
 for(int i=0;i<r->weight_count;++i)
  if(r->weights[i].vertex==vertex){
   if(weight==1.f){
    for(int j=i+1;j<r->weight_count;++j)r->weights[j-1]=r->weights[j];
    --r->weight_count;
   }else r->weights[i].weight=weight;
   return 1;
  }
 if(weight==1.f)return 1;
 if(r->weight_count>=STUDIO_RIG_MAX_WEIGHTS)return 0;
 r->weights[r->weight_count++]=(StudioRigWeight){vertex,weight};
 return 1;
}
int studio_rig_save(const StudioRig *r,const char *path){
 if(!r||!path||!r->model_signature||
    r->parent_count<0||r->parent_count>STUDIO_RIG_MAX_PARENTS||
    r->weight_count<0||r->weight_count>STUDIO_RIG_MAX_WEIGHTS)return 0;
 FILE *f=fopen(path,"w");if(!f)return 0;
 int ok=fprintf(f,"STUDIO_RIG_3 %llu %d %d %d %d %d %d %d\n",
    (unsigned long long)r->model_signature,r->drawable,r->vertex_count,
    r->anchor_count,r->beam_count,r->role,
    r->parent_count,r->weight_count)>0;
 for(int i=0;i<r->anchor_count;++i)
  ok &= fprintf(f,"A %d\n",r->anchors[i])>0;
 for(int i=0;i<r->beam_count;++i)
  ok &= fprintf(f,"B %d %d\n",r->beams[i].a,r->beams[i].b)>0;
 for(int i=0;i<r->parent_count;++i)
  ok &= fprintf(f,"P %d %d %.9g\n",r->parents[i].parent,
    r->parents[i].child,r->parents[i].stiffness)>0;
 for(int i=0;i<r->weight_count;++i)
  ok &= fprintf(f,"W %d %.9g\n",r->weights[i].vertex,r->weights[i].weight)>0;
 if(ferror(f))ok=0;
 if(fclose(f))ok=0;
 return ok;
}
int studio_rig_load(StudioRig *r,const char *path,uint64_t sig,int vertices){
 if(!r||!path)return 0;
 FILE *f=fopen(path,"r");if(!f)return 0;
 char magic[32]={0};unsigned long long got=0;
 int d=0,v=0,na=0,nb=0,role=0,np=0,nw=0;
 int ok=fscanf(f,"%31s %llu %d %d %d %d",magic,&got,&d,&v,&na,&nb)==6;
 int format=0;
 if(ok){
  if(!strcmp(magic,"STUDIO_RIG_1"))format=1;
  if(!strcmp(magic,"STUDIO_RIG_2"))format=2;
  if(!strcmp(magic,"STUDIO_RIG_3"))format=3;
  ok=format>0;
 }
 if(ok&&format>=2)ok=fscanf(f,"%d",&role)==1;
 if(ok&&format==3)ok=fscanf(f,"%d %d",&np,&nw)==2;
 ok=ok&&got==sig&&v==vertices&&role>=0&&role<=3&&
    na>=0&&na<=STUDIO_RIG_MAX_ANCHORS&&
    nb>=0&&nb<=STUDIO_RIG_MAX_BEAMS&&
    np>=0&&np<=STUDIO_RIG_MAX_PARENTS&&
    nw>=0&&nw<=STUDIO_RIG_MAX_WEIGHTS;
 StudioRig temp={0};
 if(ok){ok=studio_rig_init(&temp,d,sig,v);temp.role=role;}
 for(int i=0;i<na&&ok;++i){
  char type;int x;
  ok=fscanf(f," %c %d",&type,&x)==2&&type=='A'&&valid(&temp,x);
  if(ok){
   for(int j=0;j<temp.anchor_count;++j)if(temp.anchors[j]==x)ok=0;
   if(ok)ok=studio_rig_toggle_anchor(&temp,x);
  }
 }
 for(int i=0;i<nb&&ok;++i){
  char type;int a,b;
  ok=fscanf(f," %c %d %d",&type,&a,&b)==3&&type=='B'&&valid(&temp,a)&&valid(&temp,b);
  if(ok){
   for(int j=0;j<temp.beam_count;++j)
    if(temp.beams[j].a==(a<b?a:b)&&temp.beams[j].b==(a<b?b:a))ok=0;
   if(ok)ok=studio_rig_toggle_beam(&temp,a,b);
  }
 }
 for(int i=0;i<np&&ok;++i){
  char type;int parent,child;float stiffness;
  ok=fscanf(f," %c %d %d %f",&type,&parent,&child,&stiffness)==4&&
     type=='P'&&studio_rig_parent(&temp,parent,child,stiffness,1);
 }
 for(int i=0;i<nw&&ok;++i){
  char type;int vertex;float weight;
  ok=fscanf(f," %c %d %f",&type,&vertex,&weight)==3&&type=='W'&&
     studio_rig_weight(&temp,vertex,weight);
 }
 if(ok){char extra;ok=fscanf(f," %c",&extra)==EOF;}
 fclose(f);
 if(ok)*r=temp;
 return ok;
}
int studio_rig_bank_select(StudioRigBank *bank,StudioRig *current,
                           int drawable,uint64_t signature,int vertices){
 if(!bank||!current||drawable<0||signature==0||vertices<1||vertices>2048)return 0;
 if(current->vertex_count==vertices&&current->drawable==drawable&&
    current->model_signature==signature)return 1;
 StudioRigBank temp=*bank;
 if(temp.signature!=signature){
  memset(&temp,0,sizeof temp);
  temp.signature=signature;
 }
 if(current->vertex_count>0&&current->model_signature==signature){
  int slot=-1;
  for(int i=0;i<temp.count;++i)
   if(temp.rigs[i].drawable==current->drawable){slot=i;break;}
  int edited=current->anchor_count||current->beam_count||current->parent_count||
       current->weight_count||current->role;
  if(edited){
   if(slot<0){
    if(temp.count>=STUDIO_RIG_BANK_CAP)return 0;
    slot=temp.count++;
   }
   temp.rigs[slot]=*current;
  }else if(slot>=0){
   /* Discard stale saved edits when a drawable was cleared to defaults. */
   for(int i=slot+1;i<temp.count;++i)temp.rigs[i-1]=temp.rigs[i];
   --temp.count;
  }
 }
 StudioRig next={0};
 int cached=0;
 for(int i=0;i<temp.count;++i)
  if(temp.rigs[i].drawable==drawable){
   if(temp.rigs[i].vertex_count!=vertices)return 0;
   next=temp.rigs[i];
   cached=1;break;
  }
 if(!cached&&!studio_rig_init(&next,drawable,signature,vertices))return 0;
 *bank=temp;
 *current=next;
 return 1;
}
