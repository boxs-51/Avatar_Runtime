#pragma once
#include <stdint.h>
#include "studio_layer_analysis.h"

typedef struct StudioLayerEditor {
 uint64_t generation;
 int drawable,vertex,triangle,wireframe,isolate;
} StudioLayerEditor;
void studio_layer_editor_reset(StudioLayerEditor*,uint64_t generation,int drawable);
int studio_layer_editor_pick(const StudioLayerVertex *screen,int count,
                             float x,float y,float radius);
int studio_layer_editor_select_vertex(StudioLayerEditor*,uint64_t generation,
                                      int drawable,int vertex,int vertex_count);
int studio_layer_editor_select_triangle(StudioLayerEditor*,uint64_t generation,
                       int drawable,int triangle,const unsigned short *indices,
                       int index_count,int vertex_count);
