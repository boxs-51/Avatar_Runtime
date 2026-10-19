#include "../../src/rendering/scene3d.hpp"
#include "../assets/asset_test_support.hpp"
#include <cmath>
#include <string>

static bool near(float a, float b) { return std::abs(a-b)<0.0001f; }
int main(int argc, char **argv) {
    test::check(argc==2,"corpus path");
    const std::filesystem::path root=argv[1];
    test::Host host;
    {
        ars::assets::Resource resource(test::Host::allocate,test::Host::release,&host);
        std::shared_ptr<const ars::assets::GlbModel> model;
        auto bytes=test::file(root/"positive-m2-3d-pose.glb");
        test::check(ars::assets::ingest_glb(bytes,resource,model)==ARS_OK,"C2 validates M2 pose fixture");
        ars::rendering::PreparedScene3D scene;
        test::check(ars::rendering::prepare_static_3d(model,scene)==ARS_OK,"C0 prepares two-joint pose");
        test::check(scene.draws.size()==1 && scene.draws[0].vertices.size()==3,"reachable triangle only");
        const auto &v=scene.draws[0].vertices;
        test::check(near(v[0].position[0],1.2f) && near(v[0].position[1],0) &&
                    !near(v[0].position[0],1.1f),"morph before scaled joint, wrong-order control");
        test::check(near(v[1].position[0],1.1f) && near(v[1].position[1],1),"second joint palette order");
        test::check(near(v[2].position[0],0.1f) && near(v[2].position[1],2),"second joint on third vertex");
        test::check(scene.draws[0].material.base_texture.image==0,"owned PNG reference projected");
        auto override_bytes=test::file(root/"positive-m2-3d-node-weight.glb");
        std::shared_ptr<const ars::assets::GlbModel> override_model;
        test::check(ars::assets::ingest_glb(override_bytes,resource,override_model)==ARS_OK,
                    "node-weight fixture validates");
        ars::rendering::PreparedScene3D override_scene;
        test::check(ars::rendering::prepare_static_3d(override_model,override_scene)==ARS_OK &&
                    near(override_scene.draws[0].vertices[0].position[0],1.0f),
                    "node morph weight overrides mesh weight");
        auto tangent_bytes=test::file(root/"positive-m2-3d-tangent.glb");
        std::shared_ptr<const ars::assets::GlbModel> tangent_model;
        test::check(ars::assets::ingest_glb(tangent_bytes,resource,tangent_model)==ARS_OK,
                    "tangent fixture validates");
        ars::rendering::PreparedScene3D tangent_scene;
        test::check(ars::rendering::prepare_static_3d(tangent_model,tangent_scene)==ARS_OK,
                    "tangent pose projects");
        const auto &t=tangent_scene.draws[0].vertices[0].tangent;
        test::check(std::abs(t[0]-0.894427f)<0.0001f &&
                    std::abs(t[1]-0.447214f)<0.0001f && near(t[3],1),
                    "morph tangent before nonuniform skin");
        auto old=scene.source;
        test::check(ars::rendering::prepare_static_3d(model,scene,99)==ARS_E_UNSUPPORTED &&
                    scene.source==old,"invalid scene rolls back");
        test::check(ars::rendering::prepare_static_3d(model,scene,ars::assets::glb_none,{1,1024})==
                    ARS_E_OUT_OF_BUDGET && scene.source==old,"GPU budget rolls back");
        test::check(ars::rendering::prepare_static_3d(model,scene,ars::assets::glb_none,{1024,1})==
                    ARS_E_OUT_OF_BUDGET && scene.source==old,"pose budget rolls back");
        test::check(ars::rendering::prepare_static_3d({},scene)==ARS_E_UNSUPPORTED &&
                    scene.source==old,"missing 3D representation rolls back");
        for (const auto *kind:{"perspective","orthographic","infinite"}) {
            auto camera_bytes=test::file(root/(std::string("positive-m2-3d-camera-")+kind+".glb"));
            std::shared_ptr<const ars::assets::GlbModel> camera_model;
            test::check(ars::assets::ingest_glb(camera_bytes,resource,camera_model)==ARS_OK,
                        "validated camera fixture");
            ars::rendering::PreparedScene3D camera_scene;
            test::check(ars::rendering::prepare_static_3d(camera_model,camera_scene)==ARS_OK &&
                        camera_scene.cameras.size()==1,"reachable camera projected");
            ars::rendering::Camera3D camera;
            test::check(ars::rendering::select_gltf_camera(camera_scene,3,8,8,camera)==ARS_OK,
                        "explicit glTF camera selection");
            const auto &m=camera.clip_from_world;
            const double x=m[0]*0.5+m[4]*1+m[12], y=m[1]*0.5+m[5]*1+m[13];
            test::check(std::abs(x)<1e-8 && std::abs(y)<1e-8,"camera view centers scene");
            auto previous=camera.clip_from_world;
            test::check(ars::rendering::select_gltf_camera(camera_scene,99,8,8,camera)==ARS_E_UNSUPPORTED &&
                        camera.clip_from_world==previous,"invalid camera leaves output intact");
        }
        {
            ars::rendering::PreparedScene3D hosted(&resource);
            const auto calls_before=host.calls.load();
            test::check(ars::rendering::prepare_static_3d(model,hosted)==ARS_OK &&
                        host.calls.load()>calls_before &&
                        hosted.draws.get_allocator().resource()==&resource &&
                        hosted.draws[0].vertices.get_allocator().resource()==&resource &&
                        hosted.draws[0].indices.get_allocator().resource()==&resource,
                        "retained CPU snapshot uses host allocator");
            const auto retained=hosted.draws[0].vertices[0].position;
            host.fail_at=host.calls.load()+1;
            test::check(ars::rendering::prepare_static_3d(model,hosted)==ARS_E_OUT_OF_BUDGET &&
                        hosted.draws[0].vertices[0].position==retained,
                        "host allocation failure preserves committed snapshot");
            host.fail_at=0;
        }
        scene={};model.reset();
    }
    test::check(host.live==0,"C0 source lifetime balanced");
    std::puts("PASS M2-C C0 pose, joint, morph, material and rollback");
}
