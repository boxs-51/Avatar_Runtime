#include "step_packet.hpp"
#include <cstring>
#include <ostream>
#include <stdexcept>
namespace ars::moc3::admission {
namespace {
void need(bool value){if(!value)throw std::runtime_error("STEP_PACKET_REJECTED");}
struct Reader {std::span<const std::uint8_t> b;std::size_t p=0;
    auto take(std::size_t n){need(n<=b.size()-p);auto out=b.subspan(p,n);p+=n;return out;}
    std::uint64_t integer(unsigned n){auto s=take(n);std::uint64_t out=0;for(unsigned i=0;i<n;++i)out|=std::uint64_t(s[i])<<(8*i);return out;}};
void put(std::ostream& out,std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)out.put(static_cast<char>(value>>(8*i)));}
}
bool decode_step(std::span<const std::uint8_t> bytes,std::uint64_t nonce,std::uint64_t sequence,std::uint64_t clock,const Digest& model,const Digest& predecessor,VerifiedStep& out) noexcept {
    try{
        need(bytes.size()>=120&&bytes.size()<=packet_cap&&nonce&&sequence&&sequence<=8&&clock<=1000000000ull&&(sequence!=1||clock==0));Reader r{bytes};
        need(std::memcmp(r.take(8).data(),"ARSCAND1",8)==0&&r.integer(4)==1&&r.integer(4)==0&&r.integer(8)==nonce&&r.integer(8)==sequence&&r.integer(8)==clock);
        auto size=r.integer(8),checkpoint_size=r.integer(4);need(r.integer(4)==0&&checkpoint_size<=checkpoint_cap);
        need(std::memcmp(r.take(32).data(),model.data(),32)==0&&std::memcmp(r.take(32).data(),predecessor.data(),32)==0);
        need(size<=bytes.size()-r.p);auto frame=r.take(static_cast<std::size_t>(size));auto state=r.take(static_cast<std::size_t>(checkpoint_size));need(r.p==bytes.size());
        // Validate ALL typed data before decoding any scene or state allocation.
        need(validate_checkpoint(state,model,sequence,clock)&&validate_frame(frame,nonce,sequence+1,4));
        auto checkpoint=std::make_shared<PhysicsCheckpoint>();need(decode_checkpoint(state,model,sequence,clock,*checkpoint));
        VerifiedStep candidate;need(decode_frame(frame,nonce,sequence+1,4,candidate.scene_));candidate.state_=std::move(checkpoint);
        candidate.bytes_=std::make_shared<const std::vector<std::uint8_t>>(state.begin(),state.end());out=std::move(candidate);return true;
    }catch(...){return false;}
}
void encode_step(std::ostream& out,const Snapshot& scene,const AtlasSet& images,std::uint64_t nonce,std::uint64_t sequence,std::uint64_t clock,const Digest& model,const Digest& predecessor,std::span<const std::uint8_t> checkpoint,std::uint32_t flags){
    auto size=encoded_size(scene,images);need(checkpoint.size()<=checkpoint_cap&&size<=packet_cap-120-checkpoint.size());
    out.write("ARSCAND1",8);put(out,1,4);put(out,0,4);put(out,nonce,8);put(out,sequence,8);put(out,clock,8);put(out,size,8);put(out,checkpoint.size(),4);put(out,0,4);
    out.write(reinterpret_cast<const char*>(model.data()),32);out.write(reinterpret_cast<const char*>(predecessor.data()),32);
    encode_frame(out,scene,images,nonce,flags,4);out.write(reinterpret_cast<const char*>(checkpoint.data()),checkpoint.size());need(bool(out));
}
}
