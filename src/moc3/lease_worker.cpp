#include "lease_worker.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
namespace ars::moc3::admission {
namespace {
void need(bool v){if(!v)throw std::runtime_error("LEASE_WORKER_REJECTED");}
std::uint64_t read(unsigned n){std::uint64_t v=0;for(unsigned i=0;i<n;++i){auto c=std::cin.get();need(c!=EOF);v|=std::uint64_t(c)<<(8*i);}return v;}
void put(std::vector<std::uint8_t>& b,std::uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
void append(std::vector<std::uint8_t>& b,std::span<const std::uint8_t> s){b.insert(b.end(),s.begin(),s.end());}
}
int lease_worker(Session& session,RetainedPhysics& physics,const AtlasSet& atlases,const Digest& model,std::uint64_t nonce,std::uint32_t flags){
    try{
        const auto owner=read(8),package=read(8),epoch=read(8),base_generation=read(8),size=read(4);need(owner&&package&&epoch&&base_generation<UINT64_MAX-1&&size<=checkpoint_cap);
        std::vector<std::uint8_t> accepted(static_cast<std::size_t>(size));need(bool(std::cin.read(reinterpret_cast<char*>(accepted.data()),accepted.size())));
        LeaseState previous;bool has_previous=!accepted.empty();
        if(has_previous){need(decode_lease_checkpoint(accepted,previous));auto& k=previous.key;
            need(k.session==owner&&k.package==package&&k.epoch<epoch&&k.model==model);physics.restore_epoch(previous.physics);}
        auto digest=sha256(accepted);std::vector<ParameterInfo> catalog;need(session.parameter_info(catalog)==Result::ok&&catalog.size()<=4096);
        std::set<std::string> names;std::vector<std::uint8_t> ready;append(ready,model);append(ready,digest);put(ready,owner,8);put(ready,package,8);put(ready,epoch,8);put(ready,base_generation,8);put(ready,catalog.size(),4);
        for(const auto& p:catalog){need(names.insert(p.id).second&&std::isfinite(p.minimum)&&std::isfinite(p.default_value)&&std::isfinite(p.maximum)&&p.minimum>=-1e6&&p.maximum<=1e6&&p.minimum<=p.default_value&&p.default_value<=p.maximum);
            for(float v:{p.minimum,p.default_value,p.maximum})put(ready,std::bit_cast<std::uint32_t>(v),4);}
        write_lease_message(std::cout,1,ready);
        unsigned commands=0;bool pending=false;LeaseKey pending_key;RetainedPhysics::Candidate prepared;std::vector<std::uint8_t> checkpoint;
        for(;;){auto message=read_lease_message(std::cin,4096);need(++commands<=18);
            if(message.kind==6){need(message.body.empty()&&!pending);write_lease_message(std::cout,7,{});return 0;}
            if(message.kind==4){need(pending&&message.body.size()==168&&lease_read_key(std::span(message.body).first(136))==pending_key);
                auto hash=sha256(checkpoint);need(std::equal(hash.begin(),hash.end(),message.body.begin()+136));physics.commit(prepared);
                accepted=std::move(checkpoint);digest=hash;need(decode_lease_checkpoint(accepted,previous));has_previous=true;pending=false;
                write_lease_message(std::cout,5,message.body);continue;}
            need(message.kind==2&&!pending);auto command=decode_lease_command(message.body);const auto& k=command.key;
            need(k.session==owner&&k.package==package&&k.epoch==epoch&&k.model==model&&k.nonce==nonce&&lease_successor(has_previous?&previous:nullptr,k,digest,base_generation));
            std::vector<Parameter> manual;
            for(auto [slot,v]:command.bindings){need(slot<catalog.size());auto& p=catalog[slot];double value=double(p.default_value)+double(v)*(v>=0?double(p.maximum)-p.default_value:double(p.default_value)-p.minimum);
                manual.push_back({p.id,std::clamp(static_cast<float>(value),p.minimum,p.maximum)});}
            prepared=physics.prepare(k.local_clock,manual,command.control&1,command.control&2);
            checkpoint=encode_lease_checkpoint(k,physics.checkpoint(prepared,model));need(session.update_complete(prepared.parameters())==Result::ok);
            auto snapshot=*session.current();snapshot.generation=k.generation+1;auto packet=encode_lease_candidate(k,snapshot,atlases,checkpoint,flags);
            pending_key=k;pending=true;write_lease_message(std::cout,3,packet);
        }
    }catch(...){return 42;}
}
}
