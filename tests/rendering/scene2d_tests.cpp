#include "rendering/scene2d.hpp"
#include "../assets/asset_test_support.hpp"
#include <limits>
#include <memory_resource>

using ars::assets::Image;
using ars::assets::Layer;
using ars::assets::Part;
using ars::rendering::PreparedScene2D;
using ars::rendering::Scene2DInput;
using ars::rendering::Scene2DLimits;
using ars::rendering::prepare_static_2d;

int main(int argc,char **argv) {
    test::check(argc==3,"schema worker and corpus required");
    auto *memory=std::pmr::get_default_resource();
    std::vector<Layer> layers;
    for (int i=0;i<3;++i) layers.emplace_back(memory);
    layers[0].z=1; layers[0].opacity=0.5f;
    layers[1].z=0; layers[1].opacity=0.25f;
    layers[2].z=-1; layers[2].visible=false;
    std::vector<Part> parts;
    for (int i=0;i<4;++i) {
        parts.emplace_back(memory);
        auto &p=parts.back();
        p.rect={0,0,2,2};
        p.vertices={{0,0},{2,0},{0,2}};
        p.uv={{0,0},{1,0},{0,1}};
        p.triangles={{0,1,2}};
    }
    parts[0].layer=0;
    parts[1].layer=1; parts[1].z=3;
    parts[2].layer=0;
    parts[3].layer=2;
    Image image(memory);
    image.width=4; image.height=4; image.rgba.resize(4*4*4,255);
    Scene2DInput input{layers,parts,{4,4},&image};
    PreparedScene2D prepared;
    test::check(prepare_static_2d(input,prepared)==ARS_OK,"static scene prepared");
    test::check(prepared.draws.size()==3 && prepared.draws[0].part_index==1 &&
                prepared.draws[1].part_index==0 && prepared.draws[2].part_index==2,
                "stable layer and part declaration order");
    test::check(prepared.draws[0].opacity==0.25f && prepared.draws[1].opacity==0.5f &&
                prepared.draws[2].opacity==0.5f && prepared.draws[0].vertices.size()==3 &&
                prepared.draws[0].triangles[0][2]==2,"effective opacity and owned geometry");
    const auto original_index=prepared.draws[0].part_index;
    parts[0].layer=99;
    test::check(prepare_static_2d(input,prepared)==ARS_E_INVALID_ARGUMENT &&
                prepared.draws.size()==3 && prepared.draws[0].part_index==original_index,
                "invalid part index preserves prior scene");
    parts[0].layer=0;
    test::check(prepare_static_2d(input,prepared,Scene2DLimits{128,2,2})==ARS_E_OUT_OF_BUDGET &&
                prepared.draws.size()==3,"vertex budget preserves prior scene");
    parts[0].triangles[0][2]=99;
    test::check(prepare_static_2d(input,prepared)==ARS_E_INVALID_ARGUMENT &&
                prepared.draws.size()==3,"invalid triangle preserves prior scene");
    parts[0].triangles[0][2]=2;
    image.rgba.pop_back();
    test::check(prepare_static_2d(input,prepared)==ARS_E_INVALID_ARGUMENT &&
                prepared.draws.size()==3,"short atlas preserves prior scene");
    image.rgba.push_back(255);
    layers[0].opacity=std::numeric_limits<float>::quiet_NaN();
    test::check(prepare_static_2d(input,prepared)==ARS_E_INVALID_ARGUMENT &&
                prepared.draws.size()==3,"nonfinite layer opacity preserves prior scene");
    layers[0].opacity=0.5f;

    test::Host host;
    {
        ars::assets::Resource resource(test::Host::allocate,test::Host::release,&host);
        ars::validation::SchemaClient schema({std::filesystem::absolute(argv[1]).string()});
        auto bytes=test::file(std::filesystem::path(argv[2])/"positive-2d.arschar");
        std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
        test::check(ars::assets::validate_bundle(bytes,schema,resource,model)==ARS_OK && bool(model),
                    "accepted bundle decoded before scene preparation");
        PreparedScene2D actual;
        test::check(prepare_static_2d(ars::rendering::scene_input(*model),actual)==ARS_OK &&
                    actual.draws.size()==2 && actual.atlas_width==model->image().width &&
                    actual.canvas==model->canvas(),"validated model feeds static scene");
    }
    test::check(host.live==0,"CPU model and host allocations released");
    std::puts("PASS M2-A private static 2D ordering, opacity, rollback and validated bundle");
}
