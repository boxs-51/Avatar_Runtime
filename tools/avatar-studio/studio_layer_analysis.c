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
