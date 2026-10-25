#pragma once
#include <stdint.h>
#include <stdio.h>
#define STUDIO_GEOM_LAYERS 8
#define STUDIO_GEOM_VERTICES 128
#define STUDIO_GEOM_TRIANGLES 256
#define STUDIO_GEOM_HISTORY 6
#define STUDIO_GEOM_LINKS 16

typedef struct {float x,y;} StudioGeometryPoint;
typedef struct {
 int id,vertex_count,tri_count;
 StudioGeometryPoint vertices[STUDIO_GEOM_VERTICES];
 StudioGeometryPoint uv[STUDIO_GEOM_VERTICES];
 float tint[4];
 int pin_vertex;
 float weights[STUDIO_GEOM_VERTICES];
 uint32_t next_vertex_id; /* monotonic within one layer lifetime */
 uint32_t vertex_ids[STUDIO_GEOM_VERTICES];
 int parent[STUDIO_GEOM_VERTICES];
 float parent_stiffness[STUDIO_GEOM_VERTICES];
 unsigned short triangles[STUDIO_GEOM_TRIANGLES][3];
} StudioGeometryLayer;
typedef struct {
 int parent_layer_id,child_layer_id;
 uint32_t parent_vertex_id,child_vertex_id;
 float stiffness;
} StudioGeometryCrossLink;
typedef struct {
 int count;
 StudioGeometryLayer layers[STUDIO_GEOM_LAYERS];
 int link_count;
 StudioGeometryCrossLink links[STUDIO_GEOM_LINKS];
} StudioGeometrySnapshot;
typedef struct {
 uint64_t signature,revision;
 StudioGeometrySnapshot active;
 StudioGeometrySnapshot undo[STUDIO_GEOM_HISTORY],redo[STUDIO_GEOM_HISTORY];
 int undo_count,redo_count;
} StudioGeometryDocument;
#ifdef __cplusplus
extern "C" {
#endif
void studio_geometry_init(StudioGeometryDocument*,uint64_t);
int studio_geometry_verify(const StudioGeometryDocument*,uint64_t);
int studio_geometry_index_by_id(const StudioGeometryDocument*,int);
int studio_geometry_vertex_index_by_id(const StudioGeometryDocument*,int,uint32_t);
int studio_geometry_link_layers(StudioGeometryDocument*,uint64_t,uint64_t,
                               int,uint32_t,int,uint32_t,float);
int studio_geometry_unlink_layers(StudioGeometryDocument*,uint64_t,uint64_t,
                                 int,uint32_t,int,uint32_t);
int studio_geometry_create(StudioGeometryDocument*,uint64_t,uint64_t,int);
int studio_geometry_remove(StudioGeometryDocument*,uint64_t,uint64_t,int);
int studio_geometry_duplicate(StudioGeometryDocument*,uint64_t,uint64_t,int,int);
int studio_geometry_reorder(StudioGeometryDocument*,uint64_t,uint64_t,int,int);
int studio_geometry_import(StudioGeometryDocument*,uint64_t,uint64_t,int,
    const StudioGeometryPoint*,int,const unsigned short*,int);
int studio_geometry_import_with_uv(StudioGeometryDocument*,uint64_t,uint64_t,int,
    const StudioGeometryPoint*,const StudioGeometryPoint*,int,
    const unsigned short*,int);
int studio_geometry_uv_set(StudioGeometryDocument*,uint64_t,uint64_t,int,int,float,float);
int studio_geometry_tint_set(StudioGeometryDocument*,uint64_t,uint64_t,int,float,float,float,float);
int studio_geometry_pin_set(StudioGeometryDocument*,uint64_t,uint64_t,int,int);
int studio_geometry_weight_set(StudioGeometryDocument*,uint64_t,uint64_t,int,int,float);
int studio_geometry_parent_set(StudioGeometryDocument*,uint64_t,uint64_t,int,int,int,float);
int studio_geometry_vertex_add(StudioGeometryDocument*,uint64_t,uint64_t,int,float,float);
int studio_geometry_vertex_move(StudioGeometryDocument*,uint64_t,uint64_t,int,int,float,float);
int studio_geometry_vertex_remove(StudioGeometryDocument*,uint64_t,uint64_t,int,int);
int studio_geometry_triangle_add(StudioGeometryDocument*,uint64_t,uint64_t,int,int,int,int);
int studio_geometry_triangle_remove(StudioGeometryDocument*,uint64_t,uint64_t,int,int);
int studio_geometry_undo(StudioGeometryDocument*,uint64_t,uint64_t);
int studio_geometry_redo(StudioGeometryDocument*,uint64_t,uint64_t);
int studio_geometry_save(const StudioGeometryDocument*,uint64_t,FILE*);
int studio_geometry_load(StudioGeometryDocument*,uint64_t,FILE*);
#ifdef __cplusplus
}
#endif
