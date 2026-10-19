#include "drawable_packet.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <ostream>
#include <stdexcept>
namespace ars::moc3::admission {
namespace {
struct Invalid {};
void need(bool value){if(!value)throw Invalid{};}
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t pos=0;
    auto take(std::size_t n){need(n<=bytes.size()-pos);auto out=bytes.subspan(pos,n);pos+=n;return out;}
    std::uint64_t integer(unsigned n){auto s=take(n);std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t(s[i])<<(8*i);return v;}
    std::uint32_t u32(){return static_cast<std::uint32_t>(integer(4));}
    std::int32_t i32(){return std::bit_cast<std::int32_t>(u32());}
    float real(float lo,float hi){auto v=std::bit_cast<float>(u32());need(std::isfinite(v)&&v>=lo&&v<=hi);return v;}
};
void add(std::uint64_t& total,std::uint64_t n,std::uint64_t cap){need(n<=cap-total);total+=n;}
struct Meta {std::uint32_t indices=0,masks=0;std::size_t mask_offset=0;};
// Pass one deliberately has fixed scratch, no attacker-count-based allocations.
void walk(Reader& r,std::uint64_t nonce,std::uint64_t generation,std::uint32_t version,Snapshot* scene,AtlasSet* images,std::uint32_t& flags){
    auto magic=r.take(8);need(std::memcmp(magic.data(),"ARSDRAW1",8)==0);
    need((version==1&&generation==1)||((version==2||version==3)&&generation>=2&&generation<=9));
    need(r.u32()==version);need(r.integer(8)==r.bytes.size());need(r.integer(8)==generation);
    need(nonce&&r.integer(8)==nonce);flags=r.u32();need((flags&~255u)==0&&((flags&3)==0||(flags&3)==3));
    need(version==3?(flags&131u)==131u:(flags&128u)==0);
    auto na=r.u32(),nd=r.u32();need(na&&na<=16&&nd&&nd<=4096);
    std::uint64_t texels=0,vertices=0,indices=0,masks=0,mask_work=0;
    if(images)images->images.reserve(na);if(scene){scene->generation=generation;scene->drawables.reserve(nd);}
    for(std::uint32_t i=0;i<na;++i){
        auto w=r.u32(),h=r.u32();auto size=r.integer(8);need(w&&h&&w<=8192&&h<=8192&&size==std::uint64_t(w)*h*4);
        add(texels,size,128ull*1024*1024);auto data=r.take(static_cast<std::size_t>(size));
        if(images){Atlas image;image.width=w;image.height=h;image.rgba.assign(data.begin(),data.end());images->images.push_back(std::move(image));}
    }
    std::array<Meta,4096> meta{};
    for(std::uint32_t i=0;i<nd;++i){
        Drawable d; // Empty fixed-capacity fields only until pass two.
        d.texture_index=r.i32();d.draw_order=r.i32();d.render_order=r.i32();d.blend_mode=r.i32();
        need(d.texture_index>=0&&std::uint32_t(d.texture_index)<na&&d.blend_mode>=0&&d.blend_mode<=2);
        auto bits=r.u32();need((bits&~7u)==0);d.visible=bits&1;d.double_sided=bits&2;d.inverted_mask=bits&4;
        d.opacity=r.real(0,1);
        d.multiply={r.real(0,1),r.real(0,1),r.real(0,1),r.real(0,1)};
        d.screen={r.real(0,1),r.real(0,1),r.real(0,1),r.real(0,1)};
        auto nv=r.u32(),ni=r.u32(),nm=r.u32();need(nv<=65536&&ni%3==0&&nm<=nd);
        add(vertices,nv,2000000);add(indices,ni,12000000);add(masks,nm,2000000);
        // Verify available bytes before any per-draw allocation.
        need(std::uint64_t(nv)*16+std::uint64_t(ni)*2+std::uint64_t(nm)*4<=r.bytes.size()-r.pos);
        if(scene){d.id=std::to_string(i);d.vertices.reserve(nv);d.indices.reserve(ni);d.masks.reserve(nm);}
        for(std::uint32_t j=0;j<nv;++j){Vertex v{r.real(-1e6f,1e6f),r.real(-1e6f,1e6f),r.real(-16,16),r.real(-16,16)};if(scene)d.vertices.push_back(v);}
        for(std::uint32_t j=0;j<ni;++j){auto v=static_cast<std::uint16_t>(r.integer(2));need(v<nv);if(scene)d.indices.push_back(v);}
        meta[i]={ni,nm,r.pos};std::array<bool,4096> seen{};
        for(std::uint32_t j=0;j<nm;++j){auto m=r.u32();need(m<nd&&m!=i&&!seen[m]);seen[m]=true;if(scene)d.masks.push_back(m);}
        if(scene)scene->drawables.push_back(std::move(d));
    }
    need(r.pos==r.bytes.size());
    for(std::uint32_t i=0;i<nd;++i){Reader refs{r.bytes,meta[i].mask_offset};for(std::uint32_t j=0;j<meta[i].masks;++j){auto m=refs.u32();need(meta[m].masks==0);add(mask_work,meta[m].indices,64ull*1024*1024);}}
}
void put(std::ostream& out,std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)out.put(static_cast<char>(value>>(8*i)));if(!out)throw Invalid{};}
void real(std::ostream& out,float f){put(out,std::bit_cast<std::uint32_t>(f),4);}
}
bool validate_frame(std::span<const std::uint8_t> bytes,std::uint64_t nonce,std::uint64_t generation,std::uint32_t version) noexcept {
    try{need(bytes.size()>=48&&bytes.size()<=packet_cap);std::uint32_t flags=0;Reader r{bytes};walk(r,nonce,generation,version,nullptr,nullptr,flags);return true;}catch(...){return false;}
}
bool decode(std::span<const std::uint8_t> bytes,std::uint64_t nonce,VerifiedScene& output) noexcept {return decode_frame(bytes,nonce,1,1,output);}
bool decode_frame(std::span<const std::uint8_t> bytes,std::uint64_t nonce,std::uint64_t generation,std::uint32_t version,VerifiedScene& output) noexcept {
    try{
        need(bytes.size()>=48&&bytes.size()<=packet_cap);std::uint32_t flags=0;Reader first{bytes};walk(first,nonce,generation,version,nullptr,nullptr,flags);
        auto scene=std::make_shared<Snapshot>();auto images=std::make_shared<AtlasSet>();Reader second{bytes};walk(second,nonce,generation,version,scene.get(),images.get(),flags);
        VerifiedScene candidate;candidate.snapshot_=std::move(scene);candidate.atlases_=std::move(images);candidate.flags_=flags;output=std::move(candidate);return true;
    }catch(...){return false;}
}
std::uint64_t encoded_size(const Snapshot& scene,const AtlasSet& images){
    std::uint64_t size=48;for(auto& a:images.images)add(size,16+a.rgba.size(),packet_cap);
    for(auto& d:scene.drawables)add(size,68+std::uint64_t(d.vertices.size())*16+std::uint64_t(d.indices.size())*2+std::uint64_t(d.masks.size())*4,packet_cap);
    return size;
}
void encode(std::ostream& out,const Snapshot& scene,const AtlasSet& images,std::uint64_t nonce,std::uint32_t flags){encode_frame(out,scene,images,nonce,flags,1);}
void encode_frame(std::ostream& out,const Snapshot& scene,const AtlasSet& images,std::uint64_t nonce,std::uint32_t flags,std::uint32_t version){
    const auto size=encoded_size(scene,images);
    out.write("ARSDRAW1",8);put(out,version,4);put(out,size,8);put(out,scene.generation,8);put(out,nonce,8);put(out,flags,4);put(out,images.images.size(),4);put(out,scene.drawables.size(),4);
    for(auto& a:images.images){put(out,a.width,4);put(out,a.height,4);put(out,a.rgba.size(),8);out.write(reinterpret_cast<const char*>(a.rgba.data()),a.rgba.size());}
    for(auto& d:scene.drawables){
        for(auto v:{d.texture_index,d.draw_order,d.render_order,d.blend_mode})put(out,std::bit_cast<std::uint32_t>(v),4);
        put(out,(d.visible?1:0)|(d.double_sided?2:0)|(d.inverted_mask?4:0),4);real(out,d.opacity);
        for(auto c:{d.multiply,d.screen})for(float f:{c.r,c.g,c.b,c.a})real(out,f);
        put(out,d.vertices.size(),4);put(out,d.indices.size(),4);put(out,d.masks.size(),4);
        for(auto v:d.vertices)for(float f:{v.x,v.y,v.u,v.v})real(out,f);
        for(auto v:d.indices)put(out,v,2);for(auto v:d.masks)put(out,v,4);
    }
    if(!out)throw Invalid{};
}
bool decode_sequence(std::span<const std::uint8_t> bytes,std::uint64_t nonce,VerifiedSequence& output) noexcept {
    try{
        need(bytes.size()>=32&&bytes.size()<=packet_cap);Reader r{bytes};auto magic=r.take(8);
        need(std::memcmp(magic.data(),"ARSEVAL1",8)==0);auto version=r.u32();need(version==1||version==2);auto count=r.u32();need(count&&count<=8);
        need(nonce&&r.integer(8)==nonce&&r.integer(8)==bytes.size());
        std::array<std::span<const std::uint8_t>,8> packets{};std::array<std::uint64_t,8> clocks{};
        for(std::uint32_t i=0;i<count;++i){
            need(r.integer(8)==i+1);auto clock=r.integer(8);need(clock<=1000000000ull);
            need(i?clock>clocks[i-1]&&clock-clocks[i-1]<=8ull*16666667:clock==0);clocks[i]=clock;
            auto length=r.integer(8);need(length>=48&&length<=r.bytes.size()-r.pos);packets[i]=r.take(static_cast<std::size_t>(length));
        }
        need(r.pos==r.bytes.size());
        // All inner streams pass a nonallocating pass before ANY decoded frame allocation.
        for(std::uint32_t i=0;i<count;++i)need(validate_frame(packets[i],nonce,i+2,version+1));
        VerifiedSequence candidate;candidate.frames_.reserve(count);candidate.clocks_.reserve(count);
        for(std::uint32_t i=0;i<count;++i){VerifiedScene scene;need(decode_frame(packets[i],nonce,i+2,version+1,scene));candidate.frames_.push_back(std::move(scene));candidate.clocks_.push_back(clocks[i]);}
        output=std::move(candidate);return true;
    }catch(...){return false;}
}

}
