#include "studio_layer_editor.h"
#include <math.h>
#include <string.h>

void studio_layer_editor_reset(StudioLayerEditor *editor,
                               uint64_t generation,int drawable){
 if(!editor)return;
 memset(editor,0,sizeof *editor);
 editor->generation=generation;
 editor->drawable=drawable;
 editor->vertex=-1;
 editor->triangle=-1;
 editor->wireframe=1;
}
int studio_layer_editor_pick(const StudioLayerVertex *screen,int count,
                             float x,float y,float radius){
 if(!screen||count<1||count>2048||!isfinite(x)||!isfinite(y)||
    !isfinite(radius)||radius<=0||radius>200)return -1;
 float best=radius*radius;int selected=-1;
 for(int i=0;i<count;i++){
  if(!isfinite(screen[i].x)||!isfinite(screen[i].y))continue;
  float dx=screen[i].x-x,dy=screen[i].y-y;
  float square=dx*dx+dy*dy;
  if(isfinite(square)&&square<best){best=square;selected=i;}
 }
 return selected;
}
int studio_layer_editor_select_vertex(StudioLayerEditor *editor,
    uint64_t generation,int drawable,int vertex,int vertex_count){
 if(!editor||!generation||editor->generation!=generation||
    editor->drawable!=drawable||vertex_count<1||vertex_count>2048||
    vertex<0||vertex>=vertex_count)return 0;
 editor->vertex=vertex;editor->triangle=-1;
 return 1;
}
int studio_layer_editor_select_triangle(StudioLayerEditor *editor,
    uint64_t generation,int drawable,int triangle,
    const unsigned short *indices,int index_count,int vertex_count){
 if(!editor||!generation||editor->generation!=generation||
    editor->drawable!=drawable||!indices||index_count<3||
    index_count>65535||index_count%3||vertex_count<3||
    vertex_count>2048||triangle<0||triangle>=index_count/3)return 0;
 int at=triangle*3;
 int a=indices[at],b=indices[at+1],c=indices[at+2];
 if(a>=vertex_count||b>=vertex_count||c>=vertex_count||
    a==b||b==c||a==c)return 0;
 editor->triangle=triangle;editor->vertex=-1;
 return 1;
}
