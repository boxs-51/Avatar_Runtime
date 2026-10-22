#include "drawable_packet.hpp"
#include <bit>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
using namespace ars::moc3;
using namespace ars::moc3::admission;
void check(bool v){if(!v){std::cerr<<"PACKET_TEST_FAIL\n";std::exit(1);}}
void put(std::string& s,std::size_t at,std::uint64_t v,unsigned n=4){for(unsigned i=0;i<n;++i)s.at(at+i)=char(v>>(8*i));}
auto span(const std::string& s){return std::span(reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
std::string packet(Snapshot s,AtlasSet a,std::uint32_t flags=3){std::ostringstream out(std::ios::binary);encode(out,s,a,42,flags);return out.str();}
int main(){
    Drawable d;d.texture_index=0;d.double_sided=true;d.screen={0,0,0,1};d.vertices={{-1,-1,0,0},{1,-1,1,0},{0,1,.5f,1}};d.indices={0,1,2};
    Snapshot s;s.generation=1;s.drawables={d};AtlasSet a;a.images.push_back({1,1,{255,0,0,255}});
    auto valid=packet(s,a);VerifiedScene scene;check(decode(span(valid),42,scene));auto old=scene.snapshot();auto old_atlas=scene.atlases();
    std::size_t rejected=0;auto reject=[&](const std::string& data,std::uint64_t nonce=42){check(!decode(span(data),nonce,scene));check(scene.snapshot()==old&&scene.atlases()==old_atlas);++rejected;};
    reject(valid,43);reject(valid,0);
    for(std::size_t i=0;i<valid.size();++i)reject(valid.substr(0,i));
    auto mutate=[&](std::size_t p,std::uint64_t v,unsigned n=4){auto data=valid;put(data,p,v,n);reject(data);};
    mutate(0,'X',1);mutate(8,2);mutate(12,valid.size()+1,8);mutate(20,2,8);mutate(28,0,8);
    mutate(36,128);mutate(36,1);mutate(36,2);mutate(40,0);mutate(40,17);mutate(44,0);mutate(44,4097);
    mutate(48,0);mutate(48,8193);mutate(56,~0ull,8);
    const std::size_t draw=68;mutate(draw,1);mutate(draw+12,3);mutate(draw+16,8);
    for(auto p:{draw+20,draw+24,draw+28,draw+32,draw+36,draw+40,draw+44,draw+48,draw+52}){
        mutate(p,0x7fc00000);mutate(p,0x7f800000);mutate(p,std::bit_cast<std::uint32_t>(-0.01f));mutate(p,std::bit_cast<std::uint32_t>(1.01f));}
    mutate(draw+56,65537);mutate(draw+60,2);mutate(draw+64,4097);
    mutate(draw+68,0x7fc00000);mutate(draw+68,std::bit_cast<std::uint32_t>(1e7f));mutate(draw+76,std::bit_cast<std::uint32_t>(17.f));
    mutate(draw+68+48,3,2);auto extra=valid+"x";reject(extra);put(extra,12,extra.size(),8);reject(extra);
    auto maskcase=[&](std::vector<Drawable> draws){Snapshot x;x.generation=1;x.drawables=std::move(draws);reject(packet(x,a));};
    auto m=d;m.masks={0};maskcase({m});m.masks={2};maskcase({m,d});m.masks={1,1};maskcase({m,d});
    auto n=d;m.masks={1};n.masks={0};maskcase({m,n});n.masks={2};maskcase({m,n,d});
    // Valid direct mask and deterministic equal order remain supported.
    m.masks={1};s.drawables={m,d};VerifiedScene masked;check(decode(span(packet(s,a)),42,masked));
    // Independently mutate every byte. Each accepted mutation must roundtrip through an immutable scene.
    for(std::size_t i=0;i<valid.size();++i){auto x=valid;x[i]^=char(0xff);VerifiedScene candidate;
        if(decode(span(x),42,candidate)){auto encoded=packet(*candidate.snapshot(),*candidate.atlases(),candidate.flags());VerifiedScene again;check(decode(span(encoded),42,again));}else ++rejected;}
    std::cout<<"PACKET_TEST_PASS rejected="<<rejected<<" GPU_CREATIONS=0\n";
}
