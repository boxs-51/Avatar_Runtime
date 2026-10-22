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
 puts("LAYER_CLASSIFICATION_PASS hair=1 skirt=1 strap=1 ambiguous=unknown conservative=1");
 return 0;
}
