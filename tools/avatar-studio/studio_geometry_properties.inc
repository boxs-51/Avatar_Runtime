static void geometry_properties_ui(float x){
 if(geometry_selected<0||geometry_selected>=geometry.active.count){
  label("Select an editable layer first.",x,477,13,MUTED);return;
 }
 const StudioGeometryLayer*l=&geometry.active.layers[geometry_selected];
 if(geometry_property_tab==4){
  label(TextFormat("Rig links: %d / %d",
      geometry.active.link_count,STUDIO_GEOM_LINKS),x,478,14,ACCENT);
  int selected=geometry_vertex;
  int selected_valid=selected>=0&&selected<l->vertex_count;
  if(selected_valid)
   label(TextFormat("Child: Layer %d / Vertex ID %u",l->id,
         (unsigned)l->vertex_ids[selected]),x,515,12,TEXT);
  else label("Choose a vertex in the viewport.",x,515,12,MUTED);
  if(button((Rectangle){x,542,134,30},"Mark parent",0)){
   if(selected_valid){
    geometry_link_parent_id=l->id;
    geometry_link_parent_vertex_id=l->vertex_ids[selected];
    geometry_link_mark_revision=geometry.revision;
    geometry_link_mark_signature=geometry_signature();
    toast("Rig parent marked; choose child layer + vertex.");
   }else toast("Select a vertex before marking parent.");
  }
  if(button((Rectangle){x+144,542,136,30},"Link to child",0)){
   if(selected_valid&&geometry_link_parent_id>0&&
      geometry_link_parent_id!=l->id&&
      geometry_link_mark_revision==geometry.revision&&
      geometry_link_mark_signature==geometry_signature()&&
      studio_geometry_link_layers(&geometry,geometry_signature(),
          geometry.revision,geometry_link_parent_id,
          geometry_link_parent_vertex_id,l->id,l->vertex_ids[selected],.8f)){
     geometry_link_parent_id=0;geometry_link_parent_vertex_id=0;
     geometry_link_mark_revision=0;geometry_link_mark_signature=0;
     toast("Cross-layer Rig link created.");
   }else toast("Link rejected: mark parent and select valid child.");
  }
  if(geometry_link_parent_id>0)
   label(TextFormat("Marked: Layer %d / Vertex ID %u",
         geometry_link_parent_id,
         (unsigned)geometry_link_parent_vertex_id),x,584,12,TEXT);
  else label("Mark parent, select child, then link.",x,584,12,MUTED);
  label("Cross-layer parent links (0.8 stiffness)",x,617,11,MUTED);
  int pages=(geometry.active.link_count+3)/4;
  if(pages<1)pages=1;
  if(geometry_link_page>=pages)geometry_link_page=pages-1;
  if(button((Rectangle){x,645,58,26},"Prev",0)&&geometry_link_page>0)
   --geometry_link_page;
  label(TextFormat("Page %d/%d",geometry_link_page+1,pages),
        x+88,652,12,TEXT);
  if(button((Rectangle){x+222,645,58,26},"Next",0)&&
     geometry_link_page+1<pages)++geometry_link_page;
  int start=geometry_link_page*4;
  for(int j=start;j<geometry.active.link_count&&j<start+4;j++){
   StudioGeometryCrossLink link=geometry.active.links[j];
   float cy=687.f+(j-start)*35.f;
   label(TextFormat("L%d/V%u -> L%d/V%u",
          link.parent_layer_id,(unsigned)link.parent_vertex_id,
          link.child_layer_id,(unsigned)link.child_vertex_id),
          x,cy+3,11,TEXT);
   if(button((Rectangle){x+232,cy,48,25},"X",0)){
    if(studio_geometry_unlink_layers(&geometry,geometry_signature(),
        geometry.revision,link.parent_layer_id,link.parent_vertex_id,
        link.child_layer_id,link.child_vertex_id)){
     toast("Cross-layer Rig link removed.");
    }else toast("Unlink rejected.");
    return;
   }
  }
  return;
 }
 if(geometry_property_tab==3){
  label(TextFormat("Editable layers: %d",geometry.active.count),x,475,14,ACCENT);
  label(TextFormat("Model revision: %llu",
          (unsigned long long)geometry.revision),x,511,12,TEXT);
  if(button((Rectangle){x,548,134,32},"Save source",0))
   toast(geometry_project_io(1)?"Source saved.":"Source save rejected.");
  if(button((Rectangle){x+144,548,136,32},"Open source",0))
   toast(geometry_project_io(0)?"Source loaded.":"Source invalid.");
  if(button((Rectangle){x,601,134,34},"Compile mesh",0))
   toast(geometry_runtime_io(1)?"Native mesh compiled.":"Compile failed.");
  if(button((Rectangle){x+144,601,136,34},"Open compiled",0))
   toast(geometry_runtime_io(0)?"Compiled mesh reopened.":"Invalid compiled mesh.");
  label("Output: model_runtime.armesh",x,657,12,TEXT);
  label("Format: ARSMESH1 (local Studio)",x,690,12,MUTED);
  label("Original MOC3 is read-only.",x,726,12,ACCENT);
  return;
 }
 if(geometry_property_tab==1){
  label(TextFormat("Layer %d / UV + Material",l->id),x,478,14,ACCENT);
  int v=geometry_vertex;
  if(v<0||v>=l->vertex_count)v=l->vertex_count?0:-1;
  if(v>=0){
   StudioGeometryPoint uv=l->uv[v];
   label(TextFormat("Vertex %d | U %.2f V %.2f",v,uv.x,uv.y),x,524,12,TEXT);
   if(button((Rectangle){x,556,64,28},"U -",0))
    (void)studio_geometry_uv_set(&geometry,geometry_signature(),geometry.revision,
        geometry_selected,v,uv.x-.05f,uv.y);
   if(button((Rectangle){x+72,556,64,28},"U +",0))
    (void)studio_geometry_uv_set(&geometry,geometry_signature(),geometry.revision,
        geometry_selected,v,uv.x+.05f,uv.y);
   if(button((Rectangle){x+144,556,64,28},"V -",0))
    (void)studio_geometry_uv_set(&geometry,geometry_signature(),geometry.revision,
        geometry_selected,v,uv.x,uv.y-.05f);
   if(button((Rectangle){x+216,556,64,28},"V +",0))
    (void)studio_geometry_uv_set(&geometry,geometry_signature(),geometry.revision,
        geometry_selected,v,uv.x,uv.y+.05f);
  }
  label("Material tint + alpha",x,615,13,TEXT);
  Color color={(unsigned char)(l->tint[0]*255),
               (unsigned char)(l->tint[1]*255),
               (unsigned char)(l->tint[2]*255),255};
  DrawRectangle((int)x,642,54,30,color);
  label(TextFormat("RGBA %.2f %.2f %.2f %.2f",l->tint[0],
      l->tint[1],l->tint[2],l->tint[3]),x+64,650,11,MUTED);

  if(button((Rectangle){x,691,88,30},"Blue",0))
   (void)studio_geometry_tint_set(&geometry,geometry_signature(),geometry.revision,
                     geometry_selected,.23f,.60f,1.f,l->tint[3]);
  if(button((Rectangle){x+96,691,88,30},"Amber",0))
   (void)studio_geometry_tint_set(&geometry,geometry_signature(),geometry.revision,
                     geometry_selected,1.f,.62f,.29f,l->tint[3]);
  if(button((Rectangle){x+192,691,88,30},"White",0))
   (void)studio_geometry_tint_set(&geometry,geometry_signature(),geometry.revision,
                     geometry_selected,1.f,1.f,1.f,l->tint[3]);
  if(button((Rectangle){x,740,134,30},"Opacity -",0))
   (void)studio_geometry_tint_set(&geometry,geometry_signature(),geometry.revision,
                     geometry_selected,l->tint[0],l->tint[1],l->tint[2],
                     fmaxf(0.f,l->tint[3]-.1f));
  if(button((Rectangle){x+144,740,136,30},"Opacity +",0))
   (void)studio_geometry_tint_set(&geometry,geometry_signature(),geometry.revision,
                     geometry_selected,l->tint[0],l->tint[1],l->tint[2],
                     fminf(1.f,l->tint[3]+.1f));
  label("UV checker preview / private source",x,798,11,MUTED);
 }else{
  label("Custom Rig + XPBD",x,479,14,ACCENT);
  label(TextFormat("Pinned vertex: %d",l->pin_vertex),x,522,13,TEXT);
  if(button((Rectangle){x,553,134,32},"Pin selected",0))
   (void)studio_geometry_pin_set(&geometry,geometry_signature(),geometry.revision,
                       geometry_selected,geometry_vertex);
  if(button((Rectangle){x+144,553,136,32},"Unpin",0))
   (void)studio_geometry_pin_set(&geometry,geometry_signature(),geometry.revision,
                       geometry_selected,-1);
  int chosen=geometry_vertex;
  if(chosen>=0&&chosen<l->vertex_count){
   label(TextFormat("Vertex %d: weight %.2f parent %d",chosen,
       l->weights[chosen],l->parent[chosen]),x,613,12,TEXT);
   if(button((Rectangle){x,639,88,28},"Fixed",0))
    (void)studio_geometry_weight_set(&geometry,geometry_signature(),
             geometry.revision,geometry_selected,chosen,0.f);
   if(button((Rectangle){x+96,639,88,28},"Soft",0))
    (void)studio_geometry_weight_set(&geometry,geometry_signature(),
             geometry.revision,geometry_selected,chosen,.5f);
   if(button((Rectangle){x+192,639,88,28},"Free",0))
    (void)studio_geometry_weight_set(&geometry,geometry_signature(),
             geometry.revision,geometry_selected,chosen,1.f);
   if(button((Rectangle){x,677,134,28},"Parent to pin",0))
    (void)studio_geometry_parent_set(&geometry,geometry_signature(),
      geometry.revision,geometry_selected,chosen,l->pin_vertex,.85f);
   if(button((Rectangle){x+144,677,136,28},"Clear parent",0))
    (void)studio_geometry_parent_set(&geometry,geometry_signature(),
      geometry.revision,geometry_selected,chosen,-1,0.f);
  }else label("Select a vertex to edit Rig weights.",x,616,11,MUTED);

  if(button((Rectangle){x,716,134,32},"Bind XPBD",0))
   toast(geometry_bind_physics()?"Local mesh XPBD bound.":"Physics bind rejected.");
  if(button((Rectangle){x+144,716,136,32},"Stop XPBD",0))
   geometry_stop_physics();
  label(TextFormat("Bound: %s | Revision %llu",
        geometry_xpbd?"YES":"NO",(unsigned long long)geometry_bound_revision),
        x,766,12,TEXT);
  label(TextFormat("GPU accepted frames: %d",geometry_accepted_frames),
        x,800,12,ACCENT);
  label("Custom solver; G5 writer still HOLD.",x,840,11,MUTED);
 }
}
