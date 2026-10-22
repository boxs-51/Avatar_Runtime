// Real validated .arschar -> committed Runtime pose -> genuine native 2D mesh ->
// nonshipping XPBD candidate. GLB path retained and compared, never faked.
#include "../../../tests/rendering/committed_test_support.hpp"
#include "../../../src/rendering/committed_scene.hpp"
#include "../native/native_mesh_xpbd.hpp"
#include <cmath>
#include <cstdio>
using namespace ars;
using namespace ars::testing39;
using namespace ars::experimental::lps;
using Scene=rendering::PreparedScene2D;
static Scene from_frame(const core::RenderFrameLease&f){
 std::vector<rendering::LayerControl> controls;
 for(auto layer:f.layers()) controls.push_back({layer.visible,layer.opacity,layer.z});
 Scene scene;check(ARS_OK,rendering::prepare_committed_2d(*f.model(),
   {f.pose()->outputs.data(),f.pose()->channel_count},controls,scene),"prepare whole committed2D");
 return scene;
}
int main(int argc,char**argv){
 test::check(argc==2,"requires generated committed-euler.arschar");
 Fixture fixture(argv[1]);auto first=fixture.capture();auto original=from_frame(*first);
 test::check(original.draws.size()>=2,"whole model has two drawable parts");
 rendering::PreparedScene3D glb_before;
 check(ARS_OK,rendering::prepare_committed_3d(*first->model(),
   {first->pose()->outputs.data(),first->pose()->channel_count},glb_before),"full original GLB");
 Options config;config.gravity_y=120;config.wind_x=18;config.contact_radius=0.5;
 config.dynamic_parts={{original.draws[0].part_index,0,1},{original.draws[1].part_index,0,2}};
 config.joints={{{original.draws[0].part_index,2},{original.draws[1].part_index,2},0.0001}};
 Session<Scene> xpbd;test::check(xpbd.bind(original,config,1)==Status::ok,"bind actual alpha model");
 Session<Scene>::Candidate speculative;
 test::check(xpbd.prepare(original,1,speculative)==Status::ok,"candidate actual alpha geometry");
 test::check(speculative.scene.draws.size()==original.draws.size(),"all original parts retained");
 for(std::size_t i=0;i<original.draws.size();++i){
  const auto&a=original.draws[i],&b=speculative.scene.draws[i];
  test::check(a.part_index==b.part_index&&a.triangles==b.triangles&&a.uv.size()==b.uv.size(),"mesh identity/topology/UV retained");
 }
 test::check(xpbd.prepare(original,2,speculative)==Status::stale,"cross model generation rejected");
 test::check(xpbd.revision()==1,"failed preparation does not commit state");
 fixture.submit({channel("mouth.open",{.75f,0,0,0})});
 auto second=fixture.capture();auto changed=from_frame(*second);
 test::check(changed.draws.size()==original.draws.size(),"full model dynamic frame retained");
 Session<Scene>::Candidate next;
 test::check(xpbd.prepare(changed,1,next)==Status::ok,"project XPBD atop real changed pose");
 test::check(next.scene.draws.size()==changed.draws.size(),"full native scene stays visible");
 test::check(xpbd.accept(std::move(next),1)==Status::ok,"explicit transaction admit");
 rendering::PreparedScene3D glb_after;
 check(ARS_OK,rendering::prepare_committed_3d(*second->model(),
   {second->pose()->outputs.data(),second->pose()->channel_count},glb_after),"GLB unaffected by native2D XPBD");
 test::check(glb_after.draws.size()==glb_before.draws.size(),"full GLB draw set preserved");
 // All accepted runtime snapshots remain unmodified; exact GPU owner/publish test separate.
 auto replay=from_frame(*first);
 test::check(replay.draws.size()==original.draws.size(),"old snapshot still projects");
 std::puts("PASS native committed-model XPBD scene integration (CPU candidate only; NOT GPU publication or Viewer parity)");
}
