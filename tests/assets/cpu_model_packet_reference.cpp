// Test-only reference. Encoded decoders are deliberately absent from cpu_model_guard.
#include "../../src/assets/cpu_model_packet.hpp"
#include "../../src/rendering/scene2d.hpp"
#include "../../src/rendering/scene3d.hpp"
#include "asset_test_support.hpp"
#include <algorithm>
#include <charconv>
namespace {
using namespace ars::assets;
bool pairs(const auto &a,const auto &b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](auto x,auto y){return x.x==y.x && x.y==y.y;});
}
void compare_glb(const GlbModel &a,const GlbModel &b) {
    const auto &x=a.data(),&y=b.data();
    test::check(x.scene==y.scene && x.views.size()==y.views.size() && x.accessors.size()==y.accessors.size() && x.meshes.size()==y.meshes.size() && x.nodes.size()==y.nodes.size() && x.skins.size()==y.skins.size() && x.animations.size()==y.animations.size() && x.scenes.size()==y.scenes.size() && x.images.size()==y.images.size(),"all GLB counts/default scene");
    for(std::size_t i=0;i<x.views.size();++i) {const auto &p=x.views[i],&q=y.views[i];test::check(p.offset==q.offset && p.length==q.length && p.stride==q.stride && p.target==q.target,"GLB views");}
    for(std::size_t i=0;i<x.accessors.size();++i) {const auto &p=x.accessors[i],&q=y.accessors[i];test::check(p.count==q.count && p.component==q.component && p.components==q.components && p.rows==q.rows && p.columns==q.columns && p.view==q.view && p.offset==q.offset && p.normalized==q.normalized && p.values==q.values,"all accessor shape/domain/values");}
    for(std::size_t i=0;i<x.meshes.size();++i) {const auto &p=x.meshes[i],&q=y.meshes[i];test::check(p.name==q.name && p.weights==q.weights && p.target_names==q.target_names && p.primitives.size()==q.primitives.size(),"mesh metadata");
        for(std::size_t j=0;j<p.primitives.size();++j) {const auto &u=p.primitives[j],&v=q.primitives[j];test::check(u.position==v.position && u.normal==v.normal && u.tangent==v.tangent && u.uv==v.uv && u.color==v.color && u.joints==v.joints && u.weights==v.weights && u.material==v.material && u.mode==v.mode && u.indices==v.indices && u.morphs.size()==v.morphs.size(),"primitive fields");for(std::size_t k=0;k<u.morphs.size();++k)test::check(u.morphs[k].position==v.morphs[k].position && u.morphs[k].normal==v.morphs[k].normal && u.morphs[k].tangent==v.morphs[k].tangent,"morph fields");}}
    for(std::size_t i=0;i<x.nodes.size();++i) {const auto &p=x.nodes[i],&q=y.nodes[i];test::check(p.name==q.name && p.children==q.children && p.weights==q.weights && p.mesh==q.mesh && p.skin==q.skin && p.has_matrix==q.has_matrix && p.matrix==q.matrix && p.translation==q.translation && p.scale==q.scale && p.rotation==q.rotation,"node graph and transforms");}
    for(std::size_t i=0;i<x.skins.size();++i) {const auto &p=x.skins[i],&q=y.skins[i];test::check(p.name==q.name && p.joints==q.joints && p.inverse_bind==q.inverse_bind && p.skeleton==q.skeleton,"skins");}
    for(std::size_t i=0;i<x.animations.size();++i) {const auto &p=x.animations[i],&q=y.animations[i];test::check(p.name==q.name && p.samplers.size()==q.samplers.size() && p.channels.size()==q.channels.size(),"animations");for(std::size_t j=0;j<p.samplers.size();++j)test::check(p.samplers[j].input==q.samplers[j].input && p.samplers[j].output==q.samplers[j].output && p.samplers[j].interpolation==q.samplers[j].interpolation,"samplers");for(std::size_t j=0;j<p.channels.size();++j)test::check(p.channels[j].sampler==q.channels[j].sampler && p.channels[j].node==q.channels[j].node && p.channels[j].path==q.channels[j].path,"channels");}
    for(std::size_t i=0;i<x.scenes.size();++i)test::check(x.scenes[i].roots==y.scenes[i].roots,"scenes");
    for(std::size_t i=0;i<x.images.size();++i)test::check(x.images[i].width==y.images[i].width && x.images[i].height==y.images[i].height && x.images[i].rgba==y.images[i].rgba,"decoded GLB images");
    const auto &xd=a.metadata_document(),&yd=b.metadata_document();test::check(xd.nodes.size()==yd.nodes.size(),"metadata tree count");
    for(std::size_t i=0;i<xd.nodes.size();++i) {const auto &p=xd.nodes[i],&q=yd.nodes[i];test::check(p.kind==q.kind && p.number==q.number && p.boolean==q.boolean && p.text==q.text && p.key==q.key && p.children==q.children,"complete typed metadata");}
    ars::rendering::PreparedScene3D p,q;
    test::check(ars::rendering::prepare_static_3d(std::shared_ptr<const GlbModel>(&a,[](auto*){}),p)==ARS_OK && ars::rendering::prepare_static_3d(std::shared_ptr<const GlbModel>(&b,[](auto*){}),q)==ARS_OK && p.draws.size()==q.draws.size() && p.cameras.size()==q.cameras.size() && p.estimated_gpu_bytes==q.estimated_gpu_bytes,"prepared 3D scene");
    for(std::size_t i=0;i<p.draws.size();++i) {const auto &u=p.draws[i],&v=q.draws[i];test::check(u.node==v.node && u.primitive==v.primitive && u.indices==v.indices && u.vertices.size()==v.vertices.size(),"prepared 3D draw");for(std::size_t j=0;j<u.vertices.size();++j)test::check(u.vertices[j].position==v.vertices[j].position && u.vertices[j].normal==v.vertices[j].normal && u.vertices[j].tangent==v.vertices[j].tangent && u.vertices[j].uv==v.vertices[j].uv && u.vertices[j].color==v.vertices[j].color,"prepared 3D vertex");}
}
void equal(const ValidatedCpuModel &a,const ValidatedCpuModel &b) {
    if (b.complete_hierarchy()) {
        test::check(a.complete_hierarchy() && a.default_mode()==b.default_mode(),"original default mode");
        for(std::size_t i=0;i<a.layers().size();++i) {const auto &x=a.layers()[i],&y=b.layers()[i];test::check(x.parent==y.parent && x.local_visible==y.local_visible && x.local_opacity==y.local_opacity,"original layer hierarchy controls");}
    }
    test::check(a.canvas()==b.canvas() && a.image().width==b.image().width && a.image().height==b.image().height && a.image().rgba==b.image().rgba && bool(a.glb())==bool(b.glb()),"canvas/complete decoded atlas identity");
    test::check(a.layers().size()==b.layers().size() && a.parameters().size()==b.parameters().size() && a.semantics().size()==b.semantics().size() && a.parts().size()==b.parts().size(),"all model counts");
    for(std::size_t i=0;i<a.layers().size();++i) {const auto &x=a.layers()[i],&y=b.layers()[i];test::check(x.id==y.id && x.z==y.z && x.visible==y.visible && x.opacity==y.opacity,"complete flattened layers");}
    for(std::size_t i=0;i<a.parameters().size();++i) {const auto &x=a.parameters()[i],&y=b.parameters()[i];test::check(x.id==y.id && x.minimum==y.minimum && x.maximum==y.maximum && x.initial==y.initial,"all parameter fields");}
    for(std::size_t i=0;i<a.semantics().size();++i) {
        const auto &x=a.semantics()[i],&y=b.semantics()[i];
        test::check(x.id==y.id && x.type==y.type && x.units==y.units && x.target2d==y.target2d && x.target3d==y.target3d && x.target3d==y.target3d && x.initial==y.initial && x.minimum==y.minimum && x.maximum==y.maximum && x.has_minimum==y.has_minimum && x.has_maximum==y.has_maximum && x.has_default==y.has_default,"all semantic fields and f64 values");
    }
    for(std::size_t i=0;i<a.parts().size();++i) {
        const auto &x=a.parts()[i],&y=b.parts()[i];
        test::check(x.id==y.id && x.rect==y.rect && x.pivot.x==y.pivot.x && x.pivot.y==y.pivot.y && x.z==y.z && x.layer==y.layer && pairs(x.vertices,y.vertices) && pairs(x.uv,y.uv) && x.triangles==y.triangles && x.deforms.size()==y.deforms.size(),"all part fields/UV/indices");
        for(std::size_t j=0;j<x.deforms.size();++j) test::check(x.deforms[j].parameter==y.deforms[j].parameter && pairs(x.deforms[j].deltas,y.deforms[j].deltas),"all deform mappings/deltas");
    }
    if (!a.parts().empty()) {
    ars::rendering::PreparedScene2D x,y;
    test::check(ars::rendering::prepare_static_2d(ars::rendering::scene_input(a),x)==ARS_OK && ars::rendering::prepare_static_2d(ars::rendering::scene_input(b),y)==ARS_OK && x.draws.size()==y.draws.size(),"native scene oracle");
    for(std::size_t i=0;i<x.draws.size();++i) {
        const auto &p=x.draws[i],&q=y.draws[i];
        test::check(p.layer_index==q.layer_index && p.part_index==q.part_index && p.opacity==q.opacity && p.atlas_rect==q.atlas_rect && pairs(p.vertices,q.vertices) && pairs(p.uv,q.uv) && p.triangles==q.triangles,"prepared geometry/effective opacity/order parity");
    }
    }
    if(a.glb()) compare_glb(*a.glb(),*b.glb());
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
