#include "../native/moc3_xpbd.hpp"
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <string>
using namespace ars::experimental::lps;
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"MOC3_XPBD_RED %s\n",message);std::exit(1);}}
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
    check(bridge.bind(source,{0,1},opt,17)==Status::ok,"bridge.bind(source,{0,1},opt,17)==Status::ok");
    bool moved=false;
    for(int i=0;i<20;++i){
        moc3::Overlay::Candidate candidate;
        check(bridge.prepare(source,candidate)==Status::ok,"bridge.prepare(source,candidate)==Status::ok");
        const auto& scene=candidate.output;
        check(scene.drawables.size()==3&&scene.drawables[2].vertices==original.drawables[2].vertices,"scene.drawables.size()==3&&scene.drawables[2].vertices==original.drawabl");
        check(scene.drawables[0].masks==original.drawables[0].masks,"scene.drawables[0].masks==original.drawables[0].masks");
        check(scene.drawables[0].blend_mode==original.drawables[0].blend_mode,"scene.drawables[0].blend_mode==original.drawables[0].blend_mode");
        check(scene.drawables[0].vertices[1].u==original.drawables[0].vertices[1].u,"scene.drawables[0].vertices[1].u==original.drawables[0].vertices[1].u");
        moved|=std::abs(scene.drawables[0].vertices[1].y-original.drawables[0].vertices[1].y)>1e-4;
        check(bridge.accept(std::move(candidate),source.generation)==Status::ok,"bridge.accept(std::move(candidate),source.generation)==Status::ok");
        ++source.generation;
    }
    check(moved,"moved");
    moc3::Overlay::Candidate stale;
    check(bridge.prepare(source,stale)==Status::ok,"bridge.prepare(source,stale)==Status::ok");
    check(bridge.accept(std::move(stale),source.generation+1)==Status::stale,"bridge.accept(std::move(stale),source.generation+1)==Status::stale");
    source.drawables[0].indices.push_back(44);
    check(bridge.prepare(source,stale)!=Status::ok,"bridge.prepare(source,stale)!=Status::ok");
    std::puts("PASS MOC3 evaluated snapshot XPBD overlay and rollback");
}
