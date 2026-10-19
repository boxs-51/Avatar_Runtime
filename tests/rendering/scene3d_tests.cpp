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
        bool packed_equivalent=true;
        for (const auto bits:{"8","16"}) for (const auto mode:{"weights","uv","both"}) {
            const auto prefix=std::string("positive-alpha54-")+bits+"-"+mode;
            std::shared_ptr<const ars::assets::GlbModel> packed_model,float_model;
            test::check(ars::assets::ingest_glb(test::file(root/(prefix+"-packed.glb")),resource,packed_model)==ARS_OK &&
                        ars::assets::ingest_glb(test::file(root/(prefix+"-float.glb")),resource,float_model)==ARS_OK,
                        "packed and equivalent float fixtures admitted");
            ars::rendering::PreparedScene3D actual,expected;
            test::check(ars::rendering::prepare_static_3d(packed_model,actual)==ARS_OK &&
                        ars::rendering::prepare_static_3d(float_model,expected)==ARS_OK,
                        "packed and float scenes project");
            test::check(actual.draws.size()==1 && expected.draws.size()==1 &&
                        actual.draws[0].vertices.size()==3 && expected.draws[0].vertices.size()==3,
                        "nonempty packed/float control geometry");
            test::check(near(expected.draws[0].vertices[1].position[0],1.1f) &&
                        near(expected.draws[0].vertices[1].position[1],1),"joint index1 stays an integer");
            bool same=actual.draws[0].indices==expected.draws[0].indices;
            for (std::size_t i=0;i<3;++i) {
                const auto &a=actual.draws[0].vertices[i],&b=expected.draws[0].vertices[i];
                for (std::size_t c=0;c<3;++c) same=near(a.position[c],b.position[c]) && near(a.normal[c],b.normal[c]) && same;
                for (std::size_t c=0;c<4;++c) same=near(a.tangent[c],b.tangent[c]) && same;
                for (std::size_t c=0;c<2;++c) same=near(a.uv[c],b.uv[c]) && same;
            }
            std::printf("ALPHA54_CPU_%s_%s=%s\n",bits,mode,same?"PASS":"FAIL");
            packed_equivalent=same && packed_equivalent;
        }
        test::check(packed_equivalent,"normalized packed weights/UV match float equivalents");
        for (const auto &vertex:scene.draws[0].vertices)
            test::check(vertex.color==std::array<float,4>{1,1,1,1},"absent color defaults white");
        for (const auto label:{"float","8","16"}) for (const auto channels:{"3","4"}) {
            const auto name=std::string("positive-alpha55-")+label+"-"+channels+"-gradient-color.glb";
            std::shared_ptr<const ars::assets::GlbModel> colored;
            test::check(ars::assets::ingest_glb(test::file(root/name),resource,colored)==ARS_OK,"color fixture admits");
            ars::rendering::PreparedScene3D projected;
            test::check(ars::rendering::prepare_static_3d(colored,projected)==ARS_OK,"color projects");
            const std::array<std::array<float,4>,3> expected{{{0,1,0,1},{1,0,1,channels[0]=='3'?1.f:0.f},{1,1,1,1}}};
            for (std::size_t i=0;i<3;++i) {
                const auto &vertex=projected.draws[0].vertices[i];
                for (std::size_t c=0;c<4;++c) test::check(near(vertex.color[c],expected[i][c]),"RGB/RGBA linear decode");
                test::check(vertex.position==scene.draws[0].vertices[i].position,"color preserves pose");
            }
            std::printf("ALPHA55_CPU_%s_%s=PASS\n",label,channels);
        }
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
