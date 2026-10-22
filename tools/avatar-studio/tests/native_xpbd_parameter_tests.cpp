#include "../native_mesh_xpbd.hpp"
#include "../studio_motion_filter.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
namespace xp=ars::experimental::lps;
struct Point{float x,y;};
struct Mesh{unsigned part_index;std::vector<Point> vertices,uv;std::vector<std::array<unsigned,3>> triangles;};
struct Scene{std::vector<Mesh> draws;};
static Scene mesh(){
 Mesh a{0,{{0,0},{1,0},{0,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{0,1,2},{1,3,2}}};
 return {{a}};
}
static std::pair<double,double> run(xp::Options opt,int steps){
 xp::Session<Scene> s;Scene source=mesh();
 opt.dynamic_parts={{0,0,1}};
 if(s.bind(source,opt,1)!=xp::Status::ok)throw 1;
 double last=0;
 for(int t=0;t<steps;++t){
  xp::Session<Scene>::Candidate c;
  if(s.prepare(source,1,c)!=xp::Status::ok)throw 2;
  last=c.scene.draws[0].vertices[3].y;
  if(s.accept(std::move(c),1)!=xp::Status::ok)throw 3;
 }
 return {last,s.revision()};
}
static void inertia_oracle(){
 xp::Options o;o.dt=1.0/120;o.iterations=12;o.damping=.98;
 o.gravity_y=0;o.wind_x=0;o.edge_compliance=.001;
 o.pose_compliance=.004;o.source_motion_transfer=.08;o.substeps=3;
 o.dynamic_parts={{0,0,1}};
 Scene pose=mesh();xp::Session<Scene> solver;
 if(solver.bind(pose,o,77)!=xp::Status::ok)throw 40;
 pose.draws[0].vertices[0].x+=.7f;
 pose.draws[0].vertices[1].x+=.7f;
 pose.draws[0].vertices[2].x+=.7f;
 pose.draws[0].vertices[3].x+=.7f;
 double start=0,after=0,last=0,maximum=0;
 for(int t=0;t<180;++t){
  xp::Session<Scene>::Candidate c;
  if(solver.prepare(pose,77,c)!=xp::Status::ok)throw 41;
  double x=c.scene.draws[0].vertices[3].x;
  if(t==0)start=x;
  if(t==30)after=x;
  if(t>80)maximum=std::max(maximum,x);
  last=x;
  if(solver.accept(std::move(c),77)!=xp::Status::ok)throw 42;
 }
 std::cout<<"INERTIA_METRICS first="<<start<<" after30="<<after<<" last="<<last<<" max="<<maximum<<" target=1.7\n";
 if(!(start>1.01&&start<1.60))throw 43; // delayed follower, not teleported
 if(!(after>start+.02))throw 44; // catch-up after source stops
 if(!(std::abs(last-1.7)<std::abs(start-1.7)))throw 45;
}
static void display_smoothing_oracle(){
 using ars::experimental::lps::MotionXY;
 using ars::experimental::lps::MotionFilter;
 MotionFilter filtered,raw;
 std::vector<MotionXY> point(1,MotionXY{0,0});
 MotionFilter::Candidate c;
 if(!filtered.prepare(point,1./60.,.085,c))throw 80;
 if(!filtered.accept(std::move(c)))throw 81;
 double filtered_jerk=0,raw_jerk=0;
 double fp=0,fv=0,rp=0,rv=0;
 const double timeline[4]={1./60.,1./48.,1./72.,1./60.};
 for(int i=0;i<120;++i){
  const double new_x=1.+((i%2)?0.045:-0.045);
  point[0].x=static_cast<float>(new_x);
  MotionFilter::Candidate next;
  if(!filtered.prepare(point,timeline[i%4],.085,next))throw 82;
  double current=next.positions[0].x;
  if(i==0&&!(current>0&&current<1))throw 83;
  double v=current-fp;
  if(i>2)filtered_jerk+=std::abs(v-fv);
  fp=current;fv=v;
  double raw_v=new_x-rp;
  if(i>2)raw_jerk+=std::abs(raw_v-rv);
  rp=new_x;rv=raw_v;
  if(!filtered.accept(std::move(next)))throw 84;
 }
 if(!(filtered_jerk<raw_jerk*.35))throw 85;
 if(!(fp>.9&&fp<1.1))throw 86;
 MotionFilter::Candidate rejected;
 point[0].x=1234;
 if(!filtered.prepare(point,1./60.,.085,rejected))throw 87;
 MotionFilter::Candidate valid;
 point[0].x=1;
 if(!filtered.prepare(point,1./60.,.085,valid))throw 88;
 if(valid.positions[0].x>2)throw 89; // rejected render must not mutate filter
 if(!filtered.accept(std::move(valid)))throw 90;
 MotionFilter::Candidate bad;
 if(filtered.prepare(point,-1.,.085,bad))throw 91;
 if(filtered.prepare(point,1./60.,-1.,bad))throw 92;
 std::cout<<"XPBD_DISPLAY_SMOOTH_PASS filtered_jerk="<<filtered_jerk<<" raw_jerk="<<raw_jerk
          <<" ratio="<<filtered_jerk/raw_jerk<<" settled="<<fp<<"\n";
}
static void interaction_force_oracle(){
 xp::Options opt;opt.gravity_y=0;opt.wind_x=0;opt.damping=.995;
 opt.pose_compliance=.08;opt.edge_compliance=.005;opt.source_motion_transfer=0;
 opt.dynamic_parts={{0,0,1}};opt.substeps=4;
 Scene pose=mesh();
 xp::Session<Scene> still,acted;
 if(still.bind(pose,opt,101)!=xp::Status::ok||acted.bind(pose,opt,102)!=xp::Status::ok)throw 110;
 double a=0,b=0,peak=0;
 for(int k=0;k<70;++k){
  xp::Session<Scene>::Candidate x,y;
  if(still.prepare(pose,101,x,0,0)!=xp::Status::ok)throw 111;
  // Simulated agent pull 12 frames, release and settle.
  const double force=k<12?220:0;
  if(acted.prepare(pose,102,y,force,0)!=xp::Status::ok)throw 112;
  a=x.scene.draws[0].vertices[3].x;
  b=y.scene.draws[0].vertices[3].x;
  peak=std::max(peak,std::abs(b-a));
  if(std::abs(y.scene.draws[0].vertices[0].x)>1e-6)throw 113;
  if(still.accept(std::move(x),101)!=xp::Status::ok||
     acted.accept(std::move(y),102)!=xp::Status::ok)throw 114;
 }
 std::cout<<"INTERACTION_ORACLE peak="<<peak<<" tail="<<std::abs(b-a)<<"\n";
 if(!(peak>.001))throw 115;  // distinct from baseline
 if(!(std::abs(b-a)<peak))throw 116;  // settles after release
}
int main(){
 try{
  xp::Options z;z.gravity_y=0;z.wind_x=0;z.damping=1;z.iterations=8;
  auto idle=run(z,32);if(std::abs(idle.first-1)>0.00001)throw 10;
  z.gravity_y=80;auto g=run(z,32);if(!(g.first>idle.first+.00001))throw 11;
  z.damping=.4;auto damped=run(z,32);if(!(std::abs(g.first-damped.first)>1e-6))throw 12;
  z.damping=1;z.dt=1./240.;auto fast=run(z,32);if(!(std::abs(g.first-fast.first)>1e-6))throw 13;
  z.dt=1./120.;z.iterations=1;auto loose=run(z,32);
  z.iterations=18;auto tight=run(z,32);
  if(!std::isfinite(loose.first)||!std::isfinite(tight.first))throw 14;
  z.damping=-.01;xp::Session<Scene> bad;
  if(bad.bind(mesh(),z,42)!=xp::Status::invalid)throw 15;
  z.damping=1.01;
  if(bad.bind(mesh(),z,42)!=xp::Status::invalid)throw 16;
  z.damping=.99;z.dynamic_parts={{0,99,1}};
  if(bad.bind(mesh(),z,42)!=xp::Status::invalid)throw 17;
  z.dynamic_parts={{0,1,1}};
  if(bad.bind(mesh(),z,42)!=xp::Status::ok)throw 18;
  xp::Session<Scene>::Candidate c;
  if(bad.prepare(mesh(),43,c)!=xp::Status::stale)throw 19;
  if(bad.prepare(mesh(),42,c)!=xp::Status::ok)throw 20;
  if(std::abs(c.scene.draws[0].vertices[1].x-1)>1e-6)throw 21;
  // Skirt collision: moving garment vertices start inside a static body
  // triangle and must be expelled by surface contact.
  {
   Scene scene;
   Mesh body{0,{{0,0},{2,0},{0,2}},{{0,0},{1,0},{0,1}},{{0,1,2}}};
   Mesh skirt{1,{{.4f,.4f},{1.6f,1.6f},{1.7f,1.6f}},
      {{0,0},{1,0},{0,1}},{{0,1,2}}};
   scene.draws={body,skirt};
   xp::Options o;o.gravity_y=0;o.wind_x=0;o.pose_compliance=.1;
   o.source_motion_transfer=0;o.surface_thickness=.08;
   o.dynamic_parts={{1,1,1}};
   xp::Session<Scene> session;
   if(session.bind(scene,o,90)!=xp::Status::ok)throw 60;
   xp::Session<Scene>::Candidate c;
   if(session.prepare(scene,90,c)!=xp::Status::ok)throw 61;
   const auto v=c.scene.draws[1].vertices[0];
   const bool inside=v.x>=0&&v.y>=0&&v.x+v.y<=2;
   std::cout<<"SKIRT_CONTACT_METRICS x="<<v.x<<" y="<<v.y<<" inside="<<inside<<"\n";
   if(inside)throw 62;
   if(session.accept(std::move(c),90)!=xp::Status::ok)throw 63;
  }  // Hair/skirt synthetic oracle: multiple roots, bending and surface clearance.
  {
   Scene hair=mesh();xp::Options cfg;cfg.gravity_y=70;cfg.wind_x=35;
   cfg.dynamic_parts={{0,0,1}};cfg.anchors={{0,0},{0,1}};
   cfg.bending_compliance=.00001;cfg.surface_thickness=.05;
   xp::Session<Scene> h;
   if(h.bind(hair,cfg,100)!=xp::Status::ok)throw 50;
   xp::Session<Scene>::Candidate hc;
   if(h.prepare(hair,100,hc)!=xp::Status::ok)throw 51;
   if(std::abs(hc.scene.draws[0].vertices[0].x)>1e-5||
      std::abs(hc.scene.draws[0].vertices[1].x-1)>1e-5)throw 52;
   if(h.accept(std::move(hc),100)!=xp::Status::ok)throw 53;
   std::cout<<"HAIR_REGION_ANCHOR_BENDING_PASS\n";
  }
  interaction_force_oracle();
  display_smoothing_oracle();
  inertia_oracle();
  std::cout<<"XPBD_PARAMETER_MATRIX_PASS idle=1 gravity=1 damping=1 timestep=1 iterations=1 invalid=3 pin=1 stale=1\n";
 }catch(int e){std::cerr<<"XPBD_PARAMETER_MATRIX_FAIL oracle="<<e<<"\n";return 1;}
}
