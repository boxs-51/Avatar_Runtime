static void geometry_editor_ui(float x){
 if(button((Rectangle){x,433,52,28},"Mesh",geometry_property_tab==0))
  geometry_property_tab=0;
 if(button((Rectangle){x+57,433,52,28},"UV",geometry_property_tab==1))
  geometry_property_tab=1;
 if(button((Rectangle){x+114,433,52,28},"Rig",geometry_property_tab==2))
  geometry_property_tab=2;
 if(button((Rectangle){x+171,433,52,28},"Links",geometry_property_tab==4))
  geometry_property_tab=4;
 if(button((Rectangle){x+228,433,52,28},"Build",geometry_property_tab==3))
  geometry_property_tab=3;
 if(geometry_property_tab!=0){
  geometry_properties_ui(x);
  return;
 }
 if(button((Rectangle){x,462,134,30},"Create layer",0)){
  int id=1,used=1;
  while(id<=9999&&used){
   used=0;
   for(int i=0;i<geometry.active.count;i++)
    if(geometry.active.layers[i].id==id)used=1;
   if(used)id++;
  }
  if(studio_geometry_create(&geometry,geometry_signature(),geometry.revision,id)){
   geometry_selected=geometry.active.count-1;geometry_vertex=-1;
   toast("Custom layer created.");
  }else toast("Layer creation rejected.");
 }
 if(button((Rectangle){x+144,462,136,30},"Delete layer",0)){
  if(studio_geometry_remove(&geometry,geometry_signature(),geometry.revision,geometry_selected)){
   geometry_selected=-1;geometry_vertex=-1;toast("Layer removed; Undo available.");
  }else toast("Select a custom layer first.");
 }
 if(button((Rectangle){x,500,134,28},"Previous custom",0)&&geometry.active.count>0){
  geometry_selected=geometry_selected<0?0:
    (geometry_selected+geometry.active.count-1)%geometry.active.count;
  geometry_vertex=-1;
 }
 if(button((Rectangle){x+144,500,136,28},"Next custom",0)&&geometry.active.count>0){
  geometry_selected=(geometry_selected+1)%geometry.active.count;
  geometry_vertex=-1;
 }
 if(geometry_selected>=0&&geometry_selected<geometry.active.count){
  StudioGeometryLayer*l=&geometry.active.layers[geometry_selected];
  label(TextFormat("ID %d | V %d | T %d",l->id,l->vertex_count,l->tri_count),
        x,533,13,TEXT);
  if(button((Rectangle){x,558,134,28},
            geometry_add_mode?"Click add ON":"Click add OFF",geometry_add_mode))
    geometry_add_mode=!geometry_add_mode;
  if(button((Rectangle){x+144,558,136,28},"Delete point",0)){
   if(studio_geometry_vertex_remove(&geometry,geometry_signature(),geometry.revision,
                                    geometry_selected,geometry_vertex)){
    geometry_vertex=-1;toast("Point removed.");
   }else toast("Point missing or used by triangle.");
  }
  if(button((Rectangle){x,594,134,28},"Add last triangle",0)){
   int n=l->vertex_count;
   if(n>=3&&studio_geometry_triangle_add(&geometry,geometry_signature(),geometry.revision,
                                         geometry_selected,n-3,n-2,n-1))
     toast("Triangle added.");
   else toast("Three non-collinear points required.");
  }
  if(button((Rectangle){x+144,594,136,28},"Delete last tri",0)){
   if(studio_geometry_triangle_remove(&geometry,geometry_signature(),geometry.revision,
                                      geometry_selected,l->tri_count-1))
     toast("Triangle removed.");
  }
  if(geometry_vertex>=0&&geometry_vertex<l->vertex_count){
   StudioGeometryPoint point=l->vertices[geometry_vertex];
   label(TextFormat("Point %d (%.2f, %.2f)",geometry_vertex,point.x,point.y),
         x,627,12,TEXT);
   if(button((Rectangle){x,649,64,27},"X -",0))
    (void)studio_geometry_vertex_move(&geometry,geometry_signature(),geometry.revision,
                 geometry_selected,geometry_vertex,point.x-.1f,point.y);
   if(button((Rectangle){x+72,649,64,27},"X +",0))
    (void)studio_geometry_vertex_move(&geometry,geometry_signature(),geometry.revision,
                 geometry_selected,geometry_vertex,point.x+.1f,point.y);
   if(button((Rectangle){x+144,649,64,27},"Y -",0))
    (void)studio_geometry_vertex_move(&geometry,geometry_signature(),geometry.revision,
                 geometry_selected,geometry_vertex,point.x,point.y-.1f);
   if(button((Rectangle){x+216,649,64,27},"Y +",0))
    (void)studio_geometry_vertex_move(&geometry,geometry_signature(),geometry.revision,
                 geometry_selected,geometry_vertex,point.x,point.y+.1f);
  }
 }
 if(button((Rectangle){x,694,134,29},"Undo mesh",0)){
  if(studio_geometry_undo(&geometry,geometry_signature(),geometry.revision)){
   geometry_selected=-1;geometry_vertex=-1;
  }else toast("No mesh undo available.");
 }
 if(button((Rectangle){x+144,694,136,29},"Redo mesh",0)){
  if(studio_geometry_redo(&geometry,geometry_signature(),geometry.revision)){
   geometry_selected=-1;geometry_vertex=-1;
  }else toast("No mesh redo available.");
 }
 if(button((Rectangle){x,734,134,31},"Save layers",0))
  toast(geometry_project_io(1)?"Private layers saved.":"Layer save rejected.");
 if(button((Rectangle){x+144,734,136,31},"Load layers",0))
  toast(geometry_project_io(0)?"Private layers loaded.":"Layer load rejected.");
 if(button((Rectangle){x,782,134,30},"Import drawable",0)){
  int id=1,used=1;
  while(id<=9999&&used){
   used=0;
   for(int i=0;i<geometry.active.count;i++)
    if(geometry.active.layers[i].id==id)used=1;
   if(used)id++;
  }
  toast(geometry_import_drawable(s.xpbd_first,id)?"Mesh copied to editor.":"Mesh too large or invalid.");
 }
 if(button((Rectangle){x+144,782,136,30},"Custom only",0))
  s.opt.onlyDrawable=s.opt.onlyDrawable==-1?
    csmGetDrawableCount(s.core.model):-1;
 if(button((Rectangle){x,819,88,28},"Duplicate",0)){
  int id=1,used=1;
  while(id<=9999&&used){
   used=0;
   for(int i=0;i<geometry.active.count;i++)
    if(geometry.active.layers[i].id==id)used=1;
   if(used)id++;
  }
  if(studio_geometry_duplicate(&geometry,geometry_signature(),geometry.revision,
                               geometry_selected,id)){
   geometry_selected=geometry.active.count-1;geometry_vertex=-1;
  }else toast("Cannot duplicate this layer.");
 }
 if(button((Rectangle){x+96,819,88,28},"Move up",0)){
  if(studio_geometry_reorder(&geometry,geometry_signature(),geometry.revision,
                            geometry_selected,geometry_selected-1))
   geometry_selected--;
 }
 if(button((Rectangle){x+192,819,88,28},"Move down",0)){
  if(studio_geometry_reorder(&geometry,geometry_signature(),geometry.revision,
                            geometry_selected,geometry_selected+1))
   geometry_selected++;
 }
 label("Native source is private; MOC3 unchanged",x,857,11,MUTED);
}
