#include "committed_scene.hpp"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <string_view>

namespace ars::rendering {
namespace {
using Q=std::array<double,4>;
Q multiply(Q a,Q b) {
    return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
        a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
        a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
        a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};
}
Q normalized(Q q) {
    double length=0;for(auto x:q)length+=x*x;
    assets::require(std::isfinite(length)&&length>1e-12,ARS_E_UNSUPPORTED);
    for(auto &x:q)x/=std::sqrt(length);return q;
}
std::array<double,4> values(const motion::Value &v) {
    std::array<double,4> result{v.scalar,v.tail[0],v.tail[1],v.tail[2]};
    for(auto x:result)assets::require(std::isfinite(x),ARS_E_UNSUPPORTED);
    return result;
}
}
ars_status prepare_committed_2d(const assets::ValidatedCpuModel &model,
    std::span<const motion::Value> pose,std::span<const LayerControl> controls,
    PreparedScene2D &out) noexcept {
    if(pose.size()!=model.semantics().size()||controls.size()!=model.layers().size()||controls.size()>128)
        return ARS_E_INVALID_ARGUMENT;
    try {
        assets::Work work{8'000'000};
        std::vector<float> parameters;parameters.reserve(model.parameters().size());
        std::vector<bool> mapped(model.parameters().size(),false);
        for(const auto &p:model.parameters())parameters.push_back(p.initial);
        for(std::size_t i=0;i<pose.size();++i){
            const auto &s=model.semantics()[i];if(s.target2d.empty())continue;
            for(std::size_t j=0;j<parameters.size();++j){
                work.take();if(model.parameters()[j].id!=s.target2d)continue;
                if(mapped[j])return ARS_E_UNSUPPORTED;
                if(s.type!="float"||!std::isfinite(pose[i].scalar))return ARS_E_UNSUPPORTED;
                mapped[j]=true;parameters[j]=pose[i].scalar;
            }
        }
        std::vector<assets::Layer> layers;layers.reserve(controls.size());
        for(std::size_t i=0;i<controls.size();++i){
            layers.emplace_back(std::pmr::get_default_resource());
            auto &l=layers.back();l.z=controls[i].z;l.visible=true;l.opacity=1;
            std::uint32_t node=static_cast<std::uint32_t>(i);std::size_t depth=0;
            while(node!=UINT32_MAX){
                work.take();if(node>=controls.size()||++depth>controls.size())return ARS_E_INVALID_ARGUMENT;
                const auto &c=controls[node];
                if(!std::isfinite(c.opacity)||c.opacity<0||c.opacity>1)return ARS_E_INVALID_ARGUMENT;
                l.visible=l.visible&&c.visible;l.opacity*=c.opacity;node=model.layers()[node].parent;
            }
        }
        PreparedScene2D next;
        auto status=prepare_static_2d({layers,model.parts(),model.canvas(),&model.image()},next);
        if(status!=ARS_OK)return status;
        for(auto &draw:next.draws){
            const auto &part=model.parts()[draw.part_index];
            for(const auto &deform:part.deforms){
                if(deform.parameter>=parameters.size()||deform.deltas.size()!=draw.vertices.size())return ARS_E_BAD_ASSET;
                const double offset=double(parameters[deform.parameter])-model.parameters()[deform.parameter].initial;
                for(std::size_t i=0;i<draw.vertices.size();++i){
                    work.take();auto &v=draw.vertices[i];
                    const double x=v.x+deform.deltas[i].x*offset,y=v.y+deform.deltas[i].y*offset;
                    if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>FLT_MAX||std::abs(y)>FLT_MAX)return ARS_E_UNSUPPORTED;
                    v={static_cast<float>(x),static_cast<float>(y)};
                }
            }
        }
        out=std::move(next);return ARS_OK;
    }catch(const assets::Failure &f){return f.status;}
    catch(const std::bad_alloc &){return ARS_E_OUT_OF_BUDGET;}
    catch(...){return ARS_E_INTERNAL;}
}
ars_status prepare_committed_3d(const assets::ValidatedCpuModel &model,
    std::span<const motion::Value> pose,PreparedScene3D &out) noexcept {
    if(pose.size()!=model.semantics().size())return ARS_E_INVALID_ARGUMENT;
    if(!model.glb())return ARS_E_UNSUPPORTED;
    try {
        const auto &data=model.glb()->data();assets::Work work{8'000'000};
        std::vector<NodePose3D> overlays;
        struct Mapping {unsigned properties=0;std::array<double,3> angles{};std::vector<bool> morphs;};
        std::vector<Mapping> mappings;
        auto node_pose=[&](std::uint32_t node)->std::size_t{
            for(std::size_t i=0;i<overlays.size();++i)if(overlays[i].node==node)return i;
            overlays.emplace_back();overlays.back().node=node;mappings.emplace_back();return overlays.size()-1;
        };
        for(std::size_t i=0;i<pose.size();++i){
            const auto &semantic=model.semantics()[i];const std::string_view target=semantic.target3d;
            if(target.empty())continue;auto v=values(pose[i]);bool found=false;
            if(target.starts_with("bone:")){
                const auto dot=target.rfind('.');if(dot==target.npos)return ARS_E_UNSUPPORTED;
                const auto name=target.substr(5,dot-5),property=target.substr(dot+1);
                for(std::uint32_t n=0;n<data.nodes.size();++n){
                    work.take();if(std::string_view(data.nodes[n].name)!=name)continue;
                    if(found)return ARS_E_UNSUPPORTED;found=true;
                    const auto p=node_pose(n);auto &o=overlays[p];auto &m=mappings[p];const auto &base=data.nodes[n];
                    if(base.has_matrix)return ARS_E_UNSUPPORTED;
                    unsigned bit=property=="yaw"?1:property=="pitch"?2:property=="roll"?4:
                        property=="rotation"?8:property=="translation"?16:property=="scale"?32:0;
                    if(!bit||(m.properties&bit)||((bit&7)&&(m.properties&8))||(bit==8&&(m.properties&7)))return ARS_E_UNSUPPORTED;
                    m.properties|=bit;
                    if(bit&7)m.angles[bit==1?0:bit==2?1:2]=v[0]-semantic.initial[0];
                    else if(bit==8){
                        auto initial=normalized(semantic.initial);for(int k=0;k<3;++k)initial[k]=-initial[k];
                        o.rotation=normalized(multiply(base.rotation,multiply(normalized(v),initial)));o.rotation_set=true;
                    }else{
                        auto &destination=bit==16?o.translation:o.scale;const auto &authored=bit==16?base.translation:base.scale;
                        for(int k=0;k<3;++k)destination[k]=authored[k]+v[k]-semantic.initial[k];
                        if(bit==16)o.translation_set=true;else o.scale_set=true;
                    }
                }
            }else if(target.starts_with("morph:")){
                const auto name=target.substr(6);
                for(std::uint32_t n=0;n<data.nodes.size();++n){
                    const auto &base=data.nodes[n];if(base.mesh==assets::glb_none)continue;
                    const auto &mesh=data.meshes[base.mesh];
                    for(std::size_t t=0;t<mesh.target_names.size();++t){
                        work.take();if(std::string_view(mesh.target_names[t])!=name)continue;found=true;
                        const auto p=node_pose(n);auto &o=overlays[p];auto &m=mappings[p];
                        if(o.weights.empty()){
                            o.weights.assign(mesh.target_names.size(),0);m.morphs.resize(mesh.target_names.size(),false);
                            const auto &weights=base.weights.empty()?mesh.weights:base.weights;
                            std::copy(weights.begin(),weights.end(),o.weights.begin());
                        }
                        if(m.morphs[t])return ARS_E_UNSUPPORTED;m.morphs[t]=true;o.weights[t]=static_cast<float>(v[0]);
                    }
                }
            }else return ARS_E_UNSUPPORTED;
            if(!found)return ARS_E_UNSUPPORTED;
        }
        for(std::size_t i=0;i<overlays.size();++i)if(mappings[i].properties&7){
            auto &o=overlays[i];const auto &a=mappings[i].angles;
            Q yaw{0,std::sin(a[0]/2),0,std::cos(a[0]/2)},pitch{std::sin(a[1]/2),0,0,std::cos(a[1]/2)},roll{0,0,std::sin(a[2]/2),std::cos(a[2]/2)};
            o.rotation=normalized(multiply(data.nodes[o.node].rotation,multiply(multiply(yaw,pitch),roll)));o.rotation_set=true;
        }
        return prepare_pose_3d(model.glb(),out,overlays);
    }catch(const assets::Failure &f){return f.status;}
    catch(const std::bad_alloc &){return ARS_E_OUT_OF_BUDGET;}
    catch(...){return ARS_E_INTERNAL;}
}
}
