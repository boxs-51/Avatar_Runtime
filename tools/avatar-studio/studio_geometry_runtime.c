#include "studio_geometry_runtime.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
/* Little-endian private Studio compiled mesh, not a Live2D MOC3 serializer. */
static uint64_t digest;
static void digest_begin(void){digest=UINT64_C(1469598103934665603);}
static void checksum(unsigned char byte){
 digest^=byte;digest*=UINT64_C(1099511628211);
}
static int put(FILE*f,uint64_t value,int size){
 for(int i=0;i<size;i++){
  unsigned char c=(unsigned char)(value>>(i*8));
  if(fputc(c,f)==EOF)return 0;checksum(c);
 }
 return 1;
}
static int take(FILE*f,uint64_t*out,int size){
 uint64_t value=0;
 for(int i=0;i<size;i++){
  int ch=fgetc(f);if(ch==EOF)return 0;
  checksum((unsigned char)ch);
  value|=((uint64_t)(unsigned char)ch)<<(i*8);
 }
 *out=value;return 1;
}
static int put_float(FILE*f,float n){
 uint32_t bits=0;memcpy(&bits,&n,sizeof bits);
 return put(f,bits,4);
}
static int take_float(FILE*f,float*out){
 uint64_t bits=0;if(!take(f,&bits,4))return 0;
 uint32_t x=(uint32_t)bits;memcpy(out,&x,sizeof x);return 1;
}
static int put_int(FILE*f,int n){return put(f,(uint32_t)(int32_t)n,4);}
static int take_int(FILE*f,int*out){
 uint64_t v=0;if(!take(f,&v,4))return 0;
 int32_t x=(int32_t)(uint32_t)v;*out=x;return 1;
}
int studio_geometry_runtime_compile(const StudioGeometryDocument*d,uint64_t sig,FILE*f){
 if(!f||!studio_geometry_verify(d,sig)||sizeof(float)!=4)return 0;
 digest_begin();
 const unsigned char magic[8]={'A','R','S','M','E','S','H','3'};
 for(int i=0;i<8;i++)if(!put(f,magic[i],1))return 0;
 if(!put(f,sig,8)||!put(f,d->revision,8)||
    !put(f,(uint32_t)d->active.count,4)||
     !put(f,(uint32_t)d->active.link_count,4))return 0;
 for(int i=0;i<d->active.count;i++){
  const StudioGeometryLayer*l=&d->active.layers[i];
  if(!put_int(f,l->id)||!put_int(f,l->vertex_count)||
     !put_int(f,l->tri_count)||!put_int(f,l->pin_vertex)||
     !put(f,l->next_vertex_id,4))return 0;
  for(int c=0;c<4;c++)if(!put_float(f,l->tint[c]))return 0;
  for(int v=0;v<l->vertex_count;v++)
   if(!put_float(f,l->vertices[v].x)||!put_float(f,l->vertices[v].y)||
      !put_float(f,l->uv[v].x)||!put_float(f,l->uv[v].y)||
      !put_float(f,l->weights[v])||!put_int(f,l->parent[v])||
      !put_float(f,l->parent_stiffness[v])||
      !put(f,l->vertex_ids[v],4))return 0;
  for(int t=0;t<l->tri_count;t++)
   for(int k=0;k<3;k++)if(!put(f,l->triangles[t][k],2))return 0;
 }
 for(int i=0;i<d->active.link_count;i++){
  const StudioGeometryCrossLink*l=&d->active.links[i];
  if(!put_int(f,l->parent_layer_id)||!put(f,l->parent_vertex_id,4)||
     !put_int(f,l->child_layer_id)||!put(f,l->child_vertex_id,4)||
     !put_float(f,l->stiffness))return 0;
 }
 uint64_t fingerprint=digest;
 return put(f,fingerprint,8)&&fflush(f)==0&&!ferror(f);
}

int studio_geometry_runtime_load(StudioGeometryDocument*out,uint64_t sig,FILE*f){
 if(!out||!f||!sig||sizeof(float)!=4)return 0;
 StudioGeometryDocument*doc=calloc(1,sizeof *doc);if(!doc)return 0;
 studio_geometry_init(doc,sig);
 digest_begin();
 const unsigned char magic[8]={'A','R','S','M','E','S','H','1'};
 uint64_t value=0;int ok=0;
 int stable=0,linked=0;
 for(int i=0;i<8;i++){
  if(!take(f,&value,1))goto done;
  if(i==7){if(value!='1'&&value!='2'&&value!='3')goto done;stable=value!='1';linked=value=='3';}
  else if(value!=magic[i])goto done;
 }
 if(!take(f,&value,8)||value!=sig)goto done;
 if(!take(f,&value,8)||value>=INT64_MAX)goto done;
 doc->revision=value;
 if(!take(f,&value,4)||value>STUDIO_GEOM_LAYERS)goto done;
 doc->active.count=(int)value;
 if(linked){
  if(!take(f,&value,4)||value>STUDIO_GEOM_LINKS)goto done;
  doc->active.link_count=(int)value;
 }
 for(int i=0;i<doc->active.count;i++){
  StudioGeometryLayer*l=&doc->active.layers[i];
  if(!take_int(f,&l->id)||!take_int(f,&l->vertex_count)||
     !take_int(f,&l->tri_count)||!take_int(f,&l->pin_vertex)||
     l->vertex_count<0||l->vertex_count>STUDIO_GEOM_VERTICES||
     l->tri_count<0||l->tri_count>STUDIO_GEOM_TRIANGLES)goto done;
  if(stable){
   if(!take(f,&value,4))goto done;
   l->next_vertex_id=(uint32_t)value;
  }else l->next_vertex_id=(uint32_t)(l->vertex_count+1);
  for(int c=0;c<4;c++)if(!take_float(f,&l->tint[c]))goto done;
  for(int j=0;j<l->vertex_count;j++){
   if(!take_float(f,&l->vertices[j].x)||!take_float(f,&l->vertices[j].y)||
      !take_float(f,&l->uv[j].x)||!take_float(f,&l->uv[j].y)||
      !take_float(f,&l->weights[j])||!take_int(f,&l->parent[j])||
      !take_float(f,&l->parent_stiffness[j]))goto done;
   if(stable){
    if(!take(f,&value,4))goto done;
    l->vertex_ids[j]=(uint32_t)value;
   }else l->vertex_ids[j]=(uint32_t)(j+1);
  }
  for(int t=0;t<l->tri_count;t++)
   for(int k=0;k<3;k++){
    if(!take(f,&value,2))goto done;
    l->triangles[t][k]=(unsigned short)value;
   }
 }
 for(int i=0;i<doc->active.link_count;i++){
  StudioGeometryCrossLink*l=&doc->active.links[i];
  if(!take_int(f,&l->parent_layer_id)||!take(f,&value,4))goto done;
  l->parent_vertex_id=(uint32_t)value;
  if(!take_int(f,&l->child_layer_id)||!take(f,&value,4))goto done;
  l->child_vertex_id=(uint32_t)value;
  if(!take_float(f,&l->stiffness))goto done;
 }
 uint64_t expected=digest;
 if(!take(f,&value,8)||value!=expected||fgetc(f)!=EOF||
    !studio_geometry_verify(doc,sig))goto done;
 *out=*doc;ok=1;
done:
 free(doc);return ok;
}
