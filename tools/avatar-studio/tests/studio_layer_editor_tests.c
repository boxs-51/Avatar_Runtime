#include "../studio_layer_editor.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"LAYER_EDITOR_RED line=%d\n",__LINE__);return 1;}}while(0)
int main(void){
 StudioLayerEditor e={0};const unsigned short triangles[]={0,1,2,2,3,0};
 const StudioLayerVertex screen[]={{10,10},{110,10},{110,110},{10,110}};
 studio_layer_editor_reset(&e,23,2);
 CHECK(e.generation==23&&e.drawable==2&&e.vertex==-1&&e.triangle==-1);
 CHECK(studio_layer_editor_pick(screen,4,11,12,18)==0);
 CHECK(studio_layer_editor_pick(screen,4,50,50,10)==-1);
 CHECK(studio_layer_editor_select_vertex(&e,23,2,3,4));
 CHECK(e.vertex==3&&e.triangle==-1);
 CHECK(!studio_layer_editor_select_vertex(&e,22,2,1,4));
 CHECK(!studio_layer_editor_select_vertex(&e,23,3,1,4));
 CHECK(!studio_layer_editor_select_vertex(&e,23,2,4,4));
 CHECK(e.vertex==3);
 CHECK(studio_layer_editor_select_triangle(&e,23,2,1,triangles,6,4));
 CHECK(e.vertex==-1&&e.triangle==1);
 CHECK(!studio_layer_editor_select_triangle(&e,23,2,2,triangles,6,4));
 CHECK(e.triangle==1);
 const unsigned short bad[]={0,0,2};
 CHECK(!studio_layer_editor_select_triangle(&e,23,2,0,bad,3,4));
 studio_layer_editor_reset(&e,24,1);
 CHECK(e.generation==24&&e.drawable==1&&e.vertex==-1&&e.triangle==-1);
 puts("LAYER_EDITOR_UNIT_PASS cursor=1 bounds=1 epoch=1 triangle=1");
 return 0;
}
