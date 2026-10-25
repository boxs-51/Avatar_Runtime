/* Local geometry project, never a MOC3 writer. */
static int geometry_project_io(int save){
 char path[1200],temp[1200],bak[1200];
 if(snprintf(path,sizeof path,"%s/layer_geometry.txt",s.folder)>=(int)sizeof path||
    snprintf(temp,sizeof temp,"%s/layer_geometry.tmp",s.folder)>=(int)sizeof temp||
    snprintf(bak,sizeof bak,"%s/layer_geometry.bak",s.folder)>=(int)sizeof bak)return 0;
 if(save){
  FILE *f=fopen(temp,"wb");if(!f)return 0;
  int ok=studio_geometry_save(&geometry,geometry_signature(),f);
  if(fclose(f))ok=0;
  if(!ok){remove(temp);return 0;}
  FILE *source=fopen(path,"rb");
  if(source){
   FILE *backup=fopen(bak,"wb");
   if(backup){char buf[2048];size_t n;
    while((n=fread(buf,1,sizeof buf,source))>0)
     if(fwrite(buf,1,n,backup)!=n)break;
    fclose(backup);
   }
   fclose(source);
  }
  if(!studio_replace_file(temp,path)){remove(temp);return 0;}
  return 1;
 }
 FILE*f=fopen(path,"rb");if(!f)return 0;
 StudioGeometryDocument*candidate=malloc(sizeof *candidate);
 if(!candidate){fclose(f);return 0;}
 studio_geometry_init(candidate,geometry_signature());
 int ok=studio_geometry_load(candidate,geometry_signature(),f);
 if(fclose(f))ok=0;
 if(ok&&geometry.revision<INT64_MAX-1){
  candidate->revision=geometry.revision+1;
  geometry=*candidate;geometry_selected=-1;geometry_vertex=-1;
 }else ok=0;
 free(candidate);return ok;
}
static int geometry_runtime_io(int compile){
 char file[1200],temporary[1200],backup[1200];
 if(snprintf(file,sizeof file,"%s/model_runtime.armesh",s.folder)>=(int)sizeof file||
    snprintf(temporary,sizeof temporary,"%s/model_runtime.tmp",s.folder)>=(int)sizeof temporary||
    snprintf(backup,sizeof backup,"%s/model_runtime.bak",s.folder)>=(int)sizeof backup)return 0;
 if(compile){
  FILE*f=fopen(temporary,"wb");if(!f)return 0;
  int ok=studio_geometry_runtime_compile(&geometry,geometry_signature(),f);
  if(fclose(f))ok=0;
  if(!ok){remove(temporary);return 0;}
  FILE*check=fopen(temporary,"rb");
  StudioGeometryDocument*candidate=calloc(1,sizeof *candidate);
  ok=check&&candidate&&studio_geometry_runtime_load(candidate,geometry_signature(),check);
  if(check&&fclose(check))ok=0;
  free(candidate);
  if(!ok){remove(temporary);return 0;}
  /* Preserve the previous native compiled asset before replacement. */
  FILE*prior=fopen(file,"rb");
  if(prior){
   FILE*dest=fopen(backup,"wb");
   if(!dest){fclose(prior);remove(temporary);return 0;}
   char bytes[4096];size_t n;int copied=1;
   while((n=fread(bytes,1,sizeof bytes,prior))>0)
    if(fwrite(bytes,1,n,dest)!=n){copied=0;break;}
   if(ferror(prior))copied=0;
   if(fflush(dest))copied=0;
   if(fclose(dest))copied=0;
   fclose(prior);
   if(!copied){remove(temporary);return 0;}
  }
  if(!studio_replace_file(temporary,file)){remove(temporary);return 0;}
  return 1;
 }
 FILE*f=fopen(file,"rb");if(!f)return 0;
 StudioGeometryDocument*candidate=calloc(1,sizeof *candidate);
 if(!candidate){fclose(f);return 0;}
 int ok=studio_geometry_runtime_load(candidate,geometry_signature(),f);
 if(fclose(f))ok=0;
 if(ok&&geometry.revision<INT64_MAX-1){
  candidate->revision=geometry.revision+1;
  geometry=*candidate;geometry_selected=-1;geometry_vertex=-1;
 }else ok=0;
 free(candidate);return ok;
}
static int studio_geometry_agent(FILE*f,const char*op,char*receipt,size_t cap){
 if(strncmp(op,"GEOM_",5))return -1;
 if(!s.core.model||!s.model_generation)return 0;
 unsigned long long epoch=0,revision=0;
 if(fscanf(f,"%llu",&epoch)!=1||epoch!=s.model_generation||
    geometry.signature!=geometry_signature())return 0;
 if(!strcmp(op,"GEOM_LAYER_BY_ID")||!strcmp(op,"GEOM_ACTIVE_ID")){
  unsigned long long expected=0;
  int id=0;
  if(fscanf(f,"%llu %d",&expected,&id)!=2||!studio_agent_eof(f)||
     expected!=geometry.revision)return 0;
  int index=studio_geometry_index_by_id(&geometry,id);
  if(index<0)return 0;
  const StudioGeometryLayer*l=&geometry.active.layers[index];
  if(!strcmp(op,"GEOM_ACTIVE_ID")){
    geometry_selected=index;geometry_vertex=-1;s.tab=1;geometry_mode=1;
    return snprintf(receipt,cap,"layer=%d id=%d revision=%llu",
       index,id,(unsigned long long)geometry.revision)>0;
  }
  return snprintf(receipt,cap,
     "layer=%d id=%d vertices=%d triangles=%d revision=%llu",
     index,l->id,l->vertex_count,l->tri_count,
     (unsigned long long)geometry.revision)>0;
 }
 if(!strcmp(op,"GEOM_VERTEX_BY_ID")){
  unsigned long long revision=0;int id=0;unsigned uid=0;
  if(fscanf(f,"%llu %d %u",&revision,&id,&uid)!=3||
     !studio_agent_eof(f)||revision!=geometry.revision)return 0;
  int layer=studio_geometry_index_by_id(&geometry,id);
  int vertex=studio_geometry_vertex_index_by_id(&geometry,id,(uint32_t)uid);
  if(layer<0||vertex<0)return 0;
  return snprintf(receipt,cap,
     "layer=%d id=%d vertex=%d vertex_id=%u revision=%llu",
     layer,id,vertex,uid,(unsigned long long)geometry.revision)>0;
 }
 if(!strcmp(op,"GEOM_LAYER")||!strcmp(op,"GEOM_POINT")||
    !strcmp(op,"GEOM_FACE")){
  int index=-1,entry=-1;
  int detail=strcmp(op,"GEOM_LAYER")!=0;
  if(fscanf(f,"%d",&index)!=1||
    (detail&&fscanf(f,"%d",&entry)!=1)||!studio_agent_eof(f)||
    index<0||index>=geometry.active.count)return 0;
  const StudioGeometryLayer*l=&geometry.active.layers[index];
  if(!detail)
   return snprintf(receipt,cap,
      "layer=%d id=%d vertices=%d triangles=%d pin=%d r=%.6g g=%.6g b=%.6g alpha=%.6g revision=%llu",
      index,l->id,l->vertex_count,l->tri_count,l->pin_vertex,
      l->tint[0],l->tint[1],l->tint[2],l->tint[3],
      (unsigned long long)geometry.revision)>0;
  if(!strcmp(op,"GEOM_POINT")){
   if(entry<0||entry>=l->vertex_count)return 0;
   return snprintf(receipt,cap,"layer=%d index=%d vertex_id=%u x=%.9g y=%.9g u=%.9g v=%.9g weight=%.9g parent=%d stiffness=%.9g revision=%llu",
         index,entry,(unsigned)l->vertex_ids[entry],
         l->vertices[entry].x,l->vertices[entry].y,
          l->uv[entry].x,l->uv[entry].y,
         l->weights[entry],l->parent[entry],l->parent_stiffness[entry],
         (unsigned long long)geometry.revision)>0;
  }
  if(entry<0||entry>=l->tri_count)return 0;
  return snprintf(receipt,cap,"layer=%d index=%d a=%hu b=%hu c=%hu revision=%llu",
         index,entry,l->triangles[entry][0],l->triangles[entry][1],
         l->triangles[entry][2],(unsigned long long)geometry.revision)>0;
 }
 if(!strcmp(op,"GEOM_PHYSICS_POINT_BY_ID")){
  int layer_id=0;unsigned vertex_id=0;
  if(fscanf(f,"%d %u",&layer_id,&vertex_id)!=2||!studio_agent_eof(f)||
     !geometry_xpbd||geometry_bound_revision!=geometry.revision||
     !geometry_accepted_frames||
     geometry_accepted_revision!=geometry.revision)return 0;
  int index=studio_geometry_index_by_id(&geometry,layer_id);
  int vertex=studio_geometry_vertex_index_by_id(&geometry,layer_id,
                                               (uint32_t)vertex_id);
  if(index<0||vertex<0)return 0;
  const StudioGeometryPoint*positions=
       studio_geometry_xpbd_positions_for_layer(geometry_xpbd,index);
  if(!positions&&index==geometry_bound_layer&&geometry.active.link_count==0)
   positions=studio_geometry_xpbd_positions(geometry_xpbd);
  if(!positions)return 0;
  return snprintf(receipt,cap,
      "id=%d vertex_id=%u x=%.9g y=%.9g revision=%llu accepted=%d gpu=1",
      layer_id,vertex_id,positions[vertex].x,positions[vertex].y,
      (unsigned long long)geometry_accepted_revision,geometry_accepted_frames)>0;
 }
 if(!strcmp(op,"GEOM_PHYSICS_POINT")){
  int vertex=-1;
  if(fscanf(f,"%d",&vertex)!=1||!studio_agent_eof(f)||
     !geometry_xpbd||geometry_bound_layer<0||
     geometry_bound_layer>=geometry.active.count||
     geometry_bound_revision!=geometry.revision||
     !geometry_accepted_frames)return 0;
  const StudioGeometryLayer*l=&geometry.active.layers[geometry_bound_layer];
  const StudioGeometryPoint*positions=studio_geometry_xpbd_positions(geometry_xpbd);
  if(!positions||vertex<0||vertex>=l->vertex_count)return 0;
  return snprintf(receipt,cap,
    "vertex=%d x=%.9g y=%.9g revision=%llu accepted=%d",
    vertex,positions[vertex].x,positions[vertex].y,
    (unsigned long long)geometry_accepted_revision,geometry_accepted_frames)>0;
 }
 if(!strcmp(op,"GEOM_PHYSICS_STATUS")){
  if(!studio_agent_eof(f))return 0;
  return snprintf(receipt,cap,
    "active=%d layer=%d revision=%llu bound=%llu accepted=%d gpu=%d g5_applied=0 links=%d",
    geometry_xpbd?1:0,geometry_bound_layer,
    (unsigned long long)geometry.revision,
    (unsigned long long)geometry_bound_revision,
    geometry_accepted_frames,
    geometry_accepted_frames>0&&geometry_accepted_revision==geometry.revision,
    geometry.active.link_count)>0;
 }
 if(!strcmp(op,"GEOM_RIG_LINK_INSPECT")){
  unsigned long long revision=0;int index=-1;
  if(fscanf(f,"%llu %d",&revision,&index)!=2||!studio_agent_eof(f)||
     revision!=geometry.revision||index<0||
     index>=geometry.active.link_count)return 0;
  const StudioGeometryCrossLink*l=&geometry.active.links[index];
  return snprintf(receipt,cap,
     "index=%d parent_id=%d parent_vertex_id=%u child_id=%d child_vertex_id=%u stiffness=%.9g revision=%llu",
     index,l->parent_layer_id,(unsigned)l->parent_vertex_id,
     l->child_layer_id,(unsigned)l->child_vertex_id,l->stiffness,
     (unsigned long long)geometry.revision)>0;
 }
 if(!strcmp(op,"GEOM_RIG_LINKS")){
  if(!studio_agent_eof(f))return 0;
  return snprintf(receipt,cap,"links=%d revision=%llu",
    geometry.active.link_count,(unsigned long long)geometry.revision)>0;
 }
 if(!strcmp(op,"GEOM_STATUS")){
  if(!studio_agent_eof(f))return 0;
  return snprintf(receipt,cap,"revision=%llu layers=%d active=%d undo=%d redo=%d",
   (unsigned long long)geometry.revision,geometry.active.count,
   geometry_selected,geometry.undo_count,geometry.redo_count)>0;
 }
 if(!strcmp(op,"GEOM_ACTIVE")){
  int index=-1;
  if(fscanf(f,"%d",&index)!=1||!studio_agent_eof(f)||
     index<0||index>=geometry.active.count)return 0;
  geometry_selected=index;geometry_vertex=-1;s.tab=1;geometry_mode=1;
  snprintf(receipt,cap,"active=%d",index);return 1;
 }
 if(fscanf(f,"%llu",&revision)!=1||revision!=geometry.revision)return 0;
 int index=0,v=0,a=0,b=0,c=0,id=0,ok=0;float x=0,y=0;
 if(!strcmp(op,"GEOM_COMPILE")||!strcmp(op,"GEOM_OPEN_COMPILED")){
  if(!studio_agent_eof(f))return 0;
  ok=geometry_runtime_io(!strcmp(op,"GEOM_COMPILE"));
  return ok&&snprintf(receipt,cap,
     "revision=%llu format=ARS_MESH_RUNTIME_3 compiled=%d",
     (unsigned long long)geometry.revision,1)>0;
 }

 if(!strcmp(op,"GEOM_PHYSICS_BIND")){
  if(fscanf(f,"%d",&index)!=1||!studio_agent_eof(f)||
     index<0||index>=geometry.active.count)return 0;
  int previous=geometry_selected;
  geometry_selected=index;
  ok=geometry_bind_physics();
  if(!ok)geometry_selected=previous;
  if(ok)s.tab=1;
  return ok&&snprintf(receipt,cap,"layer=%d revision=%llu bound=1",
    index,(unsigned long long)geometry.revision)>0;
 }
 if(!strcmp(op,"GEOM_PHYSICS_STOP")){
  if(!studio_agent_eof(f))return 0;
  geometry_stop_physics();
  return snprintf(receipt,cap,"bound=0")>0;
 }

 if(!strcmp(op,"GEOM_RIG_LINK")||!strcmp(op,"GEOM_RIG_UNLINK")){
  int parent_id=0,child_id=0;unsigned parent_uid=0,child_uid=0;
  float stiffness=0;
  int linking=!strcmp(op,"GEOM_RIG_LINK");
  if(fscanf(f,"%d %u %d %u",&parent_id,&parent_uid,
     &child_id,&child_uid)!=4||
     (linking&&fscanf(f,"%f",&stiffness)!=1)||
     !studio_agent_eof(f))return 0;
  ok=linking?studio_geometry_link_layers(&geometry,geometry_signature(),
      revision,parent_id,parent_uid,child_id,child_uid,stiffness):
      studio_geometry_unlink_layers(&geometry,geometry_signature(),
      revision,parent_id,parent_uid,child_id,child_uid);
  if(ok){
   geometry_selected=studio_geometry_index_by_id(&geometry,child_id);
   geometry_vertex=studio_geometry_vertex_index_by_id(&geometry,child_id,
                           (uint32_t)child_uid);
  }
 }else if(!strcmp(op,"GEOM_CREATE")){
  if(fscanf(f,"%d",&id)!=1||!studio_agent_eof(f))return 0;
  ok=studio_geometry_create(&geometry,geometry_signature(),revision,id);
  if(ok){geometry_selected=geometry.active.count-1;geometry_vertex=-1;s.tab=1;geometry_mode=1;}
 }else if(!strcmp(op,"GEOM_COPY_DRAWABLE")){
  int drawable=-1;
  if(fscanf(f,"%d %d",&drawable,&id)!=2||!studio_agent_eof(f))return 0;
  ok=geometry_import_drawable(drawable,id);
 }else if(!strcmp(op,"GEOM_DELETE")){
  if(fscanf(f,"%d",&index)!=1||!studio_agent_eof(f))return 0;
  ok=studio_geometry_remove(&geometry,geometry_signature(),revision,index);
  if(ok){geometry_selected=-1;geometry_vertex=-1;}
 }else if(!strcmp(op,"GEOM_DUPLICATE")){
  if(fscanf(f,"%d %d",&index,&id)!=2||!studio_agent_eof(f))return 0;
  ok=studio_geometry_duplicate(&geometry,geometry_signature(),revision,index,id);
  if(ok){geometry_selected=geometry.active.count-1;geometry_vertex=-1;}
 }else if(!strcmp(op,"GEOM_REORDER")){
  if(fscanf(f,"%d %d",&index,&v)!=2||!studio_agent_eof(f))return 0;
  ok=studio_geometry_reorder(&geometry,geometry_signature(),revision,index,v);
  if(ok){geometry_selected=v;geometry_vertex=-1;}
 }else if(!strcmp(op,"GEOM_UV_SET")){
  float u=0,w=0;
  if(fscanf(f,"%d %d %f %f",&index,&v,&u,&w)!=4||
     !studio_agent_eof(f))return 0;
  ok=studio_geometry_uv_set(&geometry,geometry_signature(),revision,index,v,u,w);
 }else if(!strcmp(op,"GEOM_MATERIAL_SET")){
  float r=0,g=0,bv=0,alpha=0;
  if(fscanf(f,"%d %f %f %f %f",&index,&r,&g,&bv,&alpha)!=5||
     !studio_agent_eof(f))return 0;
  ok=studio_geometry_tint_set(&geometry,geometry_signature(),revision,index,r,g,bv,alpha);
 }else if(!strcmp(op,"GEOM_RIG_WEIGHT")){
  float weight=0;
  if(fscanf(f,"%d %d %f",&index,&v,&weight)!=3||!studio_agent_eof(f))return 0;
  ok=studio_geometry_weight_set(&geometry,geometry_signature(),
                                revision,index,v,weight);
 }else if(!strcmp(op,"GEOM_RIG_PARENT")){
  int parent=-1;float stiffness=0;
  if(fscanf(f,"%d %d %d %f",&index,&v,&parent,&stiffness)!=4||
     !studio_agent_eof(f))return 0;
  ok=studio_geometry_parent_set(&geometry,geometry_signature(),
                                revision,index,v,parent,stiffness);
 }else if(!strcmp(op,"GEOM_PIN_SET")){
  if(fscanf(f,"%d %d",&index,&v)!=2||!studio_agent_eof(f))return 0;
  ok=studio_geometry_pin_set(&geometry,geometry_signature(),revision,index,v);
 }else if(!strcmp(op,"GEOM_VERTEX_ADD")){
  if(fscanf(f,"%d %f %f",&index,&x,&y)!=3||!studio_agent_eof(f))return 0;
  ok=studio_geometry_vertex_add(&geometry,geometry_signature(),revision,index,x,y);
 }else if(!strcmp(op,"GEOM_VERTEX_MOVE")){
  if(fscanf(f,"%d %d %f %f",&index,&v,&x,&y)!=4||!studio_agent_eof(f))return 0;
  ok=studio_geometry_vertex_move(&geometry,geometry_signature(),revision,index,v,x,y);
 }else if(!strcmp(op,"GEOM_VERTEX_DELETE")){
  if(fscanf(f,"%d %d",&index,&v)!=2||!studio_agent_eof(f))return 0;
  ok=studio_geometry_vertex_remove(&geometry,geometry_signature(),revision,index,v);
 }else if(!strcmp(op,"GEOM_TRIANGLE_ADD")){
  if(fscanf(f,"%d %d %d %d",&index,&a,&b,&c)!=4||!studio_agent_eof(f))return 0;
  ok=studio_geometry_triangle_add(&geometry,geometry_signature(),revision,index,a,b,c);
 }else if(!strcmp(op,"GEOM_TRIANGLE_DELETE")){
  if(fscanf(f,"%d %d",&index,&v)!=2||!studio_agent_eof(f))return 0;
  ok=studio_geometry_triangle_remove(&geometry,geometry_signature(),revision,index,v);
 }else if(!strcmp(op,"GEOM_UNDO")){
  if(!studio_agent_eof(f))return 0;
  ok=studio_geometry_undo(&geometry,geometry_signature(),revision);
  if(ok){geometry_selected=-1;geometry_vertex=-1;}
 }else if(!strcmp(op,"GEOM_REDO")){
  if(!studio_agent_eof(f))return 0;
  ok=studio_geometry_redo(&geometry,geometry_signature(),revision);
  if(ok){geometry_selected=-1;geometry_vertex=-1;}
 }else if(!strcmp(op,"GEOM_SAVE")||!strcmp(op,"GEOM_LOAD")){
  if(!studio_agent_eof(f))return 0;
  ok=geometry_project_io(!strcmp(op,"GEOM_SAVE"));
 }else return -1;
 if(!ok)return 0;
 return snprintf(receipt,cap,"revision=%llu layers=%d active=%d links=%d",
      (unsigned long long)geometry.revision,geometry.active.count,
      geometry_selected,geometry.active.link_count)>0;
}
