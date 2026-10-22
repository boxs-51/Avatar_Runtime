#include "studio_layer_analysis.h"
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
static int word(const char *text,const char *needle){
 size_t n=strlen(needle);
 if(!text||strlen(text)>160)return 0;
 for(size_t i=0;text[i];++i){
  size_t j=0;
  while(j<n&&text[i+j]&&tolower((unsigned char)text[i+j])==needle[j])++j;
  if(j==n){
   if(i>0&&isalnum((unsigned char)text[i-1])&&
      !(isupper((unsigned char)text[i])&&islower((unsigned char)text[i-1])))continue;
   return 1;
  }
 }
 return 0;
}
static int match_any(const char *id,const char *const *terms,size_t n){
 for(size_t i=0;i<n;++i)if(word(id,terms[i]))return 1;
 return 0;
}
const char *studio_layer_kind_name(StudioLayerKind k){
 switch(k){case STUDIO_LAYER_HAIR:return "hair";
 case STUDIO_LAYER_SKIRT:return "skirt";
 case STUDIO_LAYER_STRAP:return "strap";
 default:return "unknown";}
}
int studio_layer_analyze(const StudioLayerInput *in,StudioLayerSuggestion *out){
 if(!in||!out||!in->drawable_id||!in->vertices||
    in->vertex_count<3||in->vertex_count>2048||
    !isfinite(in->object_height)||in->object_height<=0||
    !isfinite(in->object_min_y)||!isfinite(in->object_max_y)||
    in->object_min_y>in->object_max_y)return 0;
 StudioLayerSuggestion next={0};
 const char *const hair[]={"hair","bang","fringe","ponytail","ahoge"};
 const char *const skirt[]={"skirt","dress","hem"};
 const char *const strap[]={"ribbon","strap","rope","string","tie","cord"};
 const int h=match_any(in->drawable_id,hair,sizeof(hair)/sizeof(hair[0]));
 const int k=match_any(in->drawable_id,skirt,sizeof(skirt)/sizeof(skirt[0]));
 const int t=match_any(in->drawable_id,strap,sizeof(strap)/sizeof(strap[0]));
 if(h+k+t==1){
  next.kind=h?STUDIO_LAYER_HAIR:(k?STUDIO_LAYER_SKIRT:STUDIO_LAYER_STRAP);
  next.confidence=90;
 }
 float lo=INFINITY,hi=-INFINITY,lx=INFINITY,hx=-INFINITY;
 for(int i=0;i<in->vertex_count;++i){
  float x=in->vertices[i].x,y=in->vertices[i].y;
  if(!isfinite(x)||!isfinite(y))return 0;
  if(x<lx)lx=x; if(x>hx)hx=x;
  if(y<lo)lo=y;
  if(y>hi)hi=y;
 }
 if(lo<in->object_min_y-1e-3f||hi>in->object_max_y+1e-3f)return 0;
 const float height=hi-lo,width=hx-lx;
 const float object_span=in->object_max_y-in->object_min_y;
 if(object_span>0){
  const float center=((hi+lo)*.5f-in->object_min_y)/object_span;
  next.region=height>object_span*.60f?4:(center>.67f?1:(center<.33f?3:2));
 }
 const float maxdim=fmaxf(width,height);
 if(maxdim>0){
  const float aspect=width/fmaxf(height,1e-8f);
  next.shape=fminf(width,height)/maxdim<.19f?1:
    (aspect>1.8f?2:(aspect<.55f?3:0));
 }
 next.has_vertical_span=(hi-lo)>in->object_height*.04f;
 if(next.kind!=STUDIO_LAYER_UNKNOWN&&next.has_vertical_span){
  int needed=next.kind==STUDIO_LAYER_STRAP?1:2;
  for(int pass=0;pass<needed;++pass){
   int best=-1;
   for(int i=0;i<in->vertex_count;++i){
    int used=0;
    for(int j=0;j<next.anchor_count;++j)if(next.anchors[j]==i)used=1;
    if(used)continue;
    if(best<0||in->vertices[i].y>in->vertices[best].y||
       (in->vertices[i].y==in->vertices[best].y&&i<best))best=i;
   }
   if(best>=0)next.anchors[next.anchor_count++]=best;
  }
 }else{next.kind=STUDIO_LAYER_UNKNOWN;next.confidence=0;next.anchor_count=0;}
 *out=next;return 1;
}



const char *studio_layer_region_name(int region){
 switch(region){case 1:return "upper";case 2:return "middle";
 case 3:return "lower";case 4:return "spanning";default:return "unknown";}
}
const char *studio_layer_shape_name(int shape){
 switch(shape){case 1:return "thin";case 2:return "wide";case 3:return "tall";
 default:return "compact";}
}
/* Strict triangle-index connectivity analysis; no private model data persisted. */
static int studio_root(int *parent,int i){
 while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}
 return i;
}
static void studio_unite(int *parent,int *weight,int a,int b){
 a=studio_root(parent,a);b=studio_root(parent,b);
 if(a==b)return;
 if(weight[a]<weight[b]){int t=a;a=b;b=t;}
 parent[b]=a;weight[a]+=weight[b];
}
int studio_layer_components(const unsigned short *indices,int index_count,
                             int vertex_count,StudioLayerTopology *out){
 if(!indices||!out||vertex_count<3||vertex_count>2048||
     index_count<3||index_count>65535||index_count%3)return 0;
 int parent[2048],weight[2048];
 unsigned char used[2048]={0},seen[2048]={0};
 for(int i=0;i<vertex_count;++i){parent[i]=i;weight[i]=1;}
 for(int t=0;t<index_count;t+=3){
  int a=indices[t],b=indices[t+1],c=indices[t+2];
  if(a>=vertex_count||b>=vertex_count||c>=vertex_count||
     a==b||b==c||a==c)return 0;
  used[a]=used[b]=used[c]=1;
  studio_unite(parent,weight,a,b);
  studio_unite(parent,weight,b,c);
 }
 StudioLayerTopology next={0};
 next.triangles=index_count/3;
 for(int i=0;i<vertex_count;++i){
  if(!used[i]){next.isolated++;continue;}
  next.used++;
  int root=studio_root(parent,i);
  if(!seen[root]){seen[root]=1;next.islands++;}
 }
 for(int i=0;i<vertex_count;++i)
  if(seen[i]&&weight[i]>next.largest)next.largest=weight[i];
 *out=next;return 1;
}

/* Deterministically enumerate islands by smallest referenced vertex.
   Candidate root is the topmost vertex in that island; never auto-bind. */
int studio_layer_component_suggest(const unsigned short *indices,int index_count,
     const StudioLayerVertex *vertices,int vertex_count,int component,
     StudioLayerComponent *out){
 StudioLayerTopology verified={0};
 if(!vertices||!out||component<0||
    !studio_layer_components(indices,index_count,vertex_count,&verified)||
    component>=verified.islands)return 0;
 int parent[2048],weight[2048];
 unsigned char used[2048]={0},seen[2048]={0};
 for(int i=0;i<vertex_count;++i){
  if(!isfinite(vertices[i].x)||!isfinite(vertices[i].y))return 0;
  parent[i]=i;weight[i]=1;
 }
 for(int i=0;i<index_count;i+=3){
  const int a=indices[i],b=indices[i+1],c=indices[i+2];
  used[a]=used[b]=used[c]=1;
  studio_unite(parent,weight,a,b);
  studio_unite(parent,weight,b,c);
 }
 int selected_root=-1,first_vertex=-1,ordinal=0;
 for(int i=0;i<vertex_count;++i){
  if(!used[i])continue;
  int root=studio_root(parent,i);
  if(!seen[root]){
   seen[root]=1;
   if(ordinal==component){selected_root=root;first_vertex=i;break;}
   ++ordinal;
  }
 }
 if(selected_root<0)return 0;
 StudioLayerComponent next={first_vertex,0,-1};
 for(int i=0;i<vertex_count;++i){
  if(!used[i]||studio_root(parent,i)!=selected_root)continue;
  ++next.vertices;
  if(next.anchor<0||vertices[i].y>vertices[next.anchor].y||
     (vertices[i].y==vertices[next.anchor].y&&i<next.anchor))next.anchor=i;
 }
 if(next.vertices<3||next.anchor<0)return 0;
 *out=next;return 1;
}
