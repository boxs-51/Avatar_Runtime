#include "studio_geometry_document.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
static int fence(const StudioGeometryDocument*d,uint64_t s,uint64_t r){
 return d&&s&&d->signature==s&&d->revision==r&&r<INT64_MAX;
}
static int pos(float x,float y){
 return isfinite(x)&&isfinite(y)&&fabsf(x)<=10000&&fabsf(y)<=10000;
}
static int tri(const StudioGeometryLayer*l,int a,int b,int c){
 if(!l||a<0||b<0||c<0||a>=l->vertex_count||b>=l->vertex_count||
 c>=l->vertex_count||a==b||b==c||a==c)return 0;
 StudioGeometryPoint p=l->vertices[a],q=l->vertices[b],r=l->vertices[c];
 double area=((double)q.x-p.x)*((double)r.y-p.y)-((double)q.y-p.y)*((double)r.x-p.x);
 return isfinite(area)&&fabs(area)>1e-6;
}
static int layer_at(const StudioGeometrySnapshot*s,int id){
 if(!s||id<=0)return -1;
 for(int i=0;i<s->count;i++)if(s->layers[i].id==id)return i;
 return -1;
}
static int vertex_at(const StudioGeometryLayer*l,uint32_t id){
 if(!l||!id)return -1;
 for(int v=0;v<l->vertex_count;v++)if(l->vertex_ids[v]==id)return v;
 return -1;
}
static int valid(const StudioGeometrySnapshot*s){
 if(!s||s->count<0||s->count>STUDIO_GEOM_LAYERS||
    s->link_count<0||s->link_count>STUDIO_GEOM_LINKS)return 0;
 for(int i=0;i<s->count;i++){
  const StudioGeometryLayer*l=&s->layers[i];
  if(l->id<1||l->id>9999||l->vertex_count<0||l->vertex_count>STUDIO_GEOM_VERTICES||
     l->tri_count<0||l->tri_count>STUDIO_GEOM_TRIANGLES)return 0;
  for(int k=0;k<i;k++)if(l->id==s->layers[k].id)return 0;
  if(l->pin_vertex< -1||l->pin_vertex>=l->vertex_count)return 0;
  for(int c=0;c<4;c++)
   if(!isfinite(l->tint[c])||l->tint[c]<0||l->tint[c]>1)return 0;
  for(int j=0;j<l->vertex_count;j++)
   if(!pos(l->vertices[j].x,l->vertices[j].y)||
      !isfinite(l->weights[j])||l->weights[j]<0||l->weights[j]>1||
      !isfinite(l->parent_stiffness[j])||l->parent_stiffness[j]<0||
      l->parent_stiffness[j]>1||l->parent[j]< -1||
      l->parent[j]>=l->vertex_count||l->parent[j]==j||
      !isfinite(l->uv[j].x)||!isfinite(l->uv[j].y)||
      fabsf(l->uv[j].x)>8||fabsf(l->uv[j].y)>8)return 0;
  if(!l->next_vertex_id)return 0;
  for(int j=0;j<l->vertex_count;j++){
   if(!l->vertex_ids[j]||l->vertex_ids[j]>=l->next_vertex_id)return 0;
   for(int k=0;k<j;k++)
    if(l->vertex_ids[k]==l->vertex_ids[j])return 0;
  }
  for(int j=0;j<l->vertex_count;j++){
   int at=j;
   for(int n=0;n<=l->vertex_count;n++){
    at=l->parent[at];if(at<0)break;
    if(n==l->vertex_count)return 0;
   }
  }
  for(int j=0;j<l->tri_count;j++)
   if(!tri(l,l->triangles[j][0],l->triangles[j][1],l->triangles[j][2]))return 0;
 }
 for(int i=0;i<s->link_count;i++){
  const StudioGeometryCrossLink*link=&s->links[i];
  int parent=layer_at(s,link->parent_layer_id);
  int child=layer_at(s,link->child_layer_id);
  if(parent<0||child<0||parent==child||
     vertex_at(&s->layers[parent],link->parent_vertex_id)<0||
     vertex_at(&s->layers[child],link->child_vertex_id)<0||
     !isfinite(link->stiffness)||link->stiffness<0||
     link->stiffness>1)return 0;
  for(int k=0;k<i;k++){
   const StudioGeometryCrossLink*prev=&s->links[k];
   if(prev->parent_layer_id==link->parent_layer_id&&
      prev->child_layer_id==link->child_layer_id&&
      prev->parent_vertex_id==link->parent_vertex_id&&
      prev->child_vertex_id==link->child_vertex_id)return 0;
  }
 }
 /* Parent-child layer links are directed; no cross-layer cycles. */
 int edges[STUDIO_GEOM_LAYERS][STUDIO_GEOM_LAYERS]={0};
 for(int i=0;i<s->link_count;i++){
  const StudioGeometryCrossLink*l=&s->links[i];
  int a=layer_at(s,l->parent_layer_id),b=layer_at(s,l->child_layer_id);
  if(a<0||b<0)return 0;
  edges[a][b]=1;
 }
 for(int mid=0;mid<s->count;mid++)
  for(int a=0;a<s->count;a++)
   for(int b=0;b<s->count;b++)
    if(edges[a][mid]&&edges[mid][b])edges[a][b]=1;
 for(int i=0;i<s->count;i++)if(edges[i][i])return 0;
 return 1;
}
static void push(StudioGeometrySnapshot*h,int*n,const StudioGeometrySnapshot*v){
 if(*n==STUDIO_GEOM_HISTORY){memmove(h,h+1,(STUDIO_GEOM_HISTORY-1)*sizeof *h);--*n;}
 h[(*n)++]=*v;
}
static int commit(StudioGeometryDocument*d,uint64_t s,uint64_t r,
                  const StudioGeometrySnapshot*next){
 if(!fence(d,s,r)||!valid(next))return 0;
 if(!memcmp(&d->active,next,sizeof *next))return 1;
 push(d->undo,&d->undo_count,&d->active);
 d->redo_count=0;d->active=*next;d->revision++;return 1;
}
int studio_geometry_verify(const StudioGeometryDocument*d,uint64_t s){
 return d&&s&&d->signature==s&&d->revision<INT64_MAX&&valid(&d->active);
}
int studio_geometry_index_by_id(const StudioGeometryDocument*d,int id){
 if(!d||id<1||id>9999||d->active.count<0||
    d->active.count>STUDIO_GEOM_LAYERS)return -1;
 for(int i=0;i<d->active.count;i++)
  if(d->active.layers[i].id==id)return i;
 return -1;
}
int studio_geometry_vertex_index_by_id(const StudioGeometryDocument*d,
                                       int layer_id,uint32_t vertex_id){
 int index=studio_geometry_index_by_id(d,layer_id);
 if(index<0||!vertex_id)return -1;
 const StudioGeometryLayer*l=&d->active.layers[index];
 for(int v=0;v<l->vertex_count;v++)
  if(l->vertex_ids[v]==vertex_id)return v;
 return -1;
}
void studio_geometry_init(StudioGeometryDocument*d,uint64_t s){
 if(!d)return;memset(d,0,sizeof *d);d->signature=s;
}
int studio_geometry_link_layers(StudioGeometryDocument*d,uint64_t sig,
  uint64_t revision,int parent_id,uint32_t parent_uid,
  int child_id,uint32_t child_uid,float stiffness){
 if(!fence(d,sig,revision)||parent_id==child_id||
    !isfinite(stiffness)||stiffness<0||stiffness>1||
    d->active.link_count>=STUDIO_GEOM_LINKS)return 0;
 StudioGeometrySnapshot next=d->active;
 int a=layer_at(&next,parent_id),b=layer_at(&next,child_id);
 if(a<0||b<0||vertex_at(&next.layers[a],parent_uid)<0||
    vertex_at(&next.layers[b],child_uid)<0)return 0;
 for(int i=0;i<next.link_count;i++){
  const StudioGeometryCrossLink*l=&next.links[i];
  if(l->parent_layer_id==parent_id&&l->parent_vertex_id==parent_uid&&
     l->child_layer_id==child_id&&l->child_vertex_id==child_uid)return 0;
 }
 next.links[next.link_count++]=(StudioGeometryCrossLink){
    parent_id,child_id,parent_uid,child_uid,stiffness};
 return commit(d,sig,revision,&next);
}
int studio_geometry_unlink_layers(StudioGeometryDocument*d,uint64_t sig,
  uint64_t revision,int parent_id,uint32_t parent_uid,
  int child_id,uint32_t child_uid){
 if(!fence(d,sig,revision))return 0;
 StudioGeometrySnapshot next=d->active;
 int match=-1;
 for(int i=0;i<next.link_count;i++){
  const StudioGeometryCrossLink*l=&next.links[i];
  if(l->parent_layer_id==parent_id&&l->parent_vertex_id==parent_uid&&
     l->child_layer_id==child_id&&l->child_vertex_id==child_uid){
   match=i;break;
  }
 }
 if(match<0)return 0;
 for(int i=match;i<next.link_count-1;i++)next.links[i]=next.links[i+1];
 memset(&next.links[--next.link_count],0,sizeof next.links[0]);
 return commit(d,sig,revision,&next);
}
int studio_geometry_create(StudioGeometryDocument*d,uint64_t s,uint64_t r,int id){
 if(!fence(d,s,r)||id<1||id>9999||d->active.count==STUDIO_GEOM_LAYERS)return 0;
 StudioGeometrySnapshot next=d->active;
 for(int i=0;i<next.count;i++)if(next.layers[i].id==id)return 0;
 StudioGeometryLayer *created=&next.layers[next.count];
 created->id=id;created->pin_vertex=-1;created->next_vertex_id=1;
 for(int c=0;c<4;c++)created->tint[c]=1.f;
 next.count++;
 return commit(d,s,r,&next);
}
int studio_geometry_remove(StudioGeometryDocument*d,uint64_t s,uint64_t r,int i){
 if(!fence(d,s,r)||i<0||i>=d->active.count)return 0;
 StudioGeometrySnapshot next=d->active;
 int removed_id=next.layers[i].id;
 for(int j=0;j<next.link_count;j++)
  if(next.links[j].parent_layer_id==removed_id||
     next.links[j].child_layer_id==removed_id)return 0;
 for(int j=i;j<next.count-1;j++)next.layers[j]=next.layers[j+1];
 memset(&next.layers[--next.count],0,sizeof next.layers[0]);
 return commit(d,s,r,&next);
}

int studio_geometry_vertex_add(StudioGeometryDocument*d,uint64_t s,uint64_t r,
                               int i,float x,float y){
 if(!fence(d,s,r)||!pos(x,y)||i<0||i>=d->active.count)return 0;
 StudioGeometrySnapshot next=d->active;StudioGeometryLayer*l=&next.layers[i];
 if(l->vertex_count==STUDIO_GEOM_VERTICES||!l->next_vertex_id||
    l->next_vertex_id==UINT32_MAX)return 0;
 l->vertex_ids[l->vertex_count]=l->next_vertex_id++;
 l->vertices[l->vertex_count]=(StudioGeometryPoint){x,y};
 l->uv[l->vertex_count]=(StudioGeometryPoint){0,0};
 l->weights[l->vertex_count]=1.f;
 l->parent[l->vertex_count]=-1;
 l->parent_stiffness[l->vertex_count]=0.f;
 l->vertex_count++;
 return commit(d,s,r,&next);
}
int studio_geometry_vertex_move(StudioGeometryDocument*d,uint64_t s,uint64_t r,
                                int i,int v,float x,float y){
 if(!fence(d,s,r)||!pos(x,y)||i<0||i>=d->active.count)return 0;
 StudioGeometrySnapshot next=d->active;StudioGeometryLayer*l=&next.layers[i];
 if(v<0||v>=l->vertex_count)return 0;
 l->vertices[v]=(StudioGeometryPoint){x,y};return commit(d,s,r,&next);
}
int studio_geometry_vertex_remove(StudioGeometryDocument*d,uint64_t s,uint64_t r,
                                  int i,int v){
 if(!fence(d,s,r)||i<0||i>=d->active.count)return 0;
 StudioGeometrySnapshot next=d->active;StudioGeometryLayer*l=&next.layers[i];
 if(v<0||v>=l->vertex_count)return 0;
 uint32_t removed_uid=l->vertex_ids[v];
 for(int j=0;j<next.link_count;j++){
  const StudioGeometryCrossLink*link=&next.links[j];
  if((link->parent_layer_id==l->id&&link->parent_vertex_id==removed_uid)||
     (link->child_layer_id==l->id&&link->child_vertex_id==removed_uid))return 0;
 }
 for(int k=0;k<l->tri_count;k++)
  for(int j=0;j<3;j++)if(l->triangles[k][j]==v)return 0;
 if(l->pin_vertex==v)return 0;
 for(int j=0;j<l->vertex_count;j++)if(l->parent[j]==v)return 0;
 if(l->pin_vertex>v)l->pin_vertex--;
 for(int j=v;j<l->vertex_count-1;j++){
  l->vertex_ids[j]=l->vertex_ids[j+1];
   l->weights[j]=l->weights[j+1];l->parent[j]=l->parent[j+1];
  l->parent_stiffness[j]=l->parent_stiffness[j+1];
 }
 for(int j=0;j<l->vertex_count-1;j++)if(l->parent[j]>v)--l->parent[j];
 for(int k=v;k<l->vertex_count-1;k++){
  l->vertices[k]=l->vertices[k+1];l->uv[k]=l->uv[k+1];
 }
 l->vertices[--l->vertex_count]=(StudioGeometryPoint){0,0};
 l->uv[l->vertex_count]=(StudioGeometryPoint){0,0};
 l->vertex_ids[l->vertex_count]=0;
 for(int k=0;k<l->tri_count;k++)
  for(int j=0;j<3;j++)if(l->triangles[k][j]>v)--l->triangles[k][j];
 return commit(d,s,r,&next);
}
int studio_geometry_triangle_add(StudioGeometryDocument*d,uint64_t s,uint64_t r,
                                 int i,int a,int b,int c){
 if(!fence(d,s,r)||i<0||i>=d->active.count)return 0;
 StudioGeometrySnapshot next=d->active;StudioGeometryLayer*l=&next.layers[i];
 if(l->tri_count==STUDIO_GEOM_TRIANGLES||!tri(l,a,b,c))return 0;
 for(int k=0;k<l->tri_count;k++){
  int same=0;
  for(int j=0;j<3;j++)
   if(l->triangles[k][j]==a||l->triangles[k][j]==b||l->triangles[k][j]==c)same++;
  if(same==3)return 0;
 }
 l->triangles[l->tri_count][0]=(unsigned short)a;
 l->triangles[l->tri_count][1]=(unsigned short)b;
 l->triangles[l->tri_count++][2]=(unsigned short)c;
 return commit(d,s,r,&next);
}
int studio_geometry_triangle_remove(StudioGeometryDocument*d,uint64_t s,uint64_t r,
                                    int i,int t){
 if(!fence(d,s,r)||i<0||i>=d->active.count)return 0;
 StudioGeometrySnapshot next=d->active;StudioGeometryLayer*l=&next.layers[i];
 if(t<0||t>=l->tri_count)return 0;
 for(int k=t;k<l->tri_count-1;k++)
  memcpy(l->triangles[k],l->triangles[k+1],sizeof l->triangles[k]);
 memset(l->triangles[--l->tri_count],0,sizeof l->triangles[0]);
 return commit(d,s,r,&next);
}
int studio_geometry_undo(StudioGeometryDocument*d,uint64_t s,uint64_t r){
 if(!fence(d,s,r)||!d->undo_count)return 0;
 push(d->redo,&d->redo_count,&d->active);
 d->active=d->undo[--d->undo_count];d->revision++;return 1;
}
int studio_geometry_redo(StudioGeometryDocument*d,uint64_t s,uint64_t r){
 if(!fence(d,s,r)||!d->redo_count)return 0;
 push(d->undo,&d->undo_count,&d->active);
 d->active=d->redo[--d->redo_count];d->revision++;return 1;
}

int studio_geometry_uv_set(StudioGeometryDocument*d,uint64_t s,uint64_t rev,
                          int index,int vertex,float u,float v){
 if(!fence(d,s,rev)||index<0||index>=d->active.count||
    !isfinite(u)||!isfinite(v)||fabsf(u)>8||fabsf(v)>8)return 0;
 StudioGeometrySnapshot next=d->active;StudioGeometryLayer*l=&next.layers[index];
 if(vertex<0||vertex>=l->vertex_count)return 0;
 l->uv[vertex]=(StudioGeometryPoint){u,v};
 return commit(d,s,rev,&next);
}
int studio_geometry_tint_set(StudioGeometryDocument*d,uint64_t s,uint64_t rev,
                            int index,float r,float g,float b,float a){
 if(!fence(d,s,rev)||index<0||index>=d->active.count)return 0;
 float color[4]={r,g,b,a};
 for(int i=0;i<4;i++)
  if(!isfinite(color[i])||color[i]<0||color[i]>1)return 0;
 StudioGeometrySnapshot next=d->active;
 memcpy(next.layers[index].tint,color,sizeof color);
 return commit(d,s,rev,&next);
}
int studio_geometry_pin_set(StudioGeometryDocument*d,uint64_t s,uint64_t rev,
                           int index,int vertex){
 if(!fence(d,s,rev)||index<0||index>=d->active.count)return 0;
 StudioGeometrySnapshot next=d->active;
 if(vertex< -1||vertex>=next.layers[index].vertex_count)return 0;
 next.layers[index].pin_vertex=vertex;
 return commit(d,s,rev,&next);
}
int studio_geometry_weight_set(StudioGeometryDocument*d,uint64_t s,uint64_t rev,
                               int index,int vertex,float weight){
 if(!fence(d,s,rev)||index<0||index>=d->active.count||
    !isfinite(weight)||weight<0||weight>1)return 0;
 StudioGeometrySnapshot next=d->active;
 if(vertex<0||vertex>=next.layers[index].vertex_count)return 0;
 next.layers[index].weights[vertex]=weight;return commit(d,s,rev,&next);
}
int studio_geometry_parent_set(StudioGeometryDocument*d,uint64_t s,uint64_t rev,
                               int index,int vertex,int parent,float stiffness){
 if(!fence(d,s,rev)||index<0||index>=d->active.count||
    !isfinite(stiffness)||stiffness<0||stiffness>1)return 0;
 StudioGeometrySnapshot next=d->active;StudioGeometryLayer*l=&next.layers[index];
 if(vertex<0||vertex>=l->vertex_count||parent< -1||
    parent>=l->vertex_count||parent==vertex)return 0;
 l->parent[vertex]=parent;
 l->parent_stiffness[vertex]=parent<0?0:stiffness;
 return commit(d,s,rev,&next);
}
int studio_geometry_save(const StudioGeometryDocument*d,uint64_t s,FILE*f){
 if(!d||!f||!s||d->signature!=s||!valid(&d->active))return 0;
 if(fprintf(f,"STUDIO_GEOMETRY_5 %llu %llu %d %d\n",
  (unsigned long long)s,(unsigned long long)d->revision,
   d->active.count,d->active.link_count)<0)return 0;
 for(int i=0;i<d->active.count;i++){
  const StudioGeometryLayer*l=&d->active.layers[i];
  if(fprintf(f,"L %d %d %d %d %.9g %.9g %.9g %.9g %u\n",
      l->id,l->vertex_count,l->tri_count,l->pin_vertex,
      l->tint[0],l->tint[1],l->tint[2],l->tint[3],
       (unsigned)l->next_vertex_id)<0)return 0;
  for(int j=0;j<l->vertex_count;j++)
   if(fprintf(f,"V %.9g %.9g %.9g %.9g\n",
       l->vertices[j].x,l->vertices[j].y,l->uv[j].x,l->uv[j].y)<0||
      fprintf(f,"R %.9g %d %.9g %u\n",
       l->weights[j],l->parent[j],l->parent_stiffness[j],
        (unsigned)l->vertex_ids[j])<0)return 0;
  for(int j=0;j<l->tri_count;j++)
   if(fprintf(f,"T %hu %hu %hu\n",l->triangles[j][0],
      l->triangles[j][1],l->triangles[j][2])<0)return 0;
 }
 for(int i=0;i<d->active.link_count;i++){
  const StudioGeometryCrossLink*l=&d->active.links[i];
  if(fprintf(f,"C %d %u %d %u %.9g\n",l->parent_layer_id,
     (unsigned)l->parent_vertex_id,l->child_layer_id,
     (unsigned)l->child_vertex_id,l->stiffness)<0)return 0;
 }
 return fprintf(f,"END\n")>0&&fflush(f)==0&&!ferror(f);
}
int studio_geometry_load(StudioGeometryDocument*d,uint64_t s,FILE*f){
 if(!d||!f||!s)return 0;
 StudioGeometrySnapshot*temp=calloc(1,sizeof *temp);
 if(!temp)return 0;
 char word[40]={0};unsigned long long sig=0,rev=0;int count=0,ok=0;
 if(fscanf(f,"%39s %llu %llu %d",word,&sig,&rev,&count)!=4||
    (strcmp(word,"STUDIO_GEOMETRY_1")&&
     strcmp(word,"STUDIO_GEOMETRY_2")&&
      strcmp(word,"STUDIO_GEOMETRY_3")&&
      strcmp(word,"STUDIO_GEOMETRY_4")&&strcmp(word,"STUDIO_GEOMETRY_5"))||
    sig!=s||rev>=INT64_MAX||
    count<0||count>STUDIO_GEOM_LAYERS)goto end;
 int legacy=!strcmp(word,"STUDIO_GEOMETRY_1");
 int linked=!strcmp(word,"STUDIO_GEOMETRY_5");
 int stable=linked||!strcmp(word,"STUDIO_GEOMETRY_4");
 if(linked){
  if(fscanf(f,"%d",&temp->link_count)!=1||
     temp->link_count<0||temp->link_count>STUDIO_GEOM_LINKS)goto end;
 }
 int rigged=stable||!strcmp(word,"STUDIO_GEOMETRY_3");
 temp->count=count;
 for(int i=0;i<count;i++){
  StudioGeometryLayer*l=&temp->layers[i];
  l->pin_vertex=-1;
  for(int c=0;c<4;c++)l->tint[c]=1.f;
  if(legacy){
   if(fscanf(f,"%39s %d %d %d",word,&l->id,&l->vertex_count,&l->tri_count)!=4)
    goto end;
  }else if(stable){
   unsigned next=0;
   if(fscanf(f,"%39s %d %d %d %d %f %f %f %f %u",word,&l->id,
      &l->vertex_count,&l->tri_count,&l->pin_vertex,
      &l->tint[0],&l->tint[1],&l->tint[2],&l->tint[3],&next)!=10)goto end;
   l->next_vertex_id=(uint32_t)next;
  }else{
   if(fscanf(f,"%39s %d %d %d %d %f %f %f %f",word,&l->id,
      &l->vertex_count,&l->tri_count,&l->pin_vertex,
      &l->tint[0],&l->tint[1],&l->tint[2],&l->tint[3])!=9)goto end;
  }
  if(strcmp(word,"L")||l->vertex_count<0||l->vertex_count>STUDIO_GEOM_VERTICES||
     l->tri_count<0||l->tri_count>STUDIO_GEOM_TRIANGLES)goto end;
  if(!stable)l->next_vertex_id=(uint32_t)(l->vertex_count+1);
  for(int j=0;j<l->vertex_count;j++){
   l->vertex_ids[j]=(uint32_t)(j+1);
   if(legacy){
    if(fscanf(f,"%39s %f %f",word,&l->vertices[j].x,&l->vertices[j].y)!=3)
     goto end;
   }else if(fscanf(f,"%39s %f %f %f %f",word,
         &l->vertices[j].x,&l->vertices[j].y,
         &l->uv[j].x,&l->uv[j].y)!=5)goto end;
   if(strcmp(word,"V"))goto end;
   l->weights[j]=1.f;l->parent[j]=-1;l->parent_stiffness[j]=0.f;
   if(stable){
    unsigned uid=0;
    if(fscanf(f,"%39s %f %d %f %u",word,&l->weights[j],
       &l->parent[j],&l->parent_stiffness[j],&uid)!=5||
       strcmp(word,"R"))goto end;
    l->vertex_ids[j]=(uint32_t)uid;
   }else if(rigged){
    if(fscanf(f,"%39s %f %d %f",word,&l->weights[j],
       &l->parent[j],&l->parent_stiffness[j])!=4||
       strcmp(word,"R"))goto end;
   }
  }
  for(int j=0;j<l->tri_count;j++){
   unsigned a=0,b=0,c=0;
   if(fscanf(f,"%39s %u %u %u",word,&a,&b,&c)!=4||
      strcmp(word,"T")||a>= (unsigned)l->vertex_count||
      b>= (unsigned)l->vertex_count||c>= (unsigned)l->vertex_count||
      a>65535u||b>65535u||c>65535u)goto end;
   l->triangles[j][0]=(unsigned short)a;
   l->triangles[j][1]=(unsigned short)b;
   l->triangles[j][2]=(unsigned short)c;
  }
 }
 for(int i=0;i<temp->link_count;i++){
  StudioGeometryCrossLink*l=&temp->links[i];
  unsigned parent_uid=0,child_uid=0;
  if(fscanf(f,"%39s %d %u %d %u %f",word,&l->parent_layer_id,
     &parent_uid,&l->child_layer_id,&child_uid,&l->stiffness)!=6||
     strcmp(word,"C"))goto end;
  l->parent_vertex_id=(uint32_t)parent_uid;
  l->child_vertex_id=(uint32_t)child_uid;
 }
 char tail=0;
 if(fscanf(f,"%39s",word)!=1||strcmp(word,"END")||
    fscanf(f," %c",&tail)!=EOF||!valid(temp))goto end;
 d->active=*temp;d->signature=s;d->revision=(uint64_t)rev;
 d->undo_count=d->redo_count=0;ok=1;
end:
 free(temp);return ok;
}

int studio_geometry_import_with_uv(StudioGeometryDocument*d,uint64_t sig,
  uint64_t rev,int id,const StudioGeometryPoint*positions,
  const StudioGeometryPoint*uvs,int count,
  const unsigned short*indices,int index_count){
 if(!fence(d,sig,rev)||id<1||id>9999||!positions||!indices||
    count<3||count>STUDIO_GEOM_VERTICES||index_count<3||
    index_count>STUDIO_GEOM_TRIANGLES*3||index_count%3||
    d->active.count==STUDIO_GEOM_LAYERS)return 0;
 StudioGeometrySnapshot next=d->active;
 for(int i=0;i<next.count;i++)if(next.layers[i].id==id)return 0;
 StudioGeometryLayer*l=&next.layers[next.count];l->id=id;
 l->pin_vertex=-1;l->next_vertex_id=(uint32_t)(count+1);
 for(int c=0;c<4;c++)l->tint[c]=1.f;
 l->vertex_count=count;l->tri_count=index_count/3;
 for(int i=0;i<count;i++){
  l->vertices[i]=positions[i];
  l->uv[i]=uvs?uvs[i]:(StudioGeometryPoint){0,0};
  l->vertex_ids[i]=(uint32_t)(i+1);
  l->weights[i]=1.f;l->parent[i]=-1;l->parent_stiffness[i]=0.f;
 }
 for(int i=0;i<l->tri_count;i++)
  for(int j=0;j<3;j++)l->triangles[i][j]=indices[i*3+j];
 next.count++;
 return commit(d,sig,rev,&next);
}

int studio_geometry_import(StudioGeometryDocument*d,uint64_t sig,uint64_t rev,int id,
  const StudioGeometryPoint*positions,int count,
  const unsigned short*indices,int index_count){
 return studio_geometry_import_with_uv(d,sig,rev,id,positions,NULL,count,indices,index_count);
}
int studio_geometry_duplicate(StudioGeometryDocument*d,uint64_t sig,uint64_t rev,
                              int index,int id){
 if(!fence(d,sig,rev)||index<0||index>=d->active.count||
    d->active.count==STUDIO_GEOM_LAYERS||id<1||id>9999)return 0;
 StudioGeometrySnapshot next=d->active;
 for(int i=0;i<next.count;i++)if(next.layers[i].id==id)return 0;
 next.layers[next.count]=next.layers[index];
 next.layers[next.count].id=id;next.count++;
 return commit(d,sig,rev,&next);
}
int studio_geometry_reorder(StudioGeometryDocument*d,uint64_t sig,uint64_t rev,
                            int source,int target){
 if(!fence(d,sig,rev)||source<0||target<0||
    source>=d->active.count||target>=d->active.count)return 0;
 StudioGeometrySnapshot next=d->active;
 StudioGeometryLayer temp=next.layers[source];
 if(source<target)
  for(int i=source;i<target;i++)next.layers[i]=next.layers[i+1];
 else if(source>target)
  for(int i=source;i>target;i--)next.layers[i]=next.layers[i-1];
 next.layers[target]=temp;
 return commit(d,sig,rev,&next);
}
