#pragma once
// Nonshipping native2D XPBD overlay for an already validated, committed scene.
// Header intentionally depends only on the shape of PreparedScene2D: it never
// parses assets, touches runtime handles, calls D3D11, or publishes a GPU frame.
#include <algorithm>
#include <array>
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
    double contact_radius=0,contact_compliance=0,damping=.995;
    double pose_compliance=.002,source_motion_transfer=.10; unsigned substeps=2;
    double bending_compliance=0,surface_thickness=0; std::vector<Endpoint> anchors;
    unsigned iterations=8;
    std::vector<Selection> dynamic_parts;
    std::vector<Joint> joints;std::vector<Joint> extra_beams;
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
    std::vector<std::array<std::size_t,3>> faces_;
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
           !finite(opt.contact_radius)||!finite(opt.contact_compliance)||!finite(opt.damping)||opt.damping<0||opt.damping>1||
           opt.edge_compliance<0||opt.contact_radius<0||opt.contact_radius>100||
           opt.contact_compliance<0||!finite(opt.pose_compliance)||opt.pose_compliance<0||opt.pose_compliance>1||!finite(opt.source_motion_transfer)||opt.source_motion_transfer<0||opt.source_motion_transfer>1||opt.substeps<1||opt.substeps>8||opt.iterations<1||opt.iterations>24||
           !finite(opt.bending_compliance)||opt.bending_compliance<0||opt.bending_compliance>1||!finite(opt.surface_thickness)||opt.surface_thickness<0||opt.surface_thickness>10||opt.anchors.size()>256||opt.dynamic_parts.size()>128||opt.extra_beams.size()>256||opt.joints.size()>128||scene.draws.size()>128)
            return Status::invalid;
        std::set<std::uint32_t> seen_parts,seen_selected;
        std::vector<Shape> shapes;std::vector<Node> nodes;std::vector<Edge> edges;
        std::vector<std::array<std::size_t,3>> faces;
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
            std::vector<std::vector<std::size_t>> neighbors(d.vertices.size());
            const std::size_t face_begin=faces.size();
            for(const auto &tri:d.triangles){
                if(tri[0]>=d.vertices.size()||tri[1]>=d.vertices.size()||tri[2]>=d.vertices.size())return Status::invalid;
                faces.push_back({start+tri[0],start+tri[1],start+tri[2]});
                for(int k=0;k<3;++k){auto a=std::size_t(tri[k]),b=std::size_t(tri[(k+1)%3]);
                    if(a>=d.vertices.size()||b>=d.vertices.size()||a==b)return Status::invalid;
                    if(a>b)std::swap(a,b);
                    if(inverse && unique.insert({a,b}).second){
                        neighbors[a].push_back(b);neighbors[b].push_back(a);
                        const auto &p=nodes[start+a],&q=nodes[start+b];
                        edges.push_back({start+a,start+b,std::hypot(q.x-p.x,q.y-p.y),opt.edge_compliance});
                    }
                }
            }
            // Dihedral proxy for 2D cloth: distance between opposite
            // vertices on two triangles sharing a mesh edge.
            if(inverse&&opt.bending_compliance>0)
            for(std::size_t ta=face_begin;ta<faces.size();++ta)
            for(std::size_t tb=ta+1;tb<faces.size();++tb){
                std::size_t oa=nodes.size(),ob=nodes.size();unsigned shared=0;
                for(auto a:faces[ta]){
                    bool hit=false;for(auto b:faces[tb])if(a==b){hit=true;break;}
                    if(hit)++shared;else oa=a;
                }
                for(auto b:faces[tb]){
                    bool hit=false;for(auto a:faces[ta])if(a==b){hit=true;break;}
                    if(!hit)ob=b;
                }
                if(shared==2&&oa<nodes.size()&&ob<nodes.size()){
                    const auto &a=nodes[oa],&b=nodes[ob];
                    edges.push_back({oa,ob,std::hypot(a.x-b.x,a.y-b.y),opt.bending_compliance});
                }
            }
        }
        for(const auto &anchor:opt.anchors){
            bool found=false;
            for(auto &p:nodes)if(p.part==anchor.part && p.vertex==anchor.vertex){p.inverse_mass=0;found=true;break;}
            if(!found)return Status::invalid;
        }        if(seen_selected.size()!=opt.dynamic_parts.size())return Status::invalid;
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
        for(const auto &beam:opt.extra_beams){
          const auto a=locate(beam.a),b=locate(beam.b);
          if(a==nodes.size()||b==nodes.size()||a==b||!finite(beam.compliance)||beam.compliance<0)return Status::invalid;
          edges.push_back({a,b,std::hypot(nodes[a].x-nodes[b].x,nodes[a].y-nodes[b].y),beam.compliance});
        }
        if(edges.size()>12000)return Status::out_of_budget;
        options_=opt;shape_=std::move(shapes);accepted_=std::move(nodes);links_=std::move(edges);faces_=std::move(faces);
        identity_=identity;revision_++;ready_=true;return Status::ok;
    }
    Status prepare(const Scene&scene,std::uint64_t identity,Candidate&out,double action_fx=0,double action_fy=0)const{
        if(!ready_||identity!=identity_)return Status::stale;
        if(scene.draws.size()!=shape_.size())return Status::stale;
        for(std::size_t i=0;i<shape_.size();++i)
            if(scene.draws[i].part_index!=shape_[i].part||scene.draws[i].vertices.size()!=shape_[i].count)
                return Status::stale;
        if(!finite(action_fx)||!finite(action_fy)||std::abs(action_fx)>1000||std::abs(action_fy)>1000)return Status::invalid;
        Candidate candidate;candidate.scene=scene;candidate.state=accepted_;
        const double dt=options_.dt/options_.substeps;
        for(auto &p:candidate.state){
            const auto &v=scene.draws[p.draw].vertices[p.vertex];
            if(!finite(v.x)||!finite(v.y))return Status::invalid;
            const double dx=double(v.x)-p.source_x,dy=double(v.y)-p.source_y;
            p.x+=dx*options_.source_motion_transfer;p.y+=dy*options_.source_motion_transfer;
            p.previous_x+=dx*options_.source_motion_transfer;p.previous_y+=dy*options_.source_motion_transfer;
            p.source_x=v.x;p.source_y=v.y;
            if(!p.inverse_mass){p.x=v.x;p.y=v.y;p.previous_x=v.x;p.previous_y=v.y;}
        }
        for(unsigned sub=0;sub<options_.substeps;++sub){
            for(auto &p:candidate.state)if(p.inverse_mass){
                const double vx=(p.x-p.previous_x)*options_.damping;
                const double vy=(p.y-p.previous_y)*options_.damping;
                p.previous_x=p.x;p.previous_y=p.y;
                p.x+=vx+(options_.wind_x+action_fx)*dt*dt;
                p.y+=vy+(options_.gravity_y+action_fy)*dt*dt;
            }
            std::vector<double> lambdas(links_.size(),0);
            for(unsigned it=0;it<options_.iterations;++it){
                for(std::size_t e=0;e<links_.size();++e)satisfy(candidate.state,links_[e],dt,lambdas[e]);
                // 2D surface exclusion: dynamic vertices must not reside
                // within triangles from a different drawable. Projection
                // to closest boundary prevents cross-layer penetration.
                if(options_.surface_thickness>0){
                    std::size_t tested=0;
                    for(auto &p:candidate.state)if(p.inverse_mass)
                    for(const auto &face:faces_){
                        const auto &a=candidate.state[face[0]],&b=candidate.state[face[1]],&c=candidate.state[face[2]];
                        if(p.part==a.part)continue;
                        if(++tested>50000)return Status::out_of_budget;
                        const double cross=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
                        if(std::abs(cross)<1e-10)continue;
                        const double w0=((b.x-p.x)*(c.y-p.y)-(b.y-p.y)*(c.x-p.x))/cross;
                        const double w1=((c.x-p.x)*(a.y-p.y)-(c.y-p.y)*(a.x-p.x))/cross;
                        const double w2=1-w0-w1;
                        if(w0<0||w1<0||w2<0)continue;
                        double best=std::numeric_limits<double>::max(),tx=p.x,ty=p.y;
                        const Node* corners[3]={&a,&b,&c};
                        for(unsigned k=0;k<3;++k){
                            const auto &u=*corners[k],&v=*corners[(k+1)%3];
                            const double dx=v.x-u.x,dy=v.y-u.y,len=dx*dx+dy*dy;
                            if(len<1e-12)continue;
                            const double t=std::clamp(((p.x-u.x)*dx+(p.y-u.y)*dy)/len,0.,1.);
                            const double qx=u.x+t*dx,qy=u.y+t*dy,d=std::hypot(p.x-qx,p.y-qy);
                            if(d<best){best=d;tx=qx;ty=qy;}
                        }
                        if(best<std::numeric_limits<double>::max()){
                            double dx=tx-p.x,dy=ty-p.y,d=std::hypot(dx,dy);
                            if(d>1e-10){p.x=tx+dx/d*options_.surface_thickness;p.y=ty+dy/d*options_.surface_thickness;}
                        }
                    }
                }                const double alpha=options_.pose_compliance/(dt*dt);
                for(auto &p:candidate.state)if(p.inverse_mass){
                    const double stiffness=p.inverse_mass/(p.inverse_mass+alpha);
                    p.x+=(p.source_x-p.x)*stiffness;p.y+=(p.source_y-p.y)*stiffness;
                }
            }
        }        for(const auto &p:candidate.state){
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
