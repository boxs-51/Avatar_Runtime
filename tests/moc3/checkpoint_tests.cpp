#include "../../src/moc3/retained_physics.hpp"
#include "../../src/moc3/step_packet.hpp"
#include "../../src/assets/asset_limits.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <malloc.h>
#include <sstream>
using namespace ars::moc3;
using namespace ars::moc3::admission;
void* ARS_CALL alloc(void*,std::uint64_t n,std::uint32_t a){return _aligned_malloc(n,a);}
void ARS_CALL free_mem(void*,void* p,std::uint64_t,std::uint32_t){_aligned_free(p);}
void check(bool value){if(!value){std::cerr<<"CHECKPOINT_FAIL\n";std::exit(1);}}
auto bytes(const std::string& s){return std::span(reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
int main(int argc,char** argv){
    check(argc==2);std::ifstream file(argv[1],std::ios::binary);std::string raw((std::istreambuf_iterator<char>(file)),{});check(!raw.empty());
    Digest model=sha256(bytes("generated model"));auto empty=sha256({});check(empty[0]==0xe3&&empty[31]==0x55);
    ars::assets::Resource r(alloc,free_mem,nullptr,16*ars::assets::MiB);ars::assets::Limits limits;ars::assets::Work work{limits.work};std::vector<ParameterInfo> catalog{{"P",-30,0,30},{"Q",-45,0,45}};
    RetainedPhysics continuous(bytes(raw),catalog,r,limits,work);PhysicsCheckpoint previous;
    const std::uint64_t clocks[]{0,16666666,16666668,33333334,50000001,66666668,83333335};
    for(unsigned i=0;i<7;++i){
        RetainedPhysics resumed(bytes(raw),catalog,r,limits,work);if(i)resumed.restore(previous);
        std::vector<Parameter> manual;if(i==1)manual.push_back({"P",15});bool enabled=i!=4,reset=i==0||i==5;
        auto a=continuous.prepare(clocks[i],manual,enabled,reset),b=resumed.prepare(clocks[i],manual,enabled,reset);
        check(a.parameters().size()==b.parameters().size());for(std::size_t j=0;j<a.parameters().size();++j)check(a.parameters()[j].value==b.parameters()[j].value);
        auto original=encode_checkpoint(continuous.checkpoint(a,model));auto restored=encode_checkpoint(resumed.checkpoint(b,model));check(original==restored);
        check(decode_checkpoint(original,model,i+1,clocks[i],previous));continuous.commit(a);
    }
    auto checkpoint=encode_checkpoint(previous);PhysicsCheckpoint kept=previous;std::size_t rejects=0;
    for(std::size_t n=0;n<checkpoint.size();++n){check(!decode_checkpoint(std::span(checkpoint).first(n),model,previous.generation,previous.clock,kept));check(kept.particles==previous.particles);++rejects;}
    auto wrong=model;wrong[0]^=1;check(!validate_checkpoint(checkpoint,wrong,previous.generation,previous.clock));
    auto field=[&](std::size_t pos,std::uint64_t value,unsigned n){auto bad=checkpoint;for(unsigned j=0;j<n;++j)bad.at(pos+j)=static_cast<std::uint8_t>(value>>(8*j));check(!validate_checkpoint(bad,model,previous.generation,previous.clock));++rejects;};
    field(12,0,4);field(12,129,4);field(16,0,8);field(32,16666667,8);field(72,1,4);field(72,2049,4);field(76,0x7ff8000000000000ull,8);field(84,0x7ff0000000000000ull,8);
    {RetainedPhysics test(bytes(raw),catalog,r,limits,work);auto bad=previous;bad.particles[0].push_back({});bool rejected=false;try{test.restore(bad);}catch(...){rejected=true;}check(rejected);check(test.checkpoint(test.prepare(0,{},false,true),model).generation==1);}
    Drawable d;d.texture_index=0;d.double_sided=true;d.screen={0,0,0,1};d.vertices={{-1,-1,0,0},{1,-1,1,0},{0,1,.5f,1}};d.indices={0,1,2};Snapshot scene;scene.generation=previous.generation+1;scene.drawables={d};AtlasSet images;images.images.push_back({1,1,{255,0,0,255}});
    std::ostringstream out(std::ios::binary);encode_step(out,scene,images,42,previous.generation,previous.clock,model,empty,checkpoint,131);auto packet=out.str();VerifiedStep step;
    check(decode_step(bytes(packet),42,previous.generation,previous.clock,model,empty,step));auto prior=step.checkpoint_bytes();
    for(std::size_t n=0;n<packet.size();++n){check(!decode_step(bytes(packet).first(n),42,previous.generation,previous.clock,model,empty,step));check(step.checkpoint_bytes()==prior);++rejects;}
    for(auto offset:{std::size_t(12),std::size_t(16),std::size_t(24),std::size_t(32),std::size_t(48),std::size_t(56),std::size_t(88)}){auto bad=packet;bad[offset]^=1;check(!decode_step(bytes(bad),42,previous.generation,previous.clock,model,empty,step));++rejects;}
    check(!decode_step(bytes(packet),43,previous.generation,previous.clock,model,empty,step));VerifiedScene old;check(!decode(bytes(packet),42,old));
    std::cout<<"CHECKPOINT_RESTORE_AND_WIRE_PASS rejected="<<rejects<<"\n";
}
