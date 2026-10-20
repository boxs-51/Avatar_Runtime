#include "retained_physics.hpp"
#include "../validation/strict_json.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
namespace ars::moc3::admission {
namespace {
using assets::require;
using validation::Value;
constexpr std::uint64_t step_ns=16666667;
struct Vec {double x=0,y=0;};
struct Point {Vec position,velocity;};
using Range=std::array<double,3>;
struct Binding {std::size_t slot,vertex;std::string axis;double weight,scale;bool reflect;};
struct Particle {Vec position;double mobility,delay,acceleration,radius;};
struct Chain {std::vector<Binding> inputs,outputs;std::vector<Particle> particles;Range position,angle;};
Vec vec(Value value){return {value.at("X").number(),value.at("Y").number()};}
Range range(Value value){return {value.at("Minimum").number(),value.at("Default").number(),value.at("Maximum").number()};}
double checked(double value){require(std::isfinite(value)&&std::abs(value)<=1e6);return value;}
double normalized(const ParameterInfo& p,double value,const Range& r){
    if(value<p.default_value&&p.default_value>p.minimum)return r[1]+(value-p.default_value)*(r[1]-r[0])/(double(p.default_value)-p.minimum);
    if(value>p.default_value&&p.maximum>p.default_value)return r[1]+(value-p.default_value)*(r[2]-r[1])/(double(p.maximum)-p.default_value);
    return r[1];
}
}
struct RetainedPhysics::State {std::uint64_t timestamp=0,remainder=0;std::vector<std::vector<Point>> particles;};
struct RetainedPhysics::Data {
    std::vector<ParameterInfo> catalog;std::vector<Chain> chains;Vec gravity,wind;
    std::shared_ptr<State> rest() const {
        auto out=std::make_shared<State>();
        for(auto& c:chains){std::vector<Point> points;for(auto& p:c.particles)points.push_back({p.position,{}});out->particles.push_back(std::move(points));}
        return out;
    }
};
RetainedPhysics::~RetainedPhysics()=default;
RetainedPhysics::RetainedPhysics(std::span<const std::uint8_t> bytes,const std::vector<ParameterInfo>& catalog,
                                 assets::Resource& resource,const assets::Limits& limits,assets::Work& work)
    :data_(std::make_unique<Data>()),owner_(std::this_thread::get_id()){
    // Never construct executable chains from merely syntax-checked JSON.
    validate_physics(bytes,catalog,resource,limits,work);
    validation::Document doc(&resource);validation::parse_json({reinterpret_cast<const char*>(bytes.data()),bytes.size()},doc,limits,work);
    data_->catalog=catalog;auto root=doc.root();auto forces=root.at("Meta").at("EffectiveForces");
    auto g=vec(forces.at("Gravity"));auto length=std::hypot(g.x,g.y);data_->gravity={g.x/length,-g.y/length};data_->wind=vec(forces.at("Wind"));
    auto groups=root.at("PhysicsSettings");
    for(std::size_t i=0;i<groups.size();++i){
        auto group=groups.at(i);Chain chain;auto n=group.at("Normalization");chain.position=range(n.at("Position"));chain.angle=range(n.at("Angle"));
        auto vertices=group.at("Vertices");
        for(std::size_t j=0;j<vertices.size();++j){auto p=vertices.at(j);chain.particles.push_back({vec(p.at("Position")),p.at("Mobility").number(),p.at("Delay").number(),p.at("Acceleration").number(),p.at("Radius").number()});}
        for(bool output:{false,true}){
            auto entries=group.at(output?"Output":"Input");auto& destination=output?chain.outputs:chain.inputs;
            for(std::size_t j=0;j<entries.size();++j){auto e=entries.at(j);auto id=e.at(output?"Destination":"Source").at("Id").string();
                auto found=std::find_if(catalog.begin(),catalog.end(),[&](auto& p){return p.id==id;});require(found!=catalog.end());
                destination.push_back({static_cast<std::size_t>(found-catalog.begin()),output?e.at("VertexIndex").u32():0,std::string(e.at("Type").string()),
                                       e.at("Weight").number()/100,output?e.at("Scale").number():1,e.at("Reflect").boolean()});}
        }
        data_->chains.push_back(std::move(chain));
    }
    state_=data_->rest();
}
RetainedPhysics::Candidate RetainedPhysics::prepare(std::uint64_t timestamp,std::span<const Parameter> manual,bool enabled,bool reset) const {
    require(owner_==std::this_thread::get_id()&&timestamp<=1000000000ull&&(reset||timestamp>=state_->timestamp));
    std::vector<double> base;for(auto& p:data_->catalog)base.push_back(p.default_value);std::set<std::string> seen;
    for(auto& p:manual){auto found=std::find_if(data_->catalog.begin(),data_->catalog.end(),[&](auto& c){return c.id==p.id;});
        require(found!=data_->catalog.end()&&seen.insert(p.id).second&&std::isfinite(p.value)&&p.value>=found->minimum&&p.value<=found->maximum);
        base[found-data_->catalog.begin()]=p.value;}
    auto elapsed=reset?0:timestamp-state_->timestamp;auto remainder=reset?0:state_->remainder;
    require(elapsed+remainder<=8*step_ns);
    auto state=reset||!enabled?data_->rest():std::make_shared<State>(*state_);unsigned steps=0;
    if(enabled){
        steps=static_cast<unsigned>((elapsed+remainder)/step_ns);remainder=(elapsed+remainder)%step_ns;
        for(unsigned step=0;step<steps;++step)for(std::size_t k=0;k<data_->chains.size();++k){
            auto& chain=data_->chains[k];double x=0,y=0,angle=0;
            for(auto& b:chain.inputs){auto value=normalized(data_->catalog[b.slot],base[b.slot],b.axis=="Angle"?chain.angle:chain.position)*b.weight;
                if(b.reflect)value=-value;if(b.axis=="X")x+=value;else if(b.axis=="Y")y+=value;else angle+=value;}
            auto radians=-angle*(std::numbers::pi/180);auto gx=data_->gravity.x*std::cos(radians)-data_->gravity.y*std::sin(radians);
            auto gy=data_->gravity.x*std::sin(radians)+data_->gravity.y*std::cos(radians);auto& points=state->particles[k];
            points[0]={{chain.particles[0].position.x+x,chain.particles[0].position.y+y},{}};
            checked(points[0].position.x);checked(points[0].position.y);
            for(std::size_t j=1;j<chain.particles.size();++j){
                auto& p=chain.particles[j];auto old=points[j];auto parent=points[j-1].position;auto delay=(step_ns/1e9)*p.delay*30;
                auto nx=old.position.x+old.velocity.x*delay+(gx*p.acceleration+data_->wind.x)*delay*delay;
                auto ny=old.position.y+old.velocity.y*delay+(gy*p.acceleration+data_->wind.y)*delay*delay;
                auto dx=nx-parent.x,dy=ny-parent.y,length=std::hypot(dx,dy);if(length<1e-12){dx=gx;dy=gy;length=1;}
                Vec pos{checked(parent.x+dx/length*p.radius),checked(parent.y+dy/length*p.radius)};
                Vec vel{checked(delay?(pos.x-old.position.x)/delay*p.mobility:0),checked(delay?(pos.y-old.position.y)/delay*p.mobility:0)};
                points[j]={pos,vel};
            }
        }
        for(std::size_t k=0;k<data_->chains.size();++k){auto& chain=data_->chains[k];auto& points=state->particles[k];
            for(auto& b:chain.outputs){auto p=points[b.vertex].position,parent=points[b.vertex-1].position;auto dx=p.x-parent.x,dy=p.y-parent.y;
                auto value=b.axis=="Angle"?std::atan2(dx,dy)*(180/std::numbers::pi):b.axis=="X"?dx:dy;
                value*=b.scale*(b.reflect?-1:1);value=base[b.slot]*(1-b.weight)+value*b.weight;
                auto& bounds=data_->catalog[b.slot];base[b.slot]=std::clamp(checked(value),double(bounds.minimum),double(bounds.maximum));}
        }
    }else remainder=0;
    state->timestamp=timestamp;state->remainder=remainder;Candidate out;out.owner_=this;out.generation_=generation_+1;out.state_=std::move(state);out.steps_=steps;
    for(std::size_t i=0;i<base.size();++i)out.parameters_.push_back({data_->catalog[i].id,static_cast<float>(checked(base[i]))});
    return out;
}
void RetainedPhysics::commit(const Candidate& candidate){
    require(owner_==std::this_thread::get_id()&&candidate.owner_==this&&candidate.generation_==generation_+1&&bool(candidate.state_));
    state_=candidate.state_;generation_=candidate.generation_;
}
PhysicsCheckpoint RetainedPhysics::checkpoint(const Candidate& candidate,const Digest& model) const {
    require(owner_==std::this_thread::get_id()&&candidate.owner_==this&&candidate.generation_==generation_+1&&bool(candidate.state_));
    PhysicsCheckpoint out;out.model=model;out.generation=candidate.generation_;out.clock=candidate.state_->timestamp;out.remainder=candidate.state_->remainder;
    for(auto& chain:candidate.state_->particles){std::vector<std::array<double,4>> points;for(auto& p:chain)points.push_back({p.position.x,p.position.y,p.velocity.x,p.velocity.y});out.particles.push_back(std::move(points));}return out;
}
void RetainedPhysics::restore(const PhysicsCheckpoint& checkpoint){
    require(owner_==std::this_thread::get_id()&&generation_==0&&checkpoint.generation>0&&checkpoint.generation<8&&checkpoint.clock<=1000000000ull&&checkpoint.remainder<16666667&&checkpoint.particles.size()==data_->chains.size());
    auto state=std::make_shared<State>();state->timestamp=checkpoint.clock;state->remainder=checkpoint.remainder;
    for(std::size_t i=0;i<checkpoint.particles.size();++i){auto& chain=checkpoint.particles[i];require(chain.size()==data_->chains[i].particles.size());std::vector<Point> points;
        for(auto& p:chain){for(auto value:p)checked(value);points.push_back({{p[0],p[1]},{p[2],p[3]}});}state->particles.push_back(std::move(points));}
    state_=std::move(state);generation_=checkpoint.generation;
}
void RetainedPhysics::restore_epoch(const PhysicsCheckpoint& origin){
    require(owner_==std::this_thread::get_id()&&generation_==0&&origin.generation>0&&origin.generation<=8&&origin.clock<=1000000000ull);
    auto rebased=origin;rebased.generation=1;rebased.clock=0;
    restore(rebased);generation_=0;
}

}
