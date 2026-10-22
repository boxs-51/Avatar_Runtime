#include "package_admission.hpp"
#include "../validation/strict_json.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
namespace ars::moc3::admission {
namespace {
using validation::Value;using validation::Kind;using assets::require;
void object(const validation::Document& doc,Value v,std::initializer_list<std::string_view> required,
            std::initializer_list<std::string_view> optional={}){
    require(v.kind()==Kind::object);for(auto k:required)require(v.has(k));
    for(auto i:v.node().children){auto k=std::string_view(doc.nodes[i].key);require(std::find(required.begin(),required.end(),k)!=required.end()||std::find(optional.begin(),optional.end(),k)!=optional.end());}
}
double number(Value v,double lo=-1e6,double hi=1e6){auto x=v.number();require(std::isfinite(x)&&x>=lo&&x<=hi);return x;}
void array(Value v,std::size_t lo,std::size_t hi){require(v.kind()==Kind::array&&v.size()>=lo&&v.size()<=hi);}
std::string id(Value v){auto x=v.string();require(!x.empty()&&x.size()<=255);for(unsigned char c:x)require(c>32&&c<127);return std::string(x);}
void vector(const validation::Document& d,Value v){object(d,v,{"X","Y"});number(v.at("X"),-1e4,1e4);number(v.at("Y"),-1e4,1e4);}
}
void validate_physics(std::span<const std::uint8_t> bytes,const std::vector<ParameterInfo>& catalog,
                      assets::Resource& resource,const assets::Limits& limits,assets::Work& work){
    require(!bytes.empty()&&bytes.size()<=limits.json&&!catalog.empty()&&catalog.size()<=4096);
    std::set<std::string> parameters;
    for(auto& p:catalog){require(!p.id.empty()&&p.id.size()<=255);for(unsigned char c:p.id)require(c>32&&c<127);
        require(std::isfinite(p.minimum)&&std::isfinite(p.default_value)&&std::isfinite(p.maximum)&&p.minimum>=-1e6&&p.maximum<=1e6&&p.minimum<=p.default_value&&p.default_value<=p.maximum);
        require(parameters.insert(p.id).second);
    }
    validation::Document doc(&resource);validation::parse_json({reinterpret_cast<const char*>(bytes.data()),bytes.size()},doc,limits,work);
    auto root=doc.root();object(doc,root,{"Version","Meta","PhysicsSettings"});require(root.at("Version").u32()==3);
    auto meta=root.at("Meta");object(doc,meta,{"PhysicsSettingCount","TotalInputCount","TotalOutputCount","VertexCount","EffectiveForces"},{"Fps","PhysicsDictionary"});
    if(meta.has("Fps"))number(meta.at("Fps"),1,240);
    // Dictionary is descriptive, bounded by the same strict JSON/node limits; never executed.
    auto forces=meta.at("EffectiveForces");object(doc,forces,{"Gravity","Wind"});vector(doc,forces.at("Gravity"));vector(doc,forces.at("Wind"));
    auto gravity=forces.at("Gravity");require(std::hypot(gravity.at("X").number(),gravity.at("Y").number())>0);
    auto groups=root.at("PhysicsSettings");array(groups,1,128);std::set<std::string> group_ids;std::size_t ni=0,no=0,nv=0;
    for(std::size_t g=0;g<groups.size();++g){
        work.take();auto group=groups.at(g);object(doc,group,{"Id","Input","Output","Vertices","Normalization"});require(group_ids.insert(id(group.at("Id"))).second);
        auto norm=group.at("Normalization");object(doc,norm,{"Position","Angle"});
        for(auto key:{"Position","Angle"}){auto v=norm.at(key);object(doc,v,{"Minimum","Default","Maximum"});auto a=number(v.at("Minimum"),-1e4,1e4),b=number(v.at("Default"),-1e4,1e4),c=number(v.at("Maximum"),-1e4,1e4);require(a<=b&&b<=c);}
        auto particles=group.at("Vertices");array(particles,2,2048);nv+=particles.size();require(nv<=2048);
        for(std::size_t j=0;j<particles.size();++j){auto p=particles.at(j);object(doc,p,{"Position","Mobility","Delay","Acceleration","Radius"});vector(doc,p.at("Position"));number(p.at("Mobility"),0,1);number(p.at("Delay"),0,1);number(p.at("Acceleration"),0,100);auto radius=number(p.at("Radius"),0,1e4);require(j==0||radius>0);}
        for(bool output:{false,true}){
            auto entries=group.at(output?"Output":"Input");array(entries,1,8192);std::set<std::string> seen;
            (output?no:ni)+=entries.size();require(ni+no<=8192);
            for(std::size_t j=0;j<entries.size();++j){
                auto e=entries.at(j);if(output)object(doc,e,{"Destination","Weight","Type","Reflect","VertexIndex","Scale"});else object(doc,e,{"Source","Weight","Type","Reflect"});
                auto ref=e.at(output?"Destination":"Source");object(doc,ref,{"Target","Id"});require(ref.at("Target").string()=="Parameter");auto key=id(ref.at("Id"));require(parameters.contains(key));
                auto axis=e.at("Type").string();require(axis=="X"||axis=="Y"||axis=="Angle");require(seen.insert(output?key:key+":"+std::string(axis)).second);
                number(e.at("Weight"),0,100);e.at("Reflect").boolean();
                if(output){auto index=e.at("VertexIndex").u32();require(index>0&&index<particles.size());number(e.at("Scale"));}
            }
        }
    }
    require(meta.at("PhysicsSettingCount").u32()==groups.size()&&meta.at("TotalInputCount").u32()==ni&&meta.at("TotalOutputCount").u32()==no&&meta.at("VertexCount").u32()==nv);
}
}
