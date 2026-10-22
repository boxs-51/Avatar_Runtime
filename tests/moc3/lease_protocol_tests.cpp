#include "../../src/moc3/lease_protocol.hpp"
#include "../../src/moc3/retained_physics.hpp"
#include "../../src/assets/asset_limits.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <malloc.h>
#include <sstream>
using namespace ars::moc3;
using namespace ars::moc3::admission;
void check(bool v){if(!v){std::cerr<<"LEASE_CODEC_FAIL\n";std::exit(1);}}
void* ARS_CALL alloc(void*,std::uint64_t n,std::uint32_t a){return _aligned_malloc(n,a);}
void ARS_CALL release(void*,void* p,std::uint64_t,std::uint32_t){_aligned_free(p);}
auto span(const std::string& s){return std::span(reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
int main(int argc,char** argv){
    check(argc==2);Digest model=sha256(span("public generated model"));LeaseKey key{7,1,1,1,0,1,0,1,42,model,sha256({})};
    auto skipped=key;skipped.generation=skipped.revision=9;check(!lease_successor(nullptr,skipped,sha256({})));check(lease_successor(nullptr,skipped,sha256({}),8));
    PhysicsCheckpoint cp;cp.generation=1;cp.model=model;cp.particles={{{0,0,0,0},{0,1,0,0}}};
    auto state=encode_lease_checkpoint(key,cp);LeaseState decoded;check(decode_lease_checkpoint(state,decoded));check(lease_successor(nullptr,key,sha256({})));
    Drawable d;d.texture_index=0;d.double_sided=true;d.screen={0,0,0,1};d.vertices={{-1,-1,0,0},{1,-1,1,0},{0,1,.5f,1}};d.indices={0,1,2};Snapshot snapshot;snapshot.generation=2;snapshot.drawables={d};AtlasSet images;images.images.push_back({1,1,{255,0,0,255}});
    auto packet=encode_lease_candidate(key,snapshot,images,state,131);VerifiedLease current;check(decode_lease_candidate(packet,key,current));auto old=current.checkpoint_bytes();unsigned rejects=0;
    for(std::size_t n=0;n<packet.size();++n){check(!decode_lease_candidate(std::span(packet).first(n),key,current));check(current.checkpoint_bytes()==old);++rejects;}
    for(auto at:{8u,12u,16u,24u,32u,40u,48u,56u,64u,72u,80u,88u,96u,128u,160u,168u,172u}){auto bad=packet;bad[at]^=1;check(!decode_lease_candidate(bad,key,current));++rejects;}
    auto trailing=packet;trailing.push_back(0);check(!decode_lease_candidate(trailing,key,current));
    VerifiedStep legacy;check(!decode_step(packet,42,1,0,model,sha256({}),legacy));
    auto next=key;next.generation=next.revision=2;next.clock=next.local_clock=16666667;next.local_sequence=2;next.predecessor=sha256(state);check(lease_successor(&decoded,next,next.predecessor));
    auto wrong=next;wrong.local_clock++;check(!lease_successor(&decoded,wrong,next.predecessor));wrong=next;wrong.epoch=2;wrong.local_sequence=1;check(lease_successor(&decoded,wrong,next.predecessor));wrong.package++;check(!lease_successor(&decoded,wrong,next.predecessor));
    auto command=lease_key_bytes(key);command.insert(command.end(),{3,0,0,0,0,0,0,0});check(decode_lease_command(command).bindings.empty());for(auto at:{136u,140u}){auto bad=command;bad[at]=255;bool rejected=false;try{decode_lease_command(bad);}catch(...){rejected=true;}check(rejected);++rejects;}
    std::ostringstream frame(std::ios::binary);write_lease_message(frame,2,command);std::istringstream input(frame.str(),std::ios::binary);check(read_lease_message(input,4096).body==command);
    // Actual native solver continuity across repeated explicit epoch rebases.
    std::ifstream file(argv[1],std::ios::binary);std::string raw((std::istreambuf_iterator<char>(file)),{});check(!raw.empty());ars::assets::Resource resource(alloc,release,nullptr,16*ars::assets::MiB);ars::assets::Limits limits;ars::assets::Work work{limits.work};
    std::vector<ParameterInfo> catalog{{"P",-30,0,30},{"Q",-45,0,45}};RetainedPhysics continuous(span(raw),catalog,resource,limits,work);PhysicsCheckpoint accepted;
    for(unsigned i=0;i<20;++i){RetainedPhysics resumed(span(raw),catalog,resource,limits,work);if(i)resumed.restore_epoch(accepted);
        std::vector<Parameter> drive{{"P",i%2?15.f:-15.f}};auto a=continuous.prepare(i*16666667ull,drive,true,false),b=resumed.prepare(i?16666667:0,drive,true,false);auto expected=continuous.checkpoint(a,model);accepted=resumed.checkpoint(b,model);
        check(expected.particles==accepted.particles&&expected.remainder==accepted.remainder);continuous.commit(a);}
    std::cout<<"LEASE_CODEC_EPOCH_SOLVER_PASS negatives="<<rejects<<"\n";
}
