#include "../studio_layer_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define CHECK(v) do {if(!(v)){fprintf(stderr,"LAYER_TEST_FAIL line=%d\n",__LINE__);return 1;}}while(0)
int main(void){
 const StudioLayerVertex quad[]={{-.3f,.9f},{.3f,.9f},{-.3f,-.7f},{.3f,-.7f}};
 StudioLayerInput in={"HairFront",quad,4,-1,1,2};
 StudioLayerSuggestion result={0};
 CHECK(studio_layer_analyze(&in,&result));
 CHECK(result.kind==STUDIO_LAYER_HAIR);
 CHECK(result.confidence>=80&&result.anchor_count>=2);
 CHECK(result.anchors[0]==0&&result.anchors[1]==1);
 in.drawable_id="Skirt_Back";
 CHECK(studio_layer_analyze(&in,&result));
 CHECK(result.kind==STUDIO_LAYER_SKIRT&&result.anchor_count>=2);
 in.drawable_id="RibbonTail";
 CHECK(studio_layer_analyze(&in,&result));
 CHECK(result.kind==STUDIO_LAYER_STRAP&&result.anchor_count>=1);
 in.drawable_id="ArtMesh_007";
 CHECK(studio_layer_analyze(&in,&result));
 CHECK(result.kind==STUDIO_LAYER_UNKNOWN&&result.confidence<=50&&result.anchor_count==0);
 CHECK(result.region!=0&&result.shape!=0);
 CHECK(studio_layer_region_name(result.region)[0]!=0);
 in.drawable_id="Chair";
 CHECK(studio_layer_analyze(&in,&result));
 CHECK(result.kind==STUDIO_LAYER_UNKNOWN);
 in.drawable_id="HairSkirt";
 CHECK(studio_layer_analyze(&in,&result));
 CHECK(result.kind==STUDIO_LAYER_UNKNOWN);
 in.drawable_id="Hair";
 in.vertices=NULL;
 CHECK(!studio_layer_analyze(&in,&result));
 in.vertices=quad;in.vertex_count=4;in.object_height=0;
 CHECK(!studio_layer_analyze(&in,&result));
 const unsigned short twins[]={0,1,2,3,4,5};
 StudioLayerTopology topology={0};
 CHECK(studio_layer_components(twins,6,7,&topology));
 CHECK(topology.islands==2&&topology.largest==3);
 CHECK(topology.isolated==1&&topology.used==6&&topology.triangles==2);
 const unsigned short connected[]={0,1,2,2,1,3};
 CHECK(studio_layer_components(connected,6,4,&topology));
 CHECK(topology.islands==1&&topology.isolated==0&&topology.largest==4);
 const unsigned short invalid[]={0,1,9};
 CHECK(!studio_layer_components(invalid,3,4,&topology));
 CHECK(!studio_layer_components(connected,5,4,&topology));
 const StudioLayerVertex points[]={{0,0},{0,2},{2,0},{4,0},{4,3},{6,0},{9,0}};
 StudioLayerComponent segment={0};
 CHECK(studio_layer_component_suggest(twins,6,points,7,0,&segment));
 CHECK(segment.vertices==3&&segment.anchor==1&&segment.root==0);
 CHECK(studio_layer_component_suggest(twins,6,points,7,1,&segment));
 CHECK(segment.vertices==3&&segment.anchor==4&&segment.root==3);
 CHECK(!studio_layer_component_suggest(twins,6,points,7,2,&segment));
 CHECK(!studio_layer_component_suggest(invalid,3,points,4,0,&segment));
 StudioLayerRelation between={0};
 CHECK(studio_layer_component_relation(twins,6,points,7,0,1,&between));
 CHECK(between.vertex_a==2&&between.vertex_b==3);
 CHECK(fabsf(between.distance-2.f)<.00001f);
 CHECK(!studio_layer_component_relation(twins,6,points,7,0,0,&between));
 CHECK(!studio_layer_component_relation(invalid,3,points,4,0,1,&between));
 puts("MESH_RELATION_PASS closest_pair=validated cross_island_not_auto_connected");
 puts("LAYER_COMPONENTS_PASS islands=2 orphan=1 invalid=reject");
 puts("LAYER_CLASSIFICATION_PASS hair=1 skirt=1 strap=1 ambiguous=unknown conservative=1");
 return 0;
}
