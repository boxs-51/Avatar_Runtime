#pragma once
#ifdef __cplusplus
extern "C" {
#endif
typedef struct StudioLayerVertex {float x,y;} StudioLayerVertex;
typedef enum StudioLayerKind {
 STUDIO_LAYER_UNKNOWN=0, STUDIO_LAYER_HAIR=1,
 STUDIO_LAYER_SKIRT=2, STUDIO_LAYER_STRAP=3
} StudioLayerKind;
typedef struct StudioLayerInput {
 const char *drawable_id;
 const StudioLayerVertex *vertices;
 int vertex_count;
 float object_min_y,object_max_y,object_height;
} StudioLayerInput;
typedef struct StudioLayerSuggestion {
 StudioLayerKind kind;
 int confidence;
 int anchor_count;
 int anchors[4];
 int has_vertical_span;
 /* Geometry-only classification; not a semantic hair/skirt label. */
 int region; /* 0 unknown, 1 upper, 2 middle, 3 lower, 4 spanning */
 int shape;  /* 0 compact, 1 thin, 2 wide, 3 tall */

} StudioLayerSuggestion;
int studio_layer_analyze(const StudioLayerInput *,StudioLayerSuggestion *);
typedef struct StudioLayerTopology {
 int islands,used,isolated,largest,triangles;
} StudioLayerTopology;
typedef struct StudioLayerRelation {
 int vertex_a,vertex_b;
 float distance;
} StudioLayerRelation;
int studio_layer_component_relation(const unsigned short *indices,int index_count,
   const StudioLayerVertex *vertices,int vertex_count,int first,int second,
   StudioLayerRelation *out);
typedef struct StudioLayerComponent {
 int root,vertices,anchor;
} StudioLayerComponent;
int studio_layer_component_suggest(const unsigned short *indices,int index_count,
     const StudioLayerVertex *vertices,int vertex_count,int component,
     StudioLayerComponent *out);
int studio_layer_components(const unsigned short *indices,int index_count,
                            int vertex_count,StudioLayerTopology *result);
const char *studio_layer_kind_name(StudioLayerKind kind);
const char *studio_layer_region_name(int region);
const char *studio_layer_shape_name(int shape);
#ifdef __cplusplus
}
#endif
