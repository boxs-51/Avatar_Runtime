#include "studio_rig.h"
#include <stdio.h>
#include <string.h>
static int valid(const StudioRig *r,int i){return r&&i>=0&&i<r->vertex_count;}
int studio_rig_init(StudioRig *r,int d,uint64_t sig,int v){
 if(!r||d<0||!sig||v<1||v>2048)return 0;
 memset(r,0,sizeof(*r));r->drawable=d;r->model_signature=sig;r->vertex_count=v;return 1;
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
int studio_rig_save(const StudioRig *r,const char *path){
 if(!r||!path||!r->model_signature)return 0;
 FILE *f=fopen(path,"w");if(!f)return 0;
 int ok=fprintf(f,"STUDIO_RIG_1 %llu %d %d %d %d\n",(unsigned long long)r->model_signature,r->drawable,r->vertex_count,r->anchor_count,r->beam_count)>0;
 for(int i=0;i<r->anchor_count;++i)ok&=fprintf(f,"A %d\n",r->anchors[i])>0;
 for(int i=0;i<r->beam_count;++i)ok&=fprintf(f,"B %d %d\n",r->beams[i].a,r->beams[i].b)>0;
 return fclose(f)==0&&ok;
}
int studio_rig_load(StudioRig *r,const char *path,uint64_t sig,int vertices){
 if(!r||!path)return 0;FILE *f=fopen(path,"r");if(!f)return 0;
 char magic[32];unsigned long long got=0;int d,v,na,nb;
 int ok=fscanf(f,"%31s %llu %d %d %d %d",magic,&got,&d,&v,&na,&nb)==6&&
 !strcmp(magic,"STUDIO_RIG_1")&&got==sig&&v==vertices&&na>=0&&na<=STUDIO_RIG_MAX_ANCHORS&&nb>=0&&nb<=STUDIO_RIG_MAX_BEAMS;
 StudioRig temp={0};if(ok)ok=studio_rig_init(&temp,d,sig,v);
 for(int i=0;i<na&&ok;++i){char type;int x;ok=fscanf(f," %c %d",&type,&x)==2&&type=='A'&&studio_rig_toggle_anchor(&temp,x);}
 for(int i=0;i<nb&&ok;++i){char type;int a,b;ok=fscanf(f," %c %d %d",&type,&a,&b)==3&&type=='B'&&studio_rig_toggle_beam(&temp,a,b);}
 if(ok){char extra;ok=fscanf(f," %c",&extra)==EOF;}
 fclose(f);if(ok)*r=temp;return ok;
}
