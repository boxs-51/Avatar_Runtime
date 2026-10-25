#include "../studio_geometry_document.h"
#include "../studio_geometry_runtime.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define REQ(x) do{if(!(x)){fprintf(stderr,"GEOMETRY_RED %d\n",__LINE__);return 1;}}while(0)
static StudioGeometryDocument d,restored;
int main(void){
 studio_geometry_init(&d,1423);
 REQ(studio_geometry_create(&d,1423,0,1));
 REQ(d.active.count==1&&d.revision==1&&d.active.layers[0].id==1);
 REQ(studio_geometry_vertex_add(&d,1423,1,0,0,0));
 REQ(studio_geometry_vertex_add(&d,1423,2,0,2,0));
 REQ(studio_geometry_vertex_add(&d,1423,3,0,0,2));
 REQ(studio_geometry_triangle_add(&d,1423,4,0,0,1,2));
 REQ(d.active.layers[0].tri_count==1&&d.revision==5);
 REQ(!studio_geometry_vertex_remove(&d,1423,5,0,1)); /* referenced */
 REQ(!studio_geometry_triangle_add(&d,1423,5,0,0,0,1));
 REQ(!studio_geometry_vertex_move(&d,1423,4,0,0,1,1)); /* stale */
 REQ(studio_geometry_vertex_move(&d,1423,5,0,0,0.5f,0.5f));
 REQ(d.active.layers[0].vertices[0].x==0.5f);
 REQ(studio_geometry_undo(&d,1423,6));
 REQ(d.active.layers[0].vertices[0].x==0&&d.revision==7);
 REQ(studio_geometry_redo(&d,1423,7));
 REQ(d.active.layers[0].vertices[0].x==0.5f&&d.revision==8);
 REQ(studio_geometry_triangle_remove(&d,1423,8,0,0));
 REQ(studio_geometry_vertex_remove(&d,1423,9,0,1));
 REQ(d.active.layers[0].vertex_count==2&&d.active.layers[0].tri_count==0);
 REQ(studio_geometry_create(&d,1423,10,2));
 REQ(d.active.count==2&&d.active.layers[1].id==2);
 REQ(studio_geometry_remove(&d,1423,11,0));
 REQ(d.active.count==1&&d.active.layers[0].id==2);
 REQ(studio_geometry_undo(&d,1423,12));
 REQ(d.active.count==2&&d.active.layers[0].id==1);
 FILE *f=tmpfile();REQ(f);
 REQ(studio_geometry_save(&d,1423,f));rewind(f);
 REQ(studio_geometry_load(&restored,1423,f));fclose(f);
 REQ(restored.active.count==2&&restored.active.layers[0].vertex_count==2);
 REQ(restored.active.layers[1].id==2);
 REQ(!studio_geometry_vertex_move(&restored,1,0,0,0,1,1));
 REQ(!studio_geometry_vertex_add(&restored,1423,999,0,0,0));
 REQ(studio_geometry_undo(&restored,1423,restored.revision)==0);
 const StudioGeometryPoint points[]={{0,0},{4,0},{0,4}};
 const unsigned short faces[]={0,1,2};
 REQ(studio_geometry_import(&restored,1423,restored.revision,25,points,3,faces,3));
 REQ(restored.active.count==3&&restored.active.layers[2].tri_count==1);
 REQ(!studio_geometry_import(&restored,1423,restored.revision,26,points,3,faces,2));
 REQ(restored.active.count==3);
 REQ(studio_geometry_duplicate(&restored,1423,restored.revision,2,26));
 REQ(restored.active.count==4&&restored.active.layers[3].id==26);
 REQ(studio_geometry_reorder(&restored,1423,restored.revision,3,0));
 REQ(studio_geometry_index_by_id(&restored,26)==0);
 REQ(studio_geometry_index_by_id(&restored,25)==3);
 REQ(studio_geometry_index_by_id(&restored,999)==-1);

 REQ(restored.active.layers[0].id==26&&restored.active.layers[3].id==25);


 REQ(studio_geometry_uv_set(&restored,1423,restored.revision,0,0,.2f,.8f));
 REQ(restored.active.layers[0].uv[0].x==.2f);
 REQ(studio_geometry_tint_set(&restored,1423,restored.revision,0,.8f,.3f,.4f,.75f));
 REQ(restored.active.layers[0].tint[3]==.75f);
 REQ(studio_geometry_pin_set(&restored,1423,restored.revision,0,0));
 REQ(restored.active.layers[0].pin_vertex==0);
 REQ(!studio_geometry_vertex_remove(&restored,1423,restored.revision,0,0));
 FILE *material=tmpfile();REQ(material);
 REQ(studio_geometry_save(&restored,1423,material));rewind(material);
 StudioGeometryDocument *loaded=calloc(1,sizeof *loaded);REQ(loaded);
 REQ(studio_geometry_load(loaded,1423,material));fclose(material);
 REQ(loaded->active.layers[0].uv[0].x==.2f);
 REQ(loaded->active.layers[0].tint[3]==.75f);
 REQ(loaded->active.layers[0].pin_vertex==0);
 free(loaded);
 FILE*legacy=tmpfile();REQ(legacy);
 REQ(fprintf(legacy,"STUDIO_GEOMETRY_1 1423 5 1\n"
    "L 50 3 1\nV 0 0\nV 2 0\nV 0 2\nT 0 1 2\nEND\n")>0);
 rewind(legacy);
 StudioGeometryDocument*old=calloc(1,sizeof *old);REQ(old);
 REQ(studio_geometry_load(old,1423,legacy));fclose(legacy);
 REQ(old->active.count==1&&old->active.layers[0].id==50);
 REQ(old->active.layers[0].pin_vertex==-1);
 REQ(old->active.layers[0].tint[3]==1.f);
 REQ(old->active.layers[0].uv[0].x==0.f);free(old);
 const StudioGeometryPoint src[]={{0,0},{2,0},{0,2}};
 const StudioGeometryPoint tex[]={{.1f,.2f},{.7f,.3f},{.4f,.9f}};
 const unsigned short tri[]={0,1,2};
 REQ(studio_geometry_import_with_uv(&restored,1423,restored.revision,73,
       src,tex,3,tri,3));
 REQ(restored.active.layers[restored.active.count-1].uv[1].x==.7f);

 REQ(studio_geometry_weight_set(&restored,1423,restored.revision,0,0,.4f));
 REQ(restored.active.layers[0].weights[0]==.4f);
 REQ(studio_geometry_parent_set(&restored,1423,restored.revision,0,1,0,.9f));
 REQ(restored.active.layers[0].parent[1]==0);
 REQ(!studio_geometry_parent_set(&restored,1423,restored.revision,0,0,1,.6f));
 REQ(!studio_geometry_weight_set(&restored,1423,restored.revision,0,1,2.f));
 REQ(!studio_geometry_vertex_remove(&restored,1423,restored.revision,0,0));
 FILE*rig_stream=tmpfile();REQ(rig_stream);
 REQ(studio_geometry_save(&restored,1423,rig_stream));rewind(rig_stream);
 StudioGeometryDocument *rig_loaded=calloc(1,sizeof *rig_loaded);REQ(rig_loaded);
 REQ(studio_geometry_load(rig_loaded,1423,rig_stream));
 REQ(rig_loaded->active.layers[0].weights[0]==.4f);
 REQ(rig_loaded->active.layers[0].parent[1]==0);
 REQ(rig_loaded->active.layers[0].parent_stiffness[1]==.9f);
 fclose(rig_stream);free(rig_loaded);
 FILE*compiled=tmpfile();REQ(compiled);
 REQ(studio_geometry_runtime_compile(&restored,1423,compiled));rewind(compiled);
 StudioGeometryDocument*compiled_model=calloc(1,sizeof *compiled_model);
 REQ(compiled_model);
 REQ(studio_geometry_runtime_load(compiled_model,1423,compiled));
 REQ(compiled_model->active.count==restored.active.count);
 REQ(compiled_model->active.layers[0].parent[1]==0);
 REQ(compiled_model->active.layers[0].weights[0]==.4f);
 REQ(compiled_model->active.layers[0].uv[0].x==.2f);
 REQ(compiled_model->active.layers[0].tint[3]==.75f);
 rewind(compiled);
 REQ(!studio_geometry_runtime_load(compiled_model,999,compiled));
 REQ(compiled_model->active.layers[0].parent[1]==0);
 fseek(compiled,15,SEEK_SET);REQ(fputc(255,compiled)!=EOF);fflush(compiled);
 rewind(compiled);REQ(!studio_geometry_runtime_load(compiled_model,1423,compiled));
 REQ(compiled_model->active.layers[0].parent[1]==0);
 fclose(compiled);free(compiled_model);
 StudioGeometryDocument *stable=calloc(1,sizeof *stable);REQ(stable);
 studio_geometry_init(stable,1423);
 REQ(studio_geometry_create(stable,1423,0,84));
 for(int i=0;i<4;i++)
  REQ(studio_geometry_vertex_add(stable,1423,stable->revision,0,(float)i,(float)i));
 uint32_t old_third=stable->active.layers[0].vertex_ids[2];
 uint32_t old_last=stable->active.layers[0].vertex_ids[3];
 REQ(studio_geometry_vertex_remove(stable,1423,stable->revision,0,1));
 REQ(stable->active.layers[0].vertex_ids[1]==old_third);
 REQ(stable->active.layers[0].vertex_ids[2]==old_last);
 REQ(studio_geometry_vertex_add(stable,1423,stable->revision,0,5.f,5.f));
 REQ(stable->active.layers[0].vertex_ids[3]!=old_third);
 REQ(stable->active.layers[0].vertex_ids[3]!=old_last);
 REQ(studio_geometry_vertex_index_by_id(stable,84,old_third)==1);
 REQ(studio_geometry_vertex_index_by_id(stable,84,old_last)==2);
 FILE *ident=tmpfile();REQ(ident);
 REQ(studio_geometry_save(stable,1423,ident));rewind(ident);
 StudioGeometryDocument*again=calloc(1,sizeof *again);REQ(again);
 REQ(studio_geometry_load(again,1423,ident));
 REQ(studio_geometry_vertex_index_by_id(again,84,old_third)==1);
 REQ(studio_geometry_vertex_index_by_id(again,84,old_last)==2);
 fclose(ident);free(stable);free(again);
 StudioGeometryDocument*coupled=calloc(1,sizeof *coupled);REQ(coupled);
 studio_geometry_init(coupled,1423);
 REQ(studio_geometry_create(coupled,1423,0,91));
 REQ(studio_geometry_create(coupled,1423,1,92));
 for(int li=0;li<2;li++)
  for(int v=0;v<3;v++)
   REQ(studio_geometry_vertex_add(coupled,1423,coupled->revision,
        li,(float)v,(float)(v==2?2:0)));
 uint32_t a=coupled->active.layers[0].vertex_ids[0];
 uint32_t b=coupled->active.layers[1].vertex_ids[1];
 REQ(studio_geometry_link_layers(coupled,1423,coupled->revision,91,a,92,b,.8f));
 REQ(coupled->active.link_count==1);
 REQ(!studio_geometry_vertex_remove(coupled,1423,coupled->revision,1,1));
 REQ(!studio_geometry_remove(coupled,1423,coupled->revision,0));
 REQ(studio_geometry_reorder(coupled,1423,coupled->revision,0,1));
 REQ(coupled->active.links[0].parent_layer_id==91);
 REQ(coupled->active.links[0].child_layer_id==92);
 REQ(!studio_geometry_link_layers(coupled,1423,coupled->revision,91,a,92,b,.8f));
 FILE*linked=tmpfile();REQ(linked);
 REQ(studio_geometry_save(coupled,1423,linked));rewind(linked);
 StudioGeometryDocument *read_linked=calloc(1,sizeof *read_linked);REQ(read_linked);
 REQ(studio_geometry_load(read_linked,1423,linked));
 REQ(read_linked->active.link_count==1);
 REQ(!studio_geometry_link_layers(read_linked,1423,read_linked->revision,
         92,b,91,a,.6f)); /* directed cross-layer cycle */
 fclose(linked);free(read_linked);
 FILE*binary_link=tmpfile();REQ(binary_link);
 REQ(studio_geometry_runtime_compile(coupled,1423,binary_link));rewind(binary_link);
 StudioGeometryDocument*compiled_link=calloc(1,sizeof *compiled_link);REQ(compiled_link);
 REQ(studio_geometry_runtime_load(compiled_link,1423,binary_link));
 REQ(compiled_link->active.link_count==1);
 REQ(compiled_link->active.links[0].parent_layer_id==91);
 REQ(compiled_link->active.links[0].child_layer_id==92);
 fclose(binary_link);free(compiled_link);
 REQ(studio_geometry_unlink_layers(coupled,1423,coupled->revision,91,a,92,b));
 REQ(coupled->active.link_count==0);
 free(coupled);
 printf("GEOMETRY_DOC_PASS create_edit_delete=1 undo_redo=1 save_load=1 cas=1\n");
 return 0;
}
