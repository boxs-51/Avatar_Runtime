#include "core_consumer.hpp"
#include "../motion/cpu_snapshot.hpp"
#include "../motion/semantic_candidate.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>

namespace ars::moc3 {
namespace {
std::array<char,129> semantic(std::size_t i){
    std::array<char,129> id{};std::snprintf(id.data(),id.size(),"control.%u",static_cast<unsigned>(i));return id;
}
}
CoreConsumer::~CoreConsumer(){if(opened_&&!closed_&&close()!=ARS_OK)std::terminate();}
ars_status CoreConsumer::open(std::span<const ParameterInfo> mapping,bool recoverable){
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(opened_||closed_||mapping.empty()||mapping.size()>32)return ARS_E_INVALID_ARGUMENT;
    for(std::size_t i=0;i<mapping.size();++i){
        const auto& p=mapping[i];
        if(p.id.empty()||p.id.size()>255||!std::isfinite(p.minimum)||!std::isfinite(p.default_value)||
           !std::isfinite(p.maximum)||p.minimum>p.default_value||p.default_value>p.maximum)return ARS_E_INVALID_ARGUMENT;
        for(std::size_t j=0;j<i;++j)if(mapping[j].id==p.id)return ARS_E_INVALID_ARGUMENT;
    }
    recoverable_=recoverable;
    try{mapping_.assign(mapping.begin(),mapping.end());}catch(const std::bad_alloc&){return ARS_E_OUT_OF_BUDGET;}
    ars_runtime_desc desc{};desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;
    auto status=router_.create(desc,&runtime_);if(status!=ARS_OK)return status;
    opened_=true;
    std::array<security::SemanticChannel,32> catalog{};
    for(std::size_t i=0;i<mapping_.size();++i){catalog[i].id=semantic(i);catalog[i].neutral[0]=mapping_[i].default_value;}
    status=router_.bind_character(runtime_,{catalog.data(),mapping_.size()},&character_);
    if(status!=ARS_OK)return status;
    std::array<ars_utf8_view,32> allowed{};
    for(std::size_t i=0;i<mapping_.size();++i)allowed[i]={catalog[i].id.data(),static_cast<std::uint32_t>(std::strlen(catalog[i].id.data()))};
    ars_source_desc source{};source.struct_size=sizeof(source);source.abi_version=ARS_ABI_VERSION;
    source.allowed_channel_ids=allowed.data();source.allowed_channel_count=static_cast<std::uint32_t>(mapping_.size());
    source.max_channels_per_frame=source.allowed_channel_count;source.max_frames_per_second=240;
    source.lease_ttl_ns=security::max_ttl;
    return router_.register_source(runtime_,character_,&source,&source_,&token_);
}
ars_status CoreConsumer::prepare(std::uint64_t sequence,std::span<const Parameter> input,
                                std::vector<Parameter>& out,std::uint64_t &generation){
    generation=0;
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(!opened_||closed_||faulted_)return ARS_E_CLOSED;
    if(pending_)return ARS_E_BUSY;
    if(input.size()>mapping_.size())return ARS_E_INVALID_ARGUMENT;
    std::array<float,32> values{};
    for(std::size_t i=0;i<mapping_.size();++i)values[i]=mapping_[i].default_value;
    std::array<bool,32> seen{};
    for(const auto& p:input){
        auto found=std::find_if(mapping_.begin(),mapping_.end(),[&](const auto& m){return m.id==p.id;});
        if(found==mapping_.end()||!std::isfinite(p.value)||p.value<found->minimum||p.value>found->maximum)return ARS_E_INVALID_ARGUMENT;
        const auto i=static_cast<std::size_t>(found-mapping_.begin());
        if(seen[i])return ARS_E_INVALID_ARGUMENT;seen[i]=true;values[i]=p.value;
    }
    // Allocate all caller-owned publication data before core ingress consumes a sequence.
    std::vector<Parameter> result;
    try{result.resize(mapping_.size());
        for(std::size_t i=0;i<result.size();++i)result[i].id=mapping_[i].id;
    }catch(const std::bad_alloc&){return ARS_E_OUT_OF_BUDGET;}
    std::array<std::array<char,129>,32> ids{};std::array<ars_motion_channel,32> channels{};
    for(std::size_t i=0;i<mapping_.size();++i){ids[i]=semantic(i);channels[i].semantic_id={ids[i].data(),static_cast<std::uint32_t>(std::strlen(ids[i].data()))};
        channels[i].kind=ARS_VALUE_SCALAR;channels[i].value[0]=values[i];}
    ars_motion_frame frame{};frame.struct_size=sizeof(frame);frame.abi_version=ARS_ABI_VERSION;
    frame.sequence=sequence;frame.timestamp_mode=ARS_TIMESTAMP_ON_ARRIVAL;frame.confidence=1;
    frame.channel_count=static_cast<std::uint32_t>(mapping_.size());frame.channels=channels.data();
    auto status=router_.submit_frame(runtime_,token_,&frame);if(status!=ARS_OK)return status;
    motion::Program program;program.count=static_cast<std::uint32_t>(mapping_.size()*2);
    for(std::uint32_t i=0;i<mapping_.size();++i){
        program.nodes[2*i]={motion::Op::input,0,0,i,0};program.nodes[2*i+1]={motion::Op::output,2*i,0,i};}
    status=router_.prepare_cpu(runtime_,character_,std::span(&source_,1),program,&candidate_);
    if(status!=ARS_OK){faulted_=true;return status;}
    status=router_.copy_cpu_candidate(candidate_,{values.data(),mapping_.size()},&generation);
    if(status!=ARS_OK){candidate_.reset();faulted_=true;return status;}
    for(std::size_t i=0;i<result.size();++i)result[i].value=values[i];
    out.swap(result);pending_=true;return ARS_OK;
}
ars_status CoreConsumer::publish(){
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(!pending_)return ARS_E_INVALID_ARGUMENT;
    const auto status=router_.publish_cpu(&candidate_);pending_=false;if(status!=ARS_OK&&!recoverable_)faulted_=true;return status;
}
ars_status CoreConsumer::revoke(){
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(!opened_||closed_)return ARS_E_CLOSED;
    return router_.revoke_source(runtime_,source_);
}
void CoreConsumer::discard() noexcept{
    if(std::this_thread::get_id()!=owner_)std::terminate();
    if(pending_&&!recoverable_)faulted_=true;
    candidate_.reset();pending_=false;
}
ars_status CoreConsumer::read(std::vector<Parameter>& out,std::uint64_t &generation){
    generation=0;
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(!opened_||closed_)return ARS_E_CLOSED;
    core::SnapshotLease lease;auto status=router_.read_cpu(runtime_,character_,&lease);if(status!=ARS_OK)return status;
    try{std::vector<Parameter> result;result.reserve(mapping_.size());
        for(std::size_t i=0;i<mapping_.size();++i)result.push_back({mapping_[i].id,lease.get()->outputs[i].scalar});
        generation=lease.get()->generation;out.swap(result);return ARS_OK;
    }catch(const std::bad_alloc&){return ARS_E_OUT_OF_BUDGET;}
}
ars_status CoreConsumer::retain_graphics(std::shared_ptr<const void>& out){
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(!opened_||closed_)return ARS_E_CLOSED;
    if(out)return ARS_E_INVALID_ARGUMENT;
    try{auto pin=std::make_shared<core::RuntimeLease>();
        const auto status=router_.acquire(runtime_,core::LeaseKind::work,pin.get());
        if(status==ARS_OK)out=std::move(pin);return status;
    }catch(const std::bad_alloc&){return ARS_E_OUT_OF_BUDGET;}
}
ars_status CoreConsumer::close(){
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(!opened_||closed_)return ARS_E_CLOSED;
    discard();auto status=router_.begin_close(runtime_);if(status!=ARS_OK&&status!=ARS_E_CLOSED)return status;
    status=router_.finish_close(runtime_);if(status==ARS_OK)closed_=true;return status;
}
}
