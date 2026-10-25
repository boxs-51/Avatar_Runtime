#pragma once
#include "studio_geometry_document.h"
#ifdef __cplusplus
extern "C" {
#endif
void *studio_geometry_xpbd_bind(const StudioGeometryLayer*,uint64_t revision);
void *studio_geometry_xpbd_bind_linked(const StudioGeometryDocument*,
                                      uint64_t revision,int selected_layer);
int studio_geometry_xpbd_prepare(void*,const StudioGeometryLayer*,uint64_t revision);
const StudioGeometryPoint *studio_geometry_xpbd_positions(void*);
const StudioGeometryPoint *studio_geometry_xpbd_positions_for_layer(void*,int layer_index);
int studio_geometry_xpbd_accept(void*);
int studio_geometry_xpbd_frames(void*);
void studio_geometry_xpbd_free(void*);
#ifdef __cplusplus
}
#endif
