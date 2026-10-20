#include "lease_protocol.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
namespace ars::moc3::admission {
namespace {
void need(bool b){if(!b)throw std::runtime_error("LEASE_REJECTED");}
struct Reader {
    std::span<const std::uint8_t> b;std::size_t p=0;
    auto take(std::size_t n){need(n<=b.size()-p);auto out=b.subspan(p,n);p+=n;return out;}
    std::uint64_t get(unsigned n){std::uint64_t v=0;unsigned i=0;for(auto x:take(n))v|=std::uint64_t(x)<<(8*i++);return v;}
    void done(){need(p==b.size());}
};
void put(std::vector<std::uint8_t>& b,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
void append(std::vector<std::uint8_t>& b,std::span<const std::uint8_t> v){b.insert(b.end(),v.begin(),v.end());}
void magic(std::vector<std::uint8_t>& b,const char* m){b.insert(b.end(),m,m+8);}
}
bool valid_lease_key(const LeaseKey& k) noexcept {
    return k.session&&k.package&&k.epoch&&k.nonce&&k.generation&&k.generation<std::numeric_limits<std::uint64_t>::max()&&
        k.revision==k.generation&&k.local_sequence&&k.local_sequence<=8&&k.generation>=k.local_sequence&&k.local_clock<=1000000000ull&&k.local_clock<=k.clock;
}
std::vector<std::uint8_t> lease_key_bytes(const LeaseKey& k){
    need(valid_lease_key(k));std::vector<std::uint8_t> out;out.reserve(136);
    for(auto v:{k.session,k.package,k.epoch,k.generation,k.clock,k.local_sequence,k.local_clock,k.revision,k.nonce})put(out,v,8);
    append(out,k.model);append(out,k.predecessor);return out;
}
LeaseKey lease_read_key(std::span<const std::uint8_t> b){
    Reader r{b};LeaseKey k;k.session=r.get(8);k.package=r.get(8);k.epoch=r.get(8);k.generation=r.get(8);k.clock=r.get(8);k.local_sequence=r.get(8);k.local_clock=r.get(8);k.revision=r.get(8);k.nonce=r.get(8);
    auto model=r.take(32),previous=r.take(32);std::copy(model.begin(),model.end(),k.model.begin());std::copy(previous.begin(),previous.end(),k.predecessor.begin());r.done();need(valid_lease_key(k));return k;
}
bool lease_successor(const LeaseState* previous,const LeaseKey& k,const Digest& digest,std::uint64_t base_generation) noexcept {
    if(!valid_lease_key(k)||k.predecessor!=digest)return false;
    if(!previous){constexpr Digest empty{0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55};
        return base_generation<UINT64_MAX-1&&k.generation==base_generation+1&&k.local_sequence==1&&k.local_clock==0&&k.clock==0&&digest==empty;}
    const auto& p=previous->key;
    if(k.session!=p.session||k.package!=p.package||k.model!=p.model||k.generation!=p.generation+1||k.clock<=p.clock||k.epoch<p.epoch)return false;
    auto delta=k.clock-p.clock;if(delta>8ull*16666667||delta+previous->physics.remainder>8ull*16666667)return false;
    return k.epoch==p.epoch?(k.local_sequence==p.local_sequence+1&&k.nonce==p.nonce&&k.local_clock==p.local_clock+delta):
        (k.local_sequence==1&&k.local_clock==delta);
}
std::vector<std::uint8_t> encode_lease_checkpoint(const LeaseKey& k,const PhysicsCheckpoint& p){
    need(p.generation==k.local_sequence&&p.clock==k.local_clock&&p.model==k.model);
    auto inner=encode_checkpoint(p);need(inner.size()<=checkpoint_cap-152&&validate_checkpoint(inner,k.model,k.local_sequence,k.local_clock));
    std::vector<std::uint8_t> out;magic(out,"ARSPHY02");append(out,lease_key_bytes(k));put(out,inner.size(),4);put(out,0,4);append(out,inner);return out;
}
bool decode_lease_checkpoint(std::span<const std::uint8_t> b,LeaseState& output) noexcept {
    try{need(b.size()>=224&&b.size()<=checkpoint_cap);Reader r{b};need(std::memcmp(r.take(8).data(),"ARSPHY02",8)==0);LeaseState next;next.key=lease_read_key(r.take(136));auto size=r.get(4);need(r.get(4)==0);auto inner=r.take(static_cast<std::size_t>(size));r.done();
        need(decode_checkpoint(inner,next.key.model,next.key.local_sequence,next.key.local_clock,next.physics));output=std::move(next);return true;
    }catch(...){return false;}
}
LeaseCommand decode_lease_command(std::span<const std::uint8_t> b){
    Reader r{b};LeaseCommand c;c.key=lease_read_key(r.take(136));c.control=static_cast<std::uint32_t>(r.get(4));auto count=r.get(4);need(c.control<=3&&count<=32);std::set<std::uint32_t> seen;
    for(unsigned i=0;i<count;++i){auto slot=static_cast<std::uint32_t>(r.get(4));auto v=std::bit_cast<float>(static_cast<std::uint32_t>(r.get(4)));need(slot<4096&&seen.insert(slot).second&&std::isfinite(v)&&v>=-1&&v<=1);c.bindings.emplace_back(slot,v);}r.done();return c;
}
LeaseMessage read_lease_message(std::istream& in,std::size_t max_body){
    std::array<std::uint8_t,24> header{};need(bool(in.read(reinterpret_cast<char*>(header.data()),header.size())));Reader r{header};need(std::memcmp(r.take(8).data(),"ARSLES02",8)==0&&r.get(4)==2);LeaseMessage m;m.kind=static_cast<std::uint32_t>(r.get(4));auto size=r.get(8);need(m.kind>=1&&m.kind<=7&&size<=max_body);m.body.resize(static_cast<std::size_t>(size));need(bool(in.read(reinterpret_cast<char*>(m.body.data()),m.body.size())));return m;
}
void write_lease_message(std::ostream& out,std::uint32_t kind,std::span<const std::uint8_t> body){
    need(kind>=1&&kind<=7&&body.size()<=packet_cap-24);std::vector<std::uint8_t> head;magic(head,"ARSLES02");put(head,2,4);put(head,kind,4);put(head,body.size(),8);
    out.write(reinterpret_cast<const char*>(head.data()),head.size());out.write(reinterpret_cast<const char*>(body.data()),body.size());out.flush();need(bool(out));
}
std::vector<std::uint8_t> encode_lease_candidate(const LeaseKey& k,const Snapshot& scene,const AtlasSet& atlas,std::span<const std::uint8_t> checkpoint,std::uint32_t flags){
    auto size=encoded_size(scene,atlas);need(scene.generation==k.generation+1&&checkpoint.size()<=checkpoint_cap&&size<=packet_cap-176-checkpoint.size());
    std::vector<std::uint8_t> b;magic(b,"ARSCND02");put(b,2,4);put(b,0,4);put(b,176+size+checkpoint.size(),8);append(b,lease_key_bytes(k));put(b,size,8);put(b,checkpoint.size(),4);put(b,0,4);
    std::ostringstream packet(std::ios::binary);encode_frame(packet,scene,atlas,k.nonce,flags,5);auto raw=packet.str();append(b,{reinterpret_cast<const std::uint8_t*>(raw.data()),raw.size()});append(b,checkpoint);return b;
}
bool decode_lease_candidate(std::span<const std::uint8_t> b,const LeaseKey& expected,VerifiedLease& output) noexcept {
    try{need(b.size()>=176&&b.size()<=packet_cap);Reader r{b};need(std::memcmp(r.take(8).data(),"ARSCND02",8)==0&&r.get(4)==2&&r.get(4)==0&&r.get(8)==b.size());auto key=lease_read_key(r.take(136));need(key==expected);auto size=r.get(8),cp_size=r.get(4);need(r.get(4)==0&&size<=b.size()-r.p&&cp_size<=checkpoint_cap);auto frame=r.take(static_cast<std::size_t>(size)),cp=r.take(static_cast<std::size_t>(cp_size));r.done();
        need(validate_frame(frame,key.nonce,key.generation+1,5));auto state=std::make_shared<LeaseState>();need(decode_lease_checkpoint(cp,*state)&&state->key==key);
        VerifiedLease next;need(decode_frame(frame,key.nonce,key.generation+1,5,next.scene_));next.state_=std::move(state);next.bytes_=std::make_shared<const std::vector<std::uint8_t>>(cp.begin(),cp.end());output=std::move(next);return true;
    }catch(...){return false;}
}
}
