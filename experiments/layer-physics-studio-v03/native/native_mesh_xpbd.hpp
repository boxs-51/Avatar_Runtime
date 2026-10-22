#pragma once
// Nonshipping native2D XPBD overlay for an already validated, committed scene.
// Header intentionally depends only on the shape of PreparedScene2D: it never
// parses assets, touches runtime handles, calls D3D11, or publishes a GPU frame.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <utility>
#include <vector>

namespace ars::experimental::lps {
struct Endpoint {std::uint32_t part=0, vertex=0;};
struct Joint {Endpoint a,b;double compliance=0;};
struct Selection {std::uint32_t part=0,pin_vertex=0;double mass=1;};
struct Options {
    double dt=1.0/120.0,gravity_y=200,wind_x=0,edge_compliance=1e-7;
    double contact_radius=0,contact_compliance=0;
    unsigned iterations=8;
    std::vector<Selection> dynamic_parts;
    std::vector<Joint> joints;
};
enum class Status {ok,invalid,stale,out_of_budget};
struct Node {double x=0,y=0,previous_x=0,previous_y=0,source_x=0,source_y=0,inverse_mass=0;std::uint32_t draw=0,vertex=0,part=0;};
struct Edge {std::size_t a=0,b=0;double rest=0,compliance=0;};
template<class Scene> class Session {
    struct Shape {std::uint32_t part=0;std::size_t count=0;};
    Options options_{};
    std::vector<Shape> shape_;
    std::vector<Node> accepted_;
    std::vector<Edge> links_;
    std::uint64_t identity_=0,revision_=0;
    bool ready_=false;
    static bool finite(double n) {return std::isfinite(n) && std::abs(n)<=1e7;}
    static void satisfy(std::vector<Node>&nodes,const Edge&link,double dt,double &lambda){
        auto &a=nodes[link.a],&b=nodes[link.b];const double dx=b.x-a.x,dy=b.y-a.y;
        const double length=std::hypot(dx,dy),weight=a.inverse_mass+b.inverse_mass;
        if(length<1e-12 || weight==0)return;
        const double alpha=link.compliance/(dt*dt);
        const double dlambda=(-(length-link.rest)-alpha*lambda)/(weight+alpha);lambda+=dlambda;
        a.x-=a.inverse_mass*dlambda*dx/length;a.y-=a.inverse_mass*dlambda*dy/length;
        b.x+=b.inverse_mass*dlambda*dx/length;b.y+=b.inverse_mass*dlambda*dy/length;
    }
public:
    struct Candidate {Scene scene;std::vector<Node> state;std::uint64_t identity=0,revision=0;};
    // Binding is explicit, unique per validated model/character revision.
    // Failure does not mutate the current accepted state.
    Status bind(const Scene&scene,const Options&opt,std::uint64_t identity){
        if(!identity||!finite(opt.dt)||opt.dt<=0||opt.dt>1.0/30||
           !finite(opt.gravity_y)||!finite(opt.wind_x)||!finite(opt.edge_compliance)||
           !finite(opt.contact_radius)||!finite(opt.contact_compliance)||
           opt.edge_compliance<0||opt.contact_radius<0||opt.contact_radius>100||
           opt.contact_compliance<0||opt.iterations<1||opt.iterations>24||
           opt.dynamic_parts.size()>128||opt.joints.size()>128||scene.draws.size()>128)
            return Status::invalid;
        std::set<std::uint32_t> seen_parts,seen_selected;
        std::vector<Shape> shapes;std::vector<Node> nodes;std::vector<Edge> edges;
        for(std::size_t i=0;i<scene.draws.size();++i){
            const auto &d=scene.draws[i];if(!seen_parts.insert(d.part_index).second||d.vertices.size()>2048||
                nodes.size()+d.vertices.size()>2048||d.vertices.size()!=d.uv.size())return Status::out_of_budget;
            shapes.push_back({d.part_index,d.vertices.size()});
            double inverse=0;std::size_t pinned=std::numeric_limits<std::size_t>::max();
            for(auto s:opt.dynamic_parts)if(s.part==d.part_index){
                if(!seen_selected.insert(s.part).second||!finite(s.mass)||s.mass<=0||s.mass>100||
                   s.pin_vertex>=d.vertices.size())return Status::invalid;
                inverse=1.0/s.mass;pinned=s.pin_vertex;
            }
            const std::size_t start=nodes.size();
            for(std::size_t j=0;j<d.vertices.size();++j){
                const auto &v=d.vertices[j];if(!finite(v.x)||!finite(v.y))return Status::invalid;
                nodes.push_back({v.x,v.y,v.x,v.y,v.x,v.y,j==pinned?0:inverse,
                    static_cast<std::uint32_t>(i),static_cast<std::uint32_t>(j),d.part_index});
            }
            std::set<std::pair<std::size_t,std::size_t>> unique;
            for(const auto &tri:d.triangles){
                for(int k=0;k<3;++k){auto a=std::size_t(tri[k]),b=std::size_t(tri[(k+1)%3]);
                    if(a>=d.vertices.size()||b>=d.vertices.size()||a==b)return Status::invalid;
                    if(a>b)std::swap(a,b);
                    if(inverse && unique.insert({a,b}).second){
                        const auto &p=nodes[start+a],&q=nodes[start+b];
                        edges.push_back({start+a,start+b,std::hypot(q.x-p.x,q.y-p.y),opt.edge_compliance});
                    }
                }
            }
        }
        if(seen_selected.size()!=opt.dynamic_parts.size())return Status::invalid;
        auto locate=[&](Endpoint e)->std::size_t{
            for(std::size_t i=0;i<nodes.size();++i)if(nodes[i].part==e.part&&nodes[i].vertex==e.vertex)return i;
            return nodes.size();
        };
        for(auto j:opt.joints){
            const auto a=locate(j.a),b=locate(j.b);
            if(a==nodes.size()||b==nodes.size()||a==b||nodes[a].part==nodes[b].part||
               !finite(j.compliance)||j.compliance<0)return Status::invalid;
            edges.push_back({a,b,std::hypot(nodes[a].x-nodes[b].x,nodes[a].y-nodes[b].y),j.compliance});
        }
        if(edges.size()>12000)return Status::out_of_budget;
        options_=opt;shape_=std::move(shapes);accepted_=std::move(nodes);links_=std::move(edges);
        identity_=identity;revision_++;ready_=true;return Status::ok;
    }
    Status prepare(const Scene&scene,std::uint64_t identity,Candidate&out)const{
        if(!ready_||identity!=identity_)return Status::stale;
        if(scene.draws.size()!=shape_.size())return Status::stale;
        for(std::size_t i=0;i<shape_.size();++i)
            if(scene.draws[i].part_index!=shape_[i].part||scene.draws[i].vertices.size()!=shape_[i].count)
                return Status::stale;
        Candidate candidate;candidate.scene=scene;candidate.state=accepted_;
        const double dt=options_.dt;
        for(auto &p:candidate.state){
            const auto &v=scene.draws[p.draw].vertices[p.vertex];
            if(!finite(v.x)||!finite(v.y))return Status::invalid;
            const double dx=double(v.x)-p.source_x,dy=double(v.y)-p.source_y;
            p.x+=dx;p.y+=dy;p.previous_x+=dx;p.previous_y+=dy;
            p.source_x=v.x;p.source_y=v.y;
            if(!p.inverse_mass){p.x=v.x;p.y=v.y;p.previous_x=v.x;p.previous_y=v.y;}
        }
        for(auto &p:candidate.state)if(p.inverse_mass){
            const double vx=(p.x-p.previous_x)*0.995,vy=(p.y-p.previous_y)*0.995;
            p.previous_x=p.x;p.previous_y=p.y;
            p.x+=vx+options_.wind_x*dt*dt;p.y+=vy+options_.gravity_y*dt*dt;
        }
        std::vector<double> lambdas(links_.size(),0);
        for(unsigned it=0;it<options_.iterations;++it){
            for(std::size_t e=0;e<links_.size();++e)satisfy(candidate.state,links_[e],dt,lambdas[e]);
            if(options_.contact_radius>0){
                std::size_t checks=0;
                for(std::size_t i=0;i<candidate.state.size();++i)
                for(std::size_t j=i+1;j<candidate.state.size();++j){
                    auto &a=candidate.state[i],&b=candidate.state[j];
                    if(a.part==b.part||!(a.inverse_mass+b.inverse_mass))continue;
                    if(++checks>40000)return Status::out_of_budget;
                    const double dx=b.x-a.x,dy=b.y-a.y,dist=std::hypot(dx,dy);
                    const double gap=2*options_.contact_radius;
                    if(dist>=gap)continue;
                    const double nx=dist>1e-9?dx/dist:1,ny=dist>1e-9?dy/dist:0;
                    const double correction=(gap-dist)/(a.inverse_mass+b.inverse_mass+options_.contact_compliance/(dt*dt));
                    a.x-=a.inverse_mass*correction*nx;a.y-=a.inverse_mass*correction*ny;
                    b.x+=b.inverse_mass*correction*nx;b.y+=b.inverse_mass*correction*ny;
                }
            }
        }
        for(const auto &p:candidate.state){
            if(!finite(p.x)||!finite(p.y)||!finite(p.previous_x)||!finite(p.previous_y))return Status::invalid;
            auto &v=candidate.scene.draws[p.draw].vertices[p.vertex];
            v.x=static_cast<float>(p.x);v.y=static_cast<float>(p.y);
        }
        candidate.identity=identity_;candidate.revision=revision_;out=std::move(candidate);return Status::ok;
    }
    // Caller invokes ONLY after an exact GPU copy query and source generation
    // have both been admitted. No speculative render/failed query may accept.
    Status accept(Candidate&&candidate,std::uint64_t identity){
        if(!ready_||identity!=identity_||candidate.identity!=identity_||candidate.revision!=revision_||
           candidate.state.size()!=accepted_.size())return Status::stale;
        for(const auto &p:candidate.state)if(!finite(p.x)||!finite(p.y))return Status::invalid;
        accepted_=std::move(candidate.state);revision_++;return Status::ok;
    }
    std::uint64_t revision()const{return revision_;}
    std::size_t particles()const{return accepted_.size();}
};
} // namespace ars::experimental::lps
