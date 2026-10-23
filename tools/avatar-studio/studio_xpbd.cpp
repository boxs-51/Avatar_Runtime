#include "studio_xpbd.h"
#include "native_mesh_xpbd.hpp"
#include "studio_motion_filter.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>
namespace xp=ars::experimental::lps;
struct Point {float x,y;};
struct Mesh {uint32_t part_index=0;std::vector<Point> vertices,uv;std::vector<std::array<uint32_t,3>> triangles;};
struct Scene {std::vector<Mesh> draws;};
struct Runtime {
 csmModel *model=nullptr;xp::Session<Scene> solver;xp::MotionFilter display;
 std::unique_ptr<xp::MotionFilter::Candidate> display_pending;
 std::unique_ptr<xp::Session<Scene>::Candidate> pending;
 std::vector<int> selected;
 std::vector<std::vector<csmVector2>> displaced;
 std::vector<const csmVector2*> pointers;
 std::vector<unsigned char> pinned;
 std::vector<xp::MotionXY> last_render_positions,prior_step;
 StudioXpbdMetrics metrics={};float action_fx=0,action_fy=0;
 uint64_t identity=1;
};
static Runtime *active=nullptr;
static bool extract(csmModel *m,const std::vector<int>&selected,Scene &scene){
 if(!m||selected.empty()||selected.size()>2)return false;
 int count=csmGetDrawableCount(m);auto vc=csmGetDrawableVertexCounts(m);
 auto ic=csmGetDrawableIndexCounts(m);auto pos=csmGetDrawableVertexPositions(m);
 auto uv=csmGetDrawableVertexUvs(m);auto idx=csmGetDrawableIndices(m);
 if(!vc||!ic||!pos||!uv||!idx)return false;
 Scene next;size_t total=0;
 for(int d:selected){
  if(d<0||d>=count||vc[d]<3||vc[d]>2048||ic[d]<3||ic[d]%3||!pos[d]||!uv[d]||!idx[d])return false;
  if((total+=vc[d])>2048)return false;
  Mesh a;a.part_index=(uint32_t)d;
  for(int i=0;i<vc[d];++i){
   auto p=pos[d][i],t=uv[d][i];
   if(!std::isfinite(p.X)||!std::isfinite(p.Y)||!std::isfinite(t.X)||!std::isfinite(t.Y))return false;
   a.vertices.push_back({p.X,p.Y});a.uv.push_back({t.X,t.Y});
  }
  for(int i=0;i<ic[d];i+=3){
   unsigned x=idx[d][i],y=idx[d][i+1],z=idx[d][i+2];
   if(x>=(unsigned)vc[d]||y>=(unsigned)vc[d]||z>=(unsigned)vc[d])return false;
   a.triangles.push_back({x,y,z});
  }
  next.draws.push_back(std::move(a));
 }
 scene=std::move(next);return true;
}
extern "C" void *studio_xpbd_bind_rig(csmModel *model,StudioXpbdOptions opt,const StudioRig *rig){
 if(!model||opt.first<0||opt.second< -1||opt.first==opt.second||
 !std::isfinite(opt.gravity)||!std::isfinite(opt.wind)||!std::isfinite(opt.compliance)||
 !std::isfinite(opt.collision)||std::abs(opt.gravity)>1000||std::abs(opt.wind)>1000||
 opt.compliance<0||opt.compliance>.01f||opt.collision<0||opt.collision>1||!std::isfinite(opt.mass)||opt.mass<=0||opt.mass>100||!std::isfinite(opt.damping)||opt.damping<0||opt.damping>1||!std::isfinite(opt.contact_compliance)||opt.contact_compliance<0||opt.contact_compliance>.01f||!std::isfinite(opt.dt)||opt.dt<=0||opt.dt>1.f/30||opt.iterations<1||opt.iterations>24||opt.pin<0||opt.pin>2047||opt.substeps<1||opt.substeps>8||!std::isfinite(opt.pose_compliance)||opt.pose_compliance<0||opt.pose_compliance>1||!std::isfinite(opt.source_motion_transfer)||opt.source_motion_transfer<0||opt.source_motion_transfer>1||!std::isfinite(opt.bending_compliance)||opt.bending_compliance<0||opt.bending_compliance>1||!std::isfinite(opt.surface_thickness)||opt.surface_thickness<0||opt.surface_thickness>10||opt.anchor_count<1||opt.anchor_count>8)return nullptr;
 auto r=std::make_unique<Runtime>();r->model=model;r->selected={opt.first};
 if(opt.second>=0)r->selected.push_back(opt.second);
 std::sort(r->selected.begin(),r->selected.end());
 Scene scene;if(!extract(model,r->selected,scene))return nullptr;
 xp::Options o;o.dt=1.0/120.0;o.gravity_y=opt.gravity;o.wind_x=opt.wind;
 o.edge_compliance=opt.compliance;o.contact_radius=opt.collision;o.contact_compliance=opt.contact_compliance;o.damping=opt.damping;o.dt=opt.dt;o.iterations=opt.iterations;o.substeps=opt.substeps;o.pose_compliance=opt.pose_compliance;o.source_motion_transfer=opt.source_motion_transfer;o.bending_compliance=opt.bending_compliance;o.surface_thickness=opt.surface_thickness;
 for(int i:r->selected){o.dynamic_parts.push_back({(uint32_t)i,(uint32_t)opt.pin,opt.mass});
  if(rig&&rig->drawable==i){for(int j=0;j<rig->anchor_count;++j)o.anchors.push_back({(uint32_t)i,(uint32_t)rig->anchors[j]});}
  else for(int j=0;j<opt.anchor_count;++j)o.anchors.push_back({(uint32_t)i,(uint32_t)(opt.pin+j)});
}
 if(opt.joint&&r->selected.size()==2)o.joints.push_back({{(uint32_t)r->selected[0],0},{(uint32_t)r->selected[1],0},opt.compliance});
 if(rig&&rig->drawable==opt.first){
  for(int j=0;j<rig->beam_count;++j)o.extra_beams.push_back({{(uint32_t)rig->drawable,(uint32_t)rig->beams[j].a},{(uint32_t)rig->drawable,(uint32_t)rig->beams[j].b},opt.compliance});
  for(int j=0;j<rig->parent_count;++j){
   const auto &edge=rig->parents[j];
   if(!std::isfinite(edge.stiffness)||edge.stiffness<0||edge.stiffness>1)return nullptr;
   o.parent_links.push_back({{(uint32_t)rig->drawable,(uint32_t)edge.parent},
      {(uint32_t)rig->drawable,(uint32_t)edge.child},
      double(1-edge.stiffness)*.0001});
  }
  for(int j=0;j<rig->weight_count;++j){
   const auto &weight=rig->weights[j];
   if(!std::isfinite(weight.weight)||weight.weight<0||weight.weight>1)return nullptr;
   o.influences.push_back({{(uint32_t)rig->drawable,(uint32_t)weight.vertex},weight.weight});
  }
  if(rig->cross_count>0){
   if(opt.second<0||rig->drawable!=opt.first)return nullptr;
   for(int j=0;j<rig->cross_count;++j){
    const auto &link=rig->cross_links[j];
    if(link.drawable!=opt.second||link.parent<0||link.parent>=rig->vertex_count||
       link.child<0||!std::isfinite(link.stiffness)||
       link.stiffness<0||link.stiffness>1)return nullptr;
    o.joints.push_back({{(uint32_t)rig->drawable,(uint32_t)link.parent},
       {(uint32_t)link.drawable,(uint32_t)link.child},
       (1.0-double(link.stiffness))*.0001});
   }
  }
 }
 if(r->solver.bind(scene,o,r->identity)!=xp::Status::ok)return nullptr;
 for(const auto &part:scene.draws)for(size_t j=0;j<part.vertices.size();++j){
  bool fixed=false;for(const auto &a:o.anchors)if(a.part==part.part_index&&a.vertex==j)fixed=true;
  for(const auto &d:o.dynamic_parts)if(d.part==part.part_index&&d.pin_vertex==j)fixed=true;
  for(const auto &w:o.influences)
   if(w.at.part==part.part_index&&w.at.vertex==j&&w.weight==0)fixed=true;
  r->pinned.push_back(fixed?1:0);
 }
 r->displaced.resize(csmGetDrawableCount(model));r->pointers.resize(csmGetDrawableCount(model));
 return r.release();
}
extern "C" int studio_xpbd_prepare(void *ptr,csmModel *model){
 Runtime *r=static_cast<Runtime*>(ptr);
 if(!r||active||r->model!=model)return 0;
 Scene scene;if(!extract(model,r->selected,scene))return 0;
 auto candidate=std::make_unique<xp::Session<Scene>::Candidate>();
 if(r->solver.prepare(scene,r->identity,*candidate,r->action_fx,r->action_fy)!=xp::Status::ok)return 0;
 auto original=csmGetDrawableVertexPositions(model);if(!original)return 0;
 for(size_t i=0;i<r->pointers.size();++i)r->pointers[i]=original[i];
 std::vector<xp::MotionXY> desired;
 for(const auto &part:candidate->scene.draws)
  for(const auto &p:part.vertices)desired.push_back({p.x,p.y});
 auto filtered=std::make_unique<xp::MotionFilter::Candidate>();
 if(!r->display.prepare(desired,1./60.,.085,*filtered,&r->pinned))return 0;
 for(size_t i=0;i<desired.size()&&i<r->pinned.size();++i)if(r->pinned[i]){
  const float pin_drift=std::hypot(desired[i].x-filtered->positions[i].x,desired[i].y-filtered->positions[i].y);
  r->metrics.pin_drift=std::max(r->metrics.pin_drift,pin_drift);
 }
 size_t cursor=0;
 for(const auto &part:candidate->scene.draws){
  auto &out=r->displaced[part.part_index];out.clear();out.reserve(part.vertices.size());
  for(auto p:part.vertices){auto v=filtered->positions[cursor++];out.push_back({v.x,v.y});}
  r->pointers[part.part_index]=out.data();
 }
 r->display_pending=std::move(filtered);r->pending=std::move(candidate);active=r;return 1;
}
extern "C" const csmVector2 **studio_xpbd_positions(csmModel *model){
 if(active&&active->model==model&&active->pending)return active->pointers.data();
 return csmGetDrawableVertexPositions(model);
}
extern "C" int studio_xpbd_accept(void *ptr){
 Runtime *r=static_cast<Runtime*>(ptr);
 if(!r||active!=r||!r->pending)return 0;
 active=nullptr;auto result=r->solver.accept(std::move(*r->pending),r->identity);
 r->pending.reset();
 if(result!=xp::Status::ok){r->display_pending.reset();return 0;}
 if(!r->display_pending)return 0;
 // Hold the accepted frame's numeric positions before the candidate is moved
 // into the presentation filter. The old code read a moved-from vector.
 const auto rendered=r->display_pending->positions;
 if(!r->display.accept(std::move(*r->display_pending))){r->display_pending.reset();return 0;}
 float frame_step=0;
 if(r->last_render_positions.size()==rendered.size()){
  for(size_t i=0;i<rendered.size();++i){
   const float dx=rendered[i].x-r->last_render_positions[i].x,dy=rendered[i].y-r->last_render_positions[i].y;
   frame_step=std::max(frame_step,std::hypot(dx,dy));
   r->metrics.max_step=std::max(r->metrics.max_step,std::hypot(dx,dy));
   if(r->prior_step.size()==rendered.size()){
    const float ax=dx-(r->last_render_positions[i].x-r->prior_step[i].x);
    const float ay=dy-(r->last_render_positions[i].y-r->prior_step[i].y);
    r->metrics.max_acceleration=std::max(r->metrics.max_acceleration,std::hypot(ax,ay));
   }
  }
 }
 // Pin drift is checked in prepare against the solver output (prior to filtering).
 r->metrics.current_step=frame_step;
 r->prior_step=r->last_render_positions;r->last_render_positions=rendered;
 ++r->metrics.accepted_frames;
 r->display_pending.reset();return 1;
}
extern "C" void studio_xpbd_disable(void *ptr){
 Runtime *r=static_cast<Runtime*>(ptr);
 if(active==r)active=nullptr;delete r;
}




extern "C" void *studio_xpbd_bind(csmModel *m,StudioXpbdOptions o){return studio_xpbd_bind_rig(m,o,nullptr);}

extern "C" int studio_xpbd_metrics(void *ptr,StudioXpbdMetrics *out){if(!ptr||!out)return 0;*out=static_cast<Runtime*>(ptr)->metrics;return 1;}


extern "C" void studio_xpbd_set_interaction(void *ptr,float x,float y){auto *r=static_cast<Runtime*>(ptr);if(r&&std::isfinite(x)&&std::isfinite(y)){r->action_fx=std::clamp(x,-500.f,500.f);r->action_fy=std::clamp(y,-500.f,500.f);}}
