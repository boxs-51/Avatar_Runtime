// Compiles against the ACTUAL Alpha native PreparedScene2D public-internal header.
#include "../../../src/rendering/scene2d.hpp"
#include "../native/native_mesh_xpbd.hpp"
#include <cassert>
#include <cstdio>
int main(){
 using namespace ars::experimental::lps;
 ars::rendering::PreparedScene2D scene;
 for(unsigned part: {1u,2u}){
  ars::rendering::DrawPart2D d;d.part_index=part;d.layer_index=part;
  d.vertices={{float(part*20),0},{float(part*20),20},{float(part*20+8),20}};
  d.uv={{0,0},{0,1},{1,1}};d.triangles={{0,1,2}};
  scene.draws.push_back(std::move(d));
 }
 Options opt;opt.gravity_y=120;opt.dynamic_parts={{1,0,1},{2,0,1}};
 opt.joints={{{1,2},{2,2},1e-7}};opt.contact_radius=1;
 Session<ars::rendering::PreparedScene2D> physics;
 assert(physics.bind(scene,opt,777)==Status::ok);
 decltype(physics)::Candidate candidate;
 assert(physics.prepare(scene,777,candidate)==Status::ok);
 assert(candidate.scene.draws.size()==2);
 assert(candidate.scene.draws[0].vertices[0].x==scene.draws[0].vertices[0].x);
 assert(physics.accept(std::move(candidate),777)==Status::ok);
 std::puts("PASS native XPBD against actual alpha PreparedScene2D header");
}
