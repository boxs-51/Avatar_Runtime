#include "studio_geometry_xpbd.h"
#include "native_mesh_xpbd.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
namespace xp=ars::experimental::lps;
struct Point{float x,y;};
struct Mesh{
 uint32_t part_index=0;
 std::vector<Point> vertices,uv;
 std::vector<std::array<uint32_t,3>> triangles;
};
struct Scene{std::vector<Mesh> draws;};
struct GeometrySolver {
 xp::Session<Scene> solver;
 std::unique_ptr<xp::Session<Scene>::Candidate> pending;
 Scene rest;
 uint64_t revision=0,identity=1;
 int accepted_frames=0;
 std::vector<StudioGeometryPoint> presented;
 std::vector<std::vector<StudioGeometryPoint>> presented_by_scene;
 std::vector<int> source_layers;
 int selected_scene=0;
};
static bool scene_from(const StudioGeometryLayer*l,Scene&scene){
 if(!l||l->vertex_count<3||l->vertex_count>STUDIO_GEOM_VERTICES||
    l->tri_count<1||l->tri_count>STUDIO_GEOM_TRIANGLES)return false;
 Mesh mesh;
 for(int i=0;i<l->vertex_count;i++){
  auto p=l->vertices[i],uv=l->uv[i];
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(uv.x)||
     !std::isfinite(uv.y))return false;
  mesh.vertices.push_back({p.x,p.y});mesh.uv.push_back({uv.x,uv.y});
 }
 for(int i=0;i<l->tri_count;i++){
  auto *t=l->triangles[i];
  if(t[0]>=l->vertex_count||t[1]>=l->vertex_count||
     t[2]>=l->vertex_count)return false;
  mesh.triangles.push_back({t[0],t[1],t[2]});
 }
 scene.draws.push_back(std::move(mesh));return true;
}
extern "C" void *studio_geometry_xpbd_bind(const StudioGeometryLayer*l,uint64_t rev){
 if(!rev||!l)return nullptr;
 auto r=std::make_unique<GeometrySolver>();
 if(!scene_from(l,r->rest))return nullptr;
 int pin=l->pin_vertex>=0?l->pin_vertex:0;
 if(pin>=l->vertex_count)return nullptr;
 xp::Options o;
 o.dt=1.0/120.0;o.gravity_y=-80;o.wind_x=12;o.edge_compliance=.00001;
 o.pose_compliance=.12;o.damping=.985;o.iterations=12;o.substeps=3;
 o.dynamic_parts.push_back({0,(uint32_t)pin,1.0});
 o.anchors.push_back({0,(uint32_t)pin});
 for(int v=0;v<l->vertex_count;v++){
  const float weight=l->weights[v];
  const int parent=l->parent[v];
  const float stiffness=l->parent_stiffness[v];
  if(!std::isfinite(weight)||weight<0||weight>1||
     !std::isfinite(stiffness)||stiffness<0||stiffness>1||
     parent< -1||parent>=l->vertex_count||parent==v)return nullptr;
  if(weight!=1.f)
    o.influences.push_back({{0,(uint32_t)v},weight});
  if(parent>=0)
    o.parent_links.push_back({{0,(uint32_t)parent},{0,(uint32_t)v},
                              double(1-stiffness)*.0001});
 }
 if(r->solver.bind(r->rest,o,r->identity)!=xp::Status::ok)return nullptr;
 r->revision=rev;
 r->presented.resize((size_t)l->vertex_count);
 r->presented_by_scene.resize(1);
 r->presented_by_scene[0].resize((size_t)l->vertex_count);
 r->source_layers.push_back(-1);
 return r.release();
}

extern "C" void *studio_geometry_xpbd_bind_linked(
    const StudioGeometryDocument*d,uint64_t rev,int selected){
 if(!d||!rev||d->revision!=rev||d->active.count<2||
    d->active.count>STUDIO_GEOM_LAYERS||d->active.link_count<1||
    selected<0||selected>=d->active.count||
    !studio_geometry_verify(d,d->signature))return nullptr;
 auto r=std::make_unique<GeometrySolver>();
 xp::Options o;
 o.dt=1.0/120.0;o.gravity_y=-80;o.wind_x=12;
 o.edge_compliance=.00001;o.pose_compliance=.12;
 o.damping=.985;o.iterations=12;o.substeps=3;
 for(int i=0;i<d->active.count;i++){
  const StudioGeometryLayer*l=&d->active.layers[i];
  if(!scene_from(l,r->rest))return nullptr;
  r->rest.draws.back().part_index=(uint32_t)i;
  int pin=l->pin_vertex>=0?l->pin_vertex:0;
  if(pin>=l->vertex_count)return nullptr;
  o.dynamic_parts.push_back({(uint32_t)i,(uint32_t)pin,1.0});
  o.anchors.push_back({(uint32_t)i,(uint32_t)pin});
  for(int v=0;v<l->vertex_count;v++){
   float weight=l->weights[v];
   int parent=l->parent[v];
   if(weight!=1.f)
    o.influences.push_back({{(uint32_t)i,(uint32_t)v},weight});
   if(parent>=0)
    o.parent_links.push_back({
      {(uint32_t)i,(uint32_t)parent},{(uint32_t)i,(uint32_t)v},
      double(1-l->parent_stiffness[v])*.0001});
  }
  r->source_layers.push_back(i);
  r->presented_by_scene.emplace_back((size_t)l->vertex_count);
 }
 for(int i=0;i<d->active.link_count;i++){
  const StudioGeometryCrossLink*link=&d->active.links[i];
  int a=studio_geometry_index_by_id(d,link->parent_layer_id);
  int b=studio_geometry_index_by_id(d,link->child_layer_id);
  int av=studio_geometry_vertex_index_by_id(d,link->parent_layer_id,
                                               link->parent_vertex_id);
  int bv=studio_geometry_vertex_index_by_id(d,link->child_layer_id,
                                               link->child_vertex_id);
  if(a<0||b<0||a==b||av<0||bv<0)return nullptr;
  o.parent_links.push_back({{(uint32_t)a,(uint32_t)av},
                             {(uint32_t)b,(uint32_t)bv},
                             double(1-link->stiffness)*.0001});
 }
 if(r->solver.bind(r->rest,o,r->identity)!=xp::Status::ok)return nullptr;
 r->revision=rev;r->selected_scene=selected;
 r->presented.resize((size_t)d->active.layers[selected].vertex_count);
 return r.release();
}
extern "C" int studio_geometry_xpbd_prepare(void*ptr,
                        const StudioGeometryLayer*l,uint64_t rev){
 auto*r=static_cast<GeometrySolver*>(ptr);
 if(!r||r->pending||!l||r->revision!=rev||
    l->vertex_count!=(int)r->presented.size()||
    r->selected_scene<0||
    r->selected_scene>=(int)r->presented_by_scene.size())return 0;
 auto candidate=std::make_unique<xp::Session<Scene>::Candidate>();
 if(r->solver.prepare(r->rest,r->identity,*candidate,0,0)!=xp::Status::ok)
  return 0;
 if(candidate->scene.draws.size()!=r->presented_by_scene.size())return 0;
 for(size_t draw=0;draw<r->presented_by_scene.size();draw++){
  const auto&vertices=candidate->scene.draws[draw].vertices;
  auto&result=r->presented_by_scene[draw];
  if(vertices.size()!=result.size())return 0;
  for(size_t i=0;i<result.size();i++){
   auto v=vertices[i];
   if(!std::isfinite(v.x)||!std::isfinite(v.y))return 0;
   result[i]={v.x,v.y};
  }
 }
 r->presented=r->presented_by_scene[(size_t)r->selected_scene];
 r->pending=std::move(candidate);
 return 1;
}
extern "C" const StudioGeometryPoint *studio_geometry_xpbd_positions(void*ptr){
 auto*r=static_cast<GeometrySolver*>(ptr);
 return r&&!r->presented.empty()?r->presented.data():nullptr;
}
extern "C" const StudioGeometryPoint *studio_geometry_xpbd_positions_for_layer(
                         void*ptr,int layer_index){
 auto*r=static_cast<GeometrySolver*>(ptr);
 if(!r)return nullptr;
 for(size_t i=0;i<r->source_layers.size();i++)
  if(r->source_layers[i]==layer_index&&
     !r->presented_by_scene[i].empty())
   return r->presented_by_scene[i].data();
 return nullptr;
}
extern "C" int studio_geometry_xpbd_accept(void*ptr){
 auto*r=static_cast<GeometrySolver*>(ptr);
 if(!r||!r->pending)return 0;
 auto result=r->solver.accept(std::move(*r->pending),r->identity);
 r->pending.reset();
 if(result!=xp::Status::ok)return 0;
 ++r->accepted_frames;return 1;
}
extern "C" int studio_geometry_xpbd_frames(void*ptr){
 auto*r=static_cast<GeometrySolver*>(ptr);
 return r?r->accepted_frames:0;
}
extern "C" void studio_geometry_xpbd_free(void*ptr){
 delete static_cast<GeometrySolver*>(ptr);
}
