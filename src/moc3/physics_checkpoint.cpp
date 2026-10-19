#include "physics_checkpoint.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ars::moc3::admission {
namespace {
void need(bool ok){if(!ok)throw std::runtime_error("CHECKPOINT_REJECTED");}
struct Hash {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;std::vector<std::uint8_t> scratch;
    Hash(){
        try{need(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0);ULONG size=0,used=0;
            need(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&used,0)>=0&&size&&size<=1024*1024);
            scratch.resize(size);need(BCryptCreateHash(algorithm,&hash,scratch.data(),size,nullptr,0,0)>=0);
        }catch(...){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);throw;}
    }
    ~Hash(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}
    void add(std::span<const std::uint8_t> bytes){need(bytes.size()<=std::numeric_limits<ULONG>::max());if(!bytes.empty())need(BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)>=0);}
    Digest finish(){Digest out{};need(BCryptFinishHash(hash,out.data(),out.size(),0)>=0);return out;}
};
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t pos=0;
    std::span<const std::uint8_t> take(std::size_t n){need(n<=bytes.size()-pos);auto out=bytes.subspan(pos,n);pos+=n;return out;}
    std::uint64_t integer(unsigned n){auto b=take(n);std::uint64_t value=0;for(unsigned i=0;i<n;++i)value|=std::uint64_t(b[i])<<(8*i);return value;}
};
void walk(std::span<const std::uint8_t> bytes,const Digest& model,std::uint64_t generation,std::uint64_t clock,PhysicsCheckpoint* out){
    need(bytes.size()>=72&&bytes.size()<=checkpoint_cap);Reader r{bytes};need(std::memcmp(r.take(8).data(),"ARSPHYS1",8)==0&&r.integer(4)==1);
    auto n=r.integer(4);need(n&&n<=128);need(generation&&generation<=8&&r.integer(8)==generation&&clock<=1000000000ull&&r.integer(8)==clock);
    auto remainder=r.integer(8);need(remainder<16666667);auto key=r.take(32);need(std::memcmp(key.data(),model.data(),32)==0);
    if(out){out->model=model;out->generation=generation;out->clock=clock;out->remainder=remainder;out->particles.reserve(n);}
    std::uint64_t total=0;
    for(std::size_t k=0;k<n;++k){auto count=r.integer(4);need(count>=2&&count<=2048&&count<=2048-total);total+=count;need(count*32<=bytes.size()-r.pos);
        std::vector<std::array<double,4>> chain;if(out)chain.reserve(count);
        for(std::size_t j=0;j<count;++j){std::array<double,4> point{};for(auto& value:point){value=std::bit_cast<double>(r.integer(8));need(std::isfinite(value)&&std::abs(value)<=1e6);}
            if(out)chain.push_back(point);}
        if(out)out->particles.push_back(std::move(chain));
    }
    need(r.pos==bytes.size());
}
void put(std::vector<std::uint8_t>& bytes,std::uint64_t value,unsigned n){for(unsigned i=0;i<n;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
}
Digest sha256(std::span<const std::uint8_t> bytes){Hash hash;hash.add(bytes);return hash.finish();}
Digest package_identity(std::span<const std::uint8_t> bytes,std::span<const std::uint8_t> entry){
    need(!entry.empty()&&entry.size()<=512);std::array<std::uint8_t,4> n{};for(unsigned i=0;i<4;++i)n[i]=static_cast<std::uint8_t>(entry.size()>>(8*i));Hash hash;hash.add(n);hash.add(entry);hash.add(bytes);return hash.finish();
}
bool validate_checkpoint(std::span<const std::uint8_t> bytes,const Digest& model,std::uint64_t generation,std::uint64_t clock) noexcept {
    try{walk(bytes,model,generation,clock,nullptr);return true;}catch(...){return false;}
}
bool decode_checkpoint(std::span<const std::uint8_t> bytes,const Digest& model,std::uint64_t generation,std::uint64_t clock,PhysicsCheckpoint& out) noexcept {
    try{walk(bytes,model,generation,clock,nullptr);PhysicsCheckpoint next;walk(bytes,model,generation,clock,&next);out=std::move(next);return true;}catch(...){return false;}
}
std::vector<std::uint8_t> encode_checkpoint(const PhysicsCheckpoint& state){
    std::vector<std::uint8_t> out{'A','R','S','P','H','Y','S','1'};put(out,1,4);put(out,state.particles.size(),4);put(out,state.generation,8);put(out,state.clock,8);put(out,state.remainder,8);out.insert(out.end(),state.model.begin(),state.model.end());
    need(state.particles.size()<=128);std::size_t total=0;for(auto& chain:state.particles){need(chain.size()<=2048-total);total+=chain.size();put(out,chain.size(),4);for(auto& point:chain)for(double value:point)put(out,std::bit_cast<std::uint64_t>(value),8);}
    need(validate_checkpoint(out,state.model,state.generation,state.clock));return out;
}
}
