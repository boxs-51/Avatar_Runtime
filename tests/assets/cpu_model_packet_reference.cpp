// Test-only reference. Encoded decoders are deliberately absent from cpu_model_guard.
#include "../../src/assets/cpu_model_packet.hpp"
#include "../../src/rendering/scene2d.hpp"
#include "asset_test_support.hpp"
#include <algorithm>
#include <charconv>
namespace {
using namespace ars::assets;
bool pairs(const auto &a,const auto &b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](auto x,auto y){return x.x==y.x && x.y==y.y;});
}
void equal(const ValidatedCpuModel &a,const ValidatedCpuModel &b) {
    test::check(a.canvas()==b.canvas() && a.image().width==b.image().width && a.image().height==b.image().height && a.image().rgba==b.image().rgba && !a.glb() && !b.glb(),"canvas/complete decoded atlas identity");
    test::check(a.layers().size()==b.layers().size() && a.parameters().size()==b.parameters().size() && a.semantics().size()==b.semantics().size() && a.parts().size()==b.parts().size(),"all model counts");
    for(std::size_t i=0;i<a.layers().size();++i) {const auto &x=a.layers()[i],&y=b.layers()[i];test::check(x.id==y.id && x.z==y.z && x.visible==y.visible && x.opacity==y.opacity,"complete flattened layers");}
    for(std::size_t i=0;i<a.parameters().size();++i) {const auto &x=a.parameters()[i],&y=b.parameters()[i];test::check(x.id==y.id && x.minimum==y.minimum && x.maximum==y.maximum && x.initial==y.initial,"all parameter fields");}
    for(std::size_t i=0;i<a.semantics().size();++i) {
        const auto &x=a.semantics()[i],&y=b.semantics()[i];
        test::check(x.id==y.id && x.type==y.type && x.units==y.units && x.target2d==y.target2d && x.target3d==y.target3d && x.initial==y.initial && x.minimum==y.minimum && x.maximum==y.maximum && x.has_minimum==y.has_minimum && x.has_maximum==y.has_maximum && x.has_default==y.has_default,"all semantic fields and f64 values");
    }
    for(std::size_t i=0;i<a.parts().size();++i) {
        const auto &x=a.parts()[i],&y=b.parts()[i];
        test::check(x.id==y.id && x.rect==y.rect && x.pivot.x==y.pivot.x && x.pivot.y==y.pivot.y && x.z==y.z && x.layer==y.layer && pairs(x.vertices,y.vertices) && pairs(x.uv,y.uv) && x.triangles==y.triangles && x.deforms.size()==y.deforms.size(),"all part fields/UV/indices");
        for(std::size_t j=0;j<x.deforms.size();++j) test::check(x.deforms[j].parameter==y.deforms[j].parameter && pairs(x.deforms[j].deltas,y.deforms[j].deltas),"all deform mappings/deltas");
    }
    ars::rendering::PreparedScene2D x,y;
    test::check(ars::rendering::prepare_static_2d(ars::rendering::scene_input(a),x)==ARS_OK && ars::rendering::prepare_static_2d(ars::rendering::scene_input(b),y)==ARS_OK && x.draws.size()==y.draws.size(),"native scene oracle");
    for(std::size_t i=0;i<x.draws.size();++i) {
        const auto &p=x.draws[i],&q=y.draws[i];
        test::check(p.layer_index==q.layer_index && p.part_index==q.part_index && p.opacity==q.opacity && p.atlas_rect==q.atlas_rect && pairs(p.vertices,q.vertices) && pairs(p.uv,q.uv) && p.triangles==q.triangles,"prepared geometry/effective opacity/order parity");
    }
}
}
int main(int argc,char **argv) {
    if(argc!=5 && argc!=6)return 2;
    auto input=test::file(argv[2]),bytes=test::file(argv[3]);
    packet::Identity id; std::string_view nonce=argv[4]; auto parsed=std::from_chars(nonce.data(),nonce.data()+nonce.size(),id.nonce);
    test::check(parsed.ec==std::errc{} && parsed.ptr==nonce.data()+nonce.size() && packet::digest(input,id.request)==ARS_OK,"reference identity");
    test::Host host;
    {
        Resource r(test::Host::allocate,test::Host::release,&host);
        ars::validation::SchemaClient schema({std::filesystem::absolute(argv[1]).string()});
        std::shared_ptr<const ValidatedCpuModel> original,copy;
        test::check(validate_bundle(input,schema,r,original)==ARS_OK,"separate original ingestion");
        if(argc==6) {
            test::check(std::string_view(argv[5])=="--expect-budget","budget mode");
            std::pmr::vector<std::uint8_t> prior(&r); prior.assign({1,2,3});
            test::check(packet::encode(*original,id,r,prior)==ARS_E_OUT_OF_BUDGET && prior==std::pmr::vector<std::uint8_t>({1,2,3},&r),"valid native model exceeds packet cap without replacing prior bytes");
            test::check(packet::decode(bytes,id,r,copy)==ARS_E_OUT_OF_BUDGET && !copy,"budget rejection has no partial model");
        } else {
            test::check(packet::decode(bytes,id,r,copy)==ARS_OK,"independent typed decoder");
            equal(*original,*copy);
        }
    }
    test::check(host.live==0,"reference balanced");
    std::puts(argc==6?"NATIVE_VALID_PACKET_BUDGET_ROLLBACK_PASS":"NATIVE_FIELD_AND_SCENE_PARITY_PASS");
}
