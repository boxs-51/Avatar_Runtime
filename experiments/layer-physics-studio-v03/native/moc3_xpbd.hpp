#pragma once
// Experimental XPBD overlay for an independently evaluated MOC3 Snapshot.
// GPU query completion must precede accept(); never mutate accepted MOC assets.
#include "../../../src/moc3/independent_snapshot.hpp"
#include "native_mesh_xpbd.hpp"
#include <array>
#include <memory>
#include <set>
#include <cstdint>
namespace ars::experimental::lps::moc3 {
struct Point {float x=0,y=0;};
struct Part {
    std::uint32_t part_index=0;
    std::vector<Point> vertices,uv;
    std::vector<std::array<std::uint32_t,3>> triangles;
};
struct Scene {std::vector<Part> draws;};
class Overlay {
    Session<Scene> solver_;
    std::vector<std::uint32_t> indices_;
    std::uint64_t key_=0;
    bool enabled_=false;
    static Status extract(const ars::moc3::Snapshot& s,
                          const std::vector<std::uint32_t>& chosen,Scene& out) {
        Scene next;std::size_t vertices=0;
        for(const auto index:chosen) {
            if(index>=s.drawables.size())return Status::invalid;
            const auto& src=s.drawables[index];
            if(src.indices.size()%3||src.vertices.empty()||src.vertices.size()>2048 ||
               vertices+src.vertices.size()>2048)return Status::out_of_budget;
            Part d;d.part_index=index;d.vertices.reserve(src.vertices.size());
            d.uv.reserve(src.vertices.size());d.triangles.reserve(src.indices.size()/3);
            for(const auto& v:src.vertices){
                if(!std::isfinite(v.x)||!std::isfinite(v.y)||
                   !std::isfinite(v.u)||!std::isfinite(v.v))return Status::invalid;
                d.vertices.push_back({v.x,v.y});d.uv.push_back({v.u,v.v});
            }
            for(std::size_t j=0;j<src.indices.size();j+=3){
                const auto a=src.indices[j],b=src.indices[j+1],c=src.indices[j+2];
                if(a>=src.vertices.size()||b>=src.vertices.size()||c>=src.vertices.size())return Status::invalid;
                d.triangles.push_back({a,b,c});
            }
            next.draws.push_back(std::move(d));vertices+=src.vertices.size();
        }
        out=std::move(next);return Status::ok;
    }
public:
    struct Candidate {
        ars::moc3::Snapshot output;
        Session<Scene>::Candidate physics;
        std::uint64_t source_generation=0;
    };
    bool enabled() const noexcept {return enabled_;}
    std::size_t selected_count() const noexcept {return indices_.size();}
    Status bind(const ars::moc3::Snapshot& source,std::vector<std::uint32_t> chosen,
                Options options,std::uint64_t key) {
        if(!key||!source.generation||chosen.empty()||chosen.size()>16)return Status::invalid;
        if(!std::is_sorted(chosen.begin(),chosen.end()) ||
           std::adjacent_find(chosen.begin(),chosen.end())!=chosen.end())return Status::invalid;
        Scene scene;auto status=extract(source,chosen,scene);if(status!=Status::ok)return status;
        options.dynamic_parts.clear();
        for(const auto index:chosen)options.dynamic_parts.push_back({index,0,1});
        Session<Scene> replacement;status=replacement.bind(scene,options,key);
        if(status!=Status::ok)return status;
        solver_=std::move(replacement);indices_=std::move(chosen);key_=key;enabled_=true;
        return Status::ok;
    }
    Status prepare(const ars::moc3::Snapshot& source,Candidate& out)const {
        if(!enabled_||!source.generation)return Status::stale;
        Scene scene;auto status=extract(source,indices_,scene);if(status!=Status::ok)return status;
        Session<Scene>::Candidate proposed;
        status=solver_.prepare(scene,key_,proposed);if(status!=Status::ok)return status;
        Candidate candidate;candidate.output=source;candidate.source_generation=source.generation;
        for(const auto& d:proposed.scene.draws){
            auto& dest=candidate.output.drawables[d.part_index];
            if(d.vertices.size()!=dest.vertices.size())return Status::stale;
            for(std::size_t i=0;i<d.vertices.size();++i){
                dest.vertices[i].x=d.vertices[i].x;dest.vertices[i].y=d.vertices[i].y;
            }
        }
        candidate.physics=std::move(proposed);out=std::move(candidate);return Status::ok;
    }
    Status accept(Candidate&& candidate,std::uint64_t completed_source_generation) {
        if(!enabled_||!completed_source_generation||candidate.source_generation!=completed_source_generation)
            return Status::stale;
        return solver_.accept(std::move(candidate.physics),key_);
    }
    void disable(){enabled_=false;indices_.clear();key_=0;}
};
}
