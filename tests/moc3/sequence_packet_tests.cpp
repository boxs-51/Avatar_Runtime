#include "drawable_packet.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
using namespace ars::moc3;
using namespace ars::moc3::admission;
void check(bool v){if(!v){std::cerr<<"SEQUENCE_PACKET_FAIL\n";std::exit(1);}}
void emit(std::ostream& out,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)out.put(static_cast<char>(v>>(8*i)));}
void patch(std::string& s,std::size_t p,std::uint64_t v,unsigned n=4){for(unsigned i=0;i<n;++i)s.at(p+i)=char(v>>(8*i));}
auto bytes(const std::string& s){return std::span(reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
int main(){
    Drawable d;d.texture_index=0;d.double_sided=true;d.screen={0,0,0,1};d.vertices={{-1,-1,0,0},{1,-1,1,0},{0,1,.5f,1}};d.indices={0,1,2};
    Snapshot s;s.generation=1;s.drawables={d};AtlasSet a;a.images.push_back({1,1,{255,0,0,255}});
    std::string frames[3];for(unsigned i=0;i<3;++i){s.generation=i+2;std::ostringstream out(std::ios::binary);encode_frame(out,s,a,42,3,2);frames[i]=out.str();}
    std::ostringstream out(std::ios::binary);out.write("ARSEVAL1",8);emit(out,1,4);emit(out,3,4);emit(out,42,8);emit(out,32+3*(24+frames[0].size()),8);
    for(unsigned i=0;i<3;++i){emit(out,i+1,8);emit(out,i*16666667ull,8);emit(out,frames[i].size(),8);out.write(frames[i].data(),frames[i].size());}
    auto valid=out.str();VerifiedSequence sequence;check(decode_sequence(bytes(valid),42,sequence)&&sequence.size()==3&&sequence.timestamp(2)==33333334);
    auto prior=sequence.frame(0).snapshot();std::size_t rejected=0;
    auto reject=[&](std::string data,std::uint64_t nonce=42){check(!decode_sequence(bytes(data),nonce,sequence));check(sequence.frame(0).snapshot()==prior);++rejected;};
    reject(valid,43);reject(valid,0);for(std::size_t i=0;i<valid.size();++i)reject(valid.substr(0,i));
    auto field=[&](std::size_t p,std::uint64_t value,unsigned n=4){auto data=valid;patch(data,p,value,n);reject(data);};
    field(0,'X',1);field(8,2);field(12,0);field(12,9);field(16,43,8);field(24,~0ull,8);field(32,0,8);field(40,1,8);field(48,~0ull,8);
    auto next=32+24+frames[0].size();field(next,1,8);field(next+8,0,8);field(next+8,1000000001,8);
    field(next+24+8,1);field(next+24+20,2,8);field(next+24+28,43,8);
    auto late=next+24+frames[1].size();field(late+24+68+20,0x7fc00000);field(late+24+68+64,4097);
    reject(valid+"x");VerifiedScene static_scene;check(!decode(bytes(frames[0]),42,static_scene));
    check(!decode_frame(bytes(frames[0]),42,3,2,static_scene));check(decode_frame(bytes(frames[0]),42,2,2,static_scene));
    auto physics=valid;patch(physics,8,2);
    for(auto start:{std::size_t(56),next+24,late+24}){patch(physics,start+8,3);patch(physics,start+36,131);}
    VerifiedSequence physical;check(decode_sequence(bytes(physics),42,physical));auto physical_prior=physical.frame(0).snapshot();
    auto physics_reject=[&](std::string data){check(!decode_sequence(bytes(data),42,physical));check(physical.frame(0).snapshot()==physical_prior);++rejected;};
    for(auto [offset,value]:{std::pair<std::size_t,unsigned>{8,1},{late+24+8,2},{late+24+36,3},{late+24+36,128},{late+24+36,387}}){auto bad=physics;patch(bad,offset,value);physics_reject(bad);}
    check(!decode(bytes(physics),42,static_scene));
    for(std::size_t i=0;i<physics.size();++i)physics_reject(physics.substr(0,i));
    std::cout<<"SEQUENCE_PACKET_PASS rejected="<<rejected<<" GPU_CREATIONS=0\n";
}
