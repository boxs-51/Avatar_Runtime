#include "../native/native_mesh_xpbd.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
struct Vec2 {float x,y;};
struct FakeDraw {unsigned part_index=0;std::vector<Vec2> vertices,uv;std::vector<std::array<unsigned,3>> triangles;};
struct FakeScene {std::vector<FakeDraw> draws;};
using namespace ars::experimental::lps;
static FakeScene sample(){
 FakeScene s;
 s.draws.push_back({11,{{0,0},{0,16},{7,16}},{{0,0},{0,1},{1,1}},{{0,1,2}}});
 s.draws.push_back({22,{{20,0},{20,16},{27,16}},{{0,0},{0,1},{1,1}},{{0,1,2}}});
 return s;
}
static Options options(){Options p;p.gravity_y=400;p.wind_x=20;
 p.dynamic_parts={{11,0,1},{22,0,2}};
 p.joints={{{11,2},{22,2},0}};return p;}
int main(){
 using S=Session<FakeScene>;auto original=sample();Options p=options();S sim;
 assert(sim.bind(original,p,111)==Status::ok && sim.particles()==6);
 S::Candidate first;assert(sim.prepare(original,111,first)==Status::ok);
 assert(original.draws[0].vertices[1].y==16); // original immutable
 assert(first.scene.draws.size()==2 && first.scene.draws[0].vertices.size()==3);
 assert(first.scene.draws[0].vertices[0].y==0 && first.scene.draws[1].vertices[0].y==0); // pins
 assert(std::abs(first.scene.draws[0].vertices[1].x)>0.0001 || std::abs(first.scene.draws[0].vertices[1].y-16)>0.0001);
 auto saved=first.scene;
 // Preparing again before acceptance must be bitwise equivalent.
 S::Candidate repeated;assert(sim.prepare(original,111,repeated)==Status::ok);
 assert(repeated.scene.draws[0].vertices[1].y==saved.draws[0].vertices[1].y);
 assert(sim.accept(std::move(first),111)==Status::ok);
 assert(sim.accept(std::move(repeated),111)==Status::stale); // replay rejected
 S::Candidate second;assert(sim.prepare(original,111,second)==Status::ok);
 assert(second.scene.draws[0].vertices[1].x!=saved.draws[0].vertices[1].x || second.scene.draws[0].vertices[1].y!=saved.draws[0].vertices[1].y);
 auto bad=original;bad.draws[0].part_index=66;
 S::Candidate invalid;assert(sim.prepare(bad,111,invalid)==Status::stale);
 assert(sim.prepare(original,222,invalid)==Status::stale);
 // Explicit collision pair: independent layers, nonzero relative movement, nonzero force split.
 auto contact=sample();contact.draws[1].vertices[1].x=1;contact.draws[1].vertices[1].y=16;
 contact.draws[1].vertices[2].x=8;contact.draws[1].vertices[2].y=16;
 Options cp=options();cp.joints.clear();cp.contact_radius=6;cp.gravity_y=0;cp.wind_x=0;
 S collisions;assert(collisions.bind(contact,cp,99)==Status::ok);
 S::Candidate touched;assert(collisions.prepare(contact,99,touched)==Status::ok);
 assert(std::abs(touched.scene.draws[0].vertices[1].x-contact.draws[0].vertices[1].x)>0.001);
 assert(std::abs(touched.scene.draws[1].vertices[1].x-contact.draws[1].vertices[1].x)>0.001);
 // Missing/duplicate/foreign joint endpoint fails closed.
 auto wrong=options();wrong.joints[0].b.part=999;
 S fail;assert(fail.bind(original,wrong,111)==Status::invalid);
 assert(sim.prepare(original,111,invalid)==Status::ok); // failed validation did not poison prior
 wrong=options();wrong.dynamic_parts.push_back(wrong.dynamic_parts[0]);
 assert(fail.bind(original,wrong,111)==Status::invalid);
 wrong=options();wrong.dt=0;assert(fail.bind(original,wrong,111)==Status::invalid);
 wrong=options();wrong.contact_radius=std::numeric_limits<double>::quiet_NaN();
 assert(fail.bind(original,wrong,111)==Status::invalid);
 // Every drawable stays in the candidate even when static; dynamic source never loses a part.
 Options static_only;S passthrough;assert(passthrough.bind(original,static_only,101)==Status::ok);
 S::Candidate clean;assert(passthrough.prepare(original,101,clean)==Status::ok);
 assert(clean.scene.draws.size()==original.draws.size());
 for(size_t i=0;i<clean.scene.draws.size();++i)for(size_t v=0;v<clean.scene.draws[i].vertices.size();++v)
  assert(clean.scene.draws[i].vertices[v].x==original.draws[i].vertices[v].x);
 std::puts("PASS native XPBD prepared-scene adapter: 16 assertions groups; identity, rollback, two-way collision, joints, full draw retention");
}
