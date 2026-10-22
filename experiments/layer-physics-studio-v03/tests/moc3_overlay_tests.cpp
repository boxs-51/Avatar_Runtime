#include "../native/moc3_xpbd.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
using namespace ars::experimental::lps;
int main(){
    ars::moc3::Snapshot source;source.generation=1;
    for(int j=0;j<3;++j){
        ars::moc3::Drawable d;d.id="part"+std::to_string(j);d.texture_index=0;
        d.render_order=5+j;d.opacity=.75f;d.masks={1};d.blend_mode=1;
        d.vertices={{0.f,float(j),0,0},{1,float(j),1,0},
                    {1,1.f+float(j),1,1},{0,1.f+float(j),0,1}};
        d.indices={0,1,2,0,2,3};source.drawables.push_back(d);
    }
    moc3::Overlay bridge;Options opt;
    opt.dt=1./120;opt.gravity_y=500;opt.wind_x=0;opt.edge_compliance=0;opt.iterations=8;
    const auto original=source;
    assert(bridge.bind(source,{0,1},opt,17)==Status::ok);
    bool moved=false;
    for(int i=0;i<20;++i){
        moc3::Overlay::Candidate candidate;
        assert(bridge.prepare(source,candidate)==Status::ok);
        const auto& scene=candidate.output;
        assert(scene.drawables.size()==3&&scene.drawables[2].vertices==original.drawables[2].vertices);
        assert(scene.drawables[0].masks==original.drawables[0].masks);
        assert(scene.drawables[0].blend_mode==original.drawables[0].blend_mode);
        assert(scene.drawables[0].vertices[1].u==original.drawables[0].vertices[1].u);
        moved|=std::abs(scene.drawables[0].vertices[1].y-original.drawables[0].vertices[1].y)>1e-4;
        assert(bridge.accept(std::move(candidate),source.generation)==Status::ok);
        ++source.generation;
    }
    assert(moved);
    moc3::Overlay::Candidate stale;
    assert(bridge.prepare(source,stale)==Status::ok);
    assert(bridge.accept(std::move(stale),source.generation+1)==Status::stale);
    source.drawables[0].indices.push_back(44);
    assert(bridge.prepare(source,stale)!=Status::ok);
    std::puts("PASS MOC3 evaluated snapshot XPBD overlay and rollback");
}
