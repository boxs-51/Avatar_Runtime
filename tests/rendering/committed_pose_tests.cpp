#include "committed_test_support.hpp"
#include "../../src/rendering/committed_scene.hpp"
#include "../assets/asset_test_support.hpp"
#include <cmath>
#include <cstring>
#include <memory>
using namespace ars;
using namespace ars::testing39;
namespace {
rendering::PreparedScene2D two(const core::RenderFrameLease &frame){
    std::vector<rendering::LayerControl> layers;for(auto l:frame.layers())layers.push_back({l.visible,l.opacity,l.z});
    rendering::PreparedScene2D scene;check(ARS_OK,rendering::prepare_committed_2d(*frame.model(),
        std::span(frame.pose()->outputs.data(),frame.pose()->channel_count),layers,scene),"project committed2D");return scene;
}
rendering::PreparedScene3D three(const core::RenderFrameLease &frame){
    rendering::PreparedScene3D scene;check(ARS_OK,rendering::prepare_committed_3d(*frame.model(),
        std::span(frame.pose()->outputs.data(),frame.pose()->channel_count),scene),"project committed3D");return scene;
}
void euler(const std::filesystem::path &path){
    Fixture f(path);auto neutral=f.capture();auto a=two(*neutral);auto b=three(*neutral);
    test::check(a.draws.size()==2,"both native layers present");near(a.draws[1].vertices[0].x,8,"nonzero neutral preserves2D authored base");
    near(b.draws[0].vertices[0].position[0],1+std::cos(.3)*.05,"neutral morph then scaled authored joint X");
    near(b.draws[0].vertices[0].position[1],std::sin(.3)*.05,"neutral authored rotation preserved");
    near(b.draws[0].vertices[1].position[0],1.025,"other joint uses its own palette");
    near(b.draws[0].vertices[1].position[1],1,"second joint translation");
    rendering::PreparedScene3D reference;check(ARS_OK,rendering::prepare_static_3d(*neutral->model(),reference),"independent neutral static control");
    for(std::size_t i=0;i<3;++i)for(std::size_t k=0;k<3;++k)
        near(b.draws[0].vertices[i].position[k],reference.draws[0].vertices[i].position[k],"neutral committed/static parity");
    f.submit({channel("mouth.open",{.75f,0,0,0}),channel("camera.position",{.5f,0,0,0},ARS_VALUE_VEC3)});
    auto moved=f.capture();auto dynamic2=two(*moved);auto dynamic3=three(*moved);
    near(dynamic2.draws[1].vertices[0].x,16,"2D scalar minus nonzero neutral gives8px displacement");
    near(dynamic3.draws[0].vertices[0].position[0],1+std::cos(.3)*.15,"morph precedes scaled joint, not post-skin delta");
    near(dynamic3.draws[0].vertices[1].position[0],1.075,"other skin joint dynamic morph");
    rendering::Camera3D camera;check(ARS_OK,rendering::select_gltf_camera(dynamic3,3,64,64,camera),"posed camera selection");
    near(camera.clip_from_world[12],-.25,"camera translation changes clip projection");
    near(two(*neutral).draws[1].vertices[0].x,8,"historical frame remains immutable after S update");
    f.submit({channel("mouth.open",{.75f,0,0,0}),channel("head.yaw_rad",{.6f,0,0,0}),
        channel("head.pitch",{.2f,0,0,0}),channel("head.roll",{.35f,0,0,0}),
        channel("head.position",{.9f,.1f,0,0},ARS_VALUE_VEC3),channel("head.scale",{1.5f,1,1,0},ARS_VALUE_VEC3)});
    auto transformed=f.capture();auto t=three(*transformed);
    // Independently apply authored scale, roll, pitch, yaw, authored Z and
    // neutral-relative translation. This is deliberately not quaternion code.
    const double x=.1875*std::cos(.2),y=.1875*std::sin(.2)*std::cos(.3),z=.1875*std::sin(.2)*std::sin(.3);
    const double rx=std::cos(.4)*x+std::sin(.4)*z,rz=-std::sin(.4)*x+std::cos(.4)*z;
    near(t.draws[0].vertices[0].position[0],1.5+std::cos(.3)*rx-std::sin(.3)*y,"yaw*pitch*roll and authored rotation X");
    near(t.draws[0].vertices[0].position[1],std::sin(.3)*rx+std::cos(.3)*y,"Euler composition Y");
    near(t.draws[0].vertices[0].position[2],rz,"Euler composition Z");
    std::puts("PASS actual committed numeric2D/nonzero neutral/skin/morph/TRS/Euler/camera/historical parity");
}
void quaternion(const std::filesystem::path &path){
    Fixture f(path);auto neutral=f.capture();auto base=three(*neutral);
    f.submit({channel("head.rotation",{0,0,0,1},ARS_VALUE_QUAT_XYZW)});auto changed=f.capture();auto scene=three(*changed);
    // Current identity times inverse(neutral X0.2) rotates a Y-axis vertex
    // only if it uses joint0. Fixture vertex0 lies on X, so compare a normal:
    // authored Z0.3 rotates R_x(-0.2)*(0,0,1).
    const auto &n=scene.draws[0].vertices[0].normal;
    near(n[0],-std::sin(.3)*std::sin(.2),"quaternion neutral inverse then authored rotation normal X");
    near(n[1],std::cos(.3)*std::sin(.2),"quaternion neutral inverse normal Y");
    near(n[2],std::cos(.2),"quaternion neutral inverse normal Z");
    test::check(std::abs(n[1]-base.draws[0].vertices[0].normal[1])>.1,"quaternion control is discriminating");
    std::puts("PASS actual committed quaternion nonidentity neutral/authored transform");
}
void hierarchy(const std::filesystem::path &path){
    Fixture f(path);ars_layer_change layer{};layer.struct_size=sizeof(layer);layer.abi_version=ARS_ABI_VERSION;
    layer.layer_id={"base.body",9};layer.visible=0;layer.opacity=1;
    check(ARS_OK,ars_character_set_visual_layer(f.runtime,f.character,&layer),"hide parent");
    auto hidden=f.capture();test::check(two(*hidden).draws.empty(),"parent visibility hides descendants");
    layer.visible=1;layer.opacity=.5f;check(ARS_OK,ars_character_set_visual_layer(f.runtime,f.character,&layer),"parent opacity");
    layer.layer_id={"face.mouth",10};layer.opacity=.5f;layer.z_index=-1;
    check(ARS_OK,ars_character_set_visual_layer(f.runtime,f.character,&layer),"child opacity/Z");
    auto faded=f.capture();auto scene=two(*faded);
    test::check(scene.draws.size()==2&&scene.draws[0].part_index==1,"runtime Z changes stable part order");
    near(scene.draws[0].opacity,.25,"opacity multiplies parent chain");
    near(scene.draws[1].opacity,.5,"parent retains local opacity");
    std::puts("PASS actual committed hierarchy/visibility/opacity/Z");
}
}
int main(int argc,char **argv){if(argc!=2)return 2;const std::filesystem::path root=argv[1];
    euler(root/"committed-euler.arschar");quaternion(root/"committed-quaternion.arschar");hierarchy(root/"committed-hierarchy.arschar");}
