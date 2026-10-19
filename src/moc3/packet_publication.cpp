#include "packet_publication.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <d3d11sdklayers.h>
namespace ars::moc3 {
using GResult=graphics::Result;
namespace {
void need(bool ok){if(!ok)throw std::runtime_error("PACKET_PUBLICATION_FAILURE");}
View fit(const Snapshot& s){
    float x0=std::numeric_limits<float>::max(),y0=x0,x1=-x0,y1=-x0;
    for(auto& d:s.drawables)for(auto& v:d.vertices){x0=std::min(x0,v.x);x1=std::max(x1,v.x);y0=std::min(y0,v.y);y1=std::max(y1,v.y);}
    auto span=std::max(x1-x0,y1-y0);if(!std::isfinite(span)||span<=0)return {};
    auto scale=1.8f/span;return {scale,scale,-(x0+x1)*.5f*scale,-(y0+y1)*.5f*scale};
}
}
struct PacketPublication::Fence final:Host::PublicationFence {
    PacketPublication* owner;
    std::chrono::steady_clock::time_point deadline;
    std::uint64_t cursor=0;
    bool cancelled=false,committed=false;
    std::shared_ptr<const admission::PhysicsCheckpoint> checkpoint;
    std::shared_ptr<const std::vector<std::uint8_t>> checkpoint_bytes;
    Fence(PacketPublication* p,std::chrono::steady_clock::time_point until):owner(p),deadline(until){}
    GResult commit() noexcept override {
        if(cancelled||std::chrono::steady_clock::now()>deadline){owner->core_.discard();return GResult::failed;}
        if(owner->core_.publish()!=ARS_OK)return GResult::failed;
        // No allocation/fallible operation after successful core publication.
        if(checkpoint){owner->accepted_state_=checkpoint;owner->accepted_state_bytes_=checkpoint_bytes;}
        owner->accepted_cursor_=cursor;committed=true;return GResult::ok;
    }
};
PacketPublication::PacketPublication(bool hardware,graphics::d3d11::QueryOps* ops):ops_(ops),hardware_(hardware){}
PacketPublication::~PacketPublication(){if(!closed_&&close()!=GResult::ok)std::terminate();}
ars_status PacketPublication::open(){const ParameterInfo channel{"request.normalized",-1,0,1};auto result=core_.open(std::span(&channel,1));if(result==ARS_OK)opened_=true;return result;}
ars_status PacketPublication::begin(std::uint64_t sequence,std::uint64_t nonce,std::uint32_t index,std::uint64_t clock,float value,std::uint32_t lease_ms){
    if(owner_!=std::this_thread::get_id())return ARS_E_WRONG_THREAD;
    if(step_session_)return ARS_E_INVALID_ARGUMENT;
    return prepare_request(sequence,nonce,index,clock,value,lease_ms);
}
ars_status PacketPublication::prepare_request(std::uint64_t sequence,std::uint64_t nonce,std::uint32_t index,std::uint64_t clock,float value,std::uint32_t lease_ms){
    if(owner_!=std::this_thread::get_id())return ARS_E_WRONG_THREAD;
    if(terminal_||closed_)return ARS_E_CLOSED;
    if(prepared_)return ARS_E_BUSY;
    if(!nonce||index>=8||sequence!=index+1||clock>1000000000ull||!std::isfinite(value)||value< -1||value>1||lease_ms<100||lease_ms>2000)return ARS_E_INVALID_ARGUMENT;
    auto fence=std::make_shared<Fence>(this,std::chrono::steady_clock::now()+std::chrono::milliseconds(lease_ms));
    const Parameter input{"request.normalized",value};std::vector<Parameter> candidate;std::uint64_t generation=0;
    auto status=core_.prepare(sequence,std::span(&input,1),candidate,generation);if(status!=ARS_OK)return status;
    if(generation!=sequence){core_.discard();terminal_=true;return ARS_E_INVALID_ARGUMENT;}
    sequence_=sequence;nonce_=nonce;index_=index;clock_=clock;fence_=std::move(fence);prepared_=true;return ARS_OK;
}
GResult PacketPublication::fail() noexcept {core_.discard();prepared_=false;terminal_=true;fence_.reset();return GResult::failed;}
GResult PacketPublication::stage(std::span<const std::uint8_t> bytes,bool fail_target){
    if(owner_!=std::this_thread::get_id())return GResult::wrong_thread;
    if(step_session_||!prepared_||terminal_||closed_||(host_&&host_->pending()))return GResult::invalid_argument;
    admission::VerifiedSequence batch;
    if(!admission::decode_sequence(bytes,nonce_,batch)||index_>=batch.size()||batch.timestamp(index_)!=clock_||std::chrono::steady_clock::now()>fence_->deadline)return fail();
    auto scene=batch.frame(index_);fence_->cursor=scene.snapshot()->generation;
    return submit(scene,*batch.frame(0).snapshot(),fail_target);
}
GResult PacketPublication::submit(const admission::VerifiedScene& scene,const Snapshot& reference,bool fail_target){
    try{
        if(!host_){
            auto candidate_view=fit(reference);D3D_FEATURE_LEVEL level{};bool debug=true;++device_calls_;
            auto hr=D3D11CreateDevice(nullptr,hardware_?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_);
            if(FAILED(hr)){debug=false;++device_calls_;hr=D3D11CreateDevice(nullptr,hardware_?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_);}
            if(FAILED(hr))return fail();
            if(hardware_){Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
                if(FAILED(device_.As(&dxgi))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetDesc(&desc))||desc.VendorId==0x1414)return fail();}
            (void)debug;host_=std::make_unique<Host>(device_.Get(),context_.Get(),101,ops_);view_=candidate_view;
        }
        auto pass=[&](graphics::d3d11::HostDevice& backend,ID3D11RenderTargetView* target,graphics::TargetInfo info,graphics::d3d11::QueryRetirement& query){
            return render_offscreen_moc3(backend,target,info,scene.snapshot(),scene.atlases(),view_,query,&probe_);};
        auto result=host_->request_moc(pass,sequence_,512,512,fail_target,fence_);
        if(result!=GResult::ok){if(host_->pending())return result;return fail();}
        return result;
    }catch(...){if(host_&&host_->pending())return GResult::failed;return fail();}
}
ars_status PacketPublication::begin_step(std::uint64_t sequence,std::uint64_t nonce,std::uint64_t clock,float value,std::uint32_t lease_ms,const admission::Digest& model){
    if(owner_!=std::this_thread::get_id())return ARS_E_WRONG_THREAD;
    if(sequence!=(accepted_state_?accepted_state_->generation+1:1))return ARS_E_INVALID_ARGUMENT;
    if(accepted_state_&&(model!=accepted_state_->model||clock<=accepted_state_->clock||clock-accepted_state_->clock+accepted_state_->remainder>8ull*16666667))return ARS_E_INVALID_ARGUMENT;
    if(!accepted_state_&&clock!=0)return ARS_E_INVALID_ARGUMENT;
    auto predecessor=admission::sha256(checkpoint_bytes()); // Before core ingress consumes sequence.
    auto result=prepare_request(sequence,nonce,static_cast<std::uint32_t>(sequence-1),clock,value,lease_ms);
    if(result==ARS_OK){step_session_=true;request_model_=model;predecessor_=predecessor;}return result;
}
GResult PacketPublication::stage_step(std::span<const std::uint8_t> bytes,bool fail_target){
    if(owner_!=std::this_thread::get_id())return GResult::wrong_thread;
    if(!step_session_||!prepared_||terminal_||closed_||(host_&&host_->pending()))return GResult::invalid_argument;
    admission::VerifiedStep candidate;
    if(!admission::decode_step(bytes,nonce_,sequence_,clock_,request_model_,predecessor_,candidate)||std::chrono::steady_clock::now()>fence_->deadline)return fail();
    if(accepted_state_){auto& next=candidate.checkpoint()->particles;auto& previous=accepted_state_->particles;
        if(next.size()!=previous.size())return fail();for(std::size_t i=0;i<next.size();++i)if(next[i].size()!=previous[i].size())return fail();}
    fence_->checkpoint=candidate.checkpoint();fence_->checkpoint_bytes=candidate.checkpoint_bytes();fence_->cursor=sequence_+1;
    return submit(candidate.scene(),*candidate.scene().snapshot(),fail_target);
}
const std::vector<std::uint8_t>& PacketPublication::checkpoint_bytes() const {
    need(owner_==std::this_thread::get_id());static const std::vector<std::uint8_t> empty;
    return accepted_state_bytes_?*accepted_state_bytes_:empty;
}
GResult PacketPublication::poll(){
    if(owner_!=std::this_thread::get_id())return GResult::wrong_thread;
    if(!prepared_||!host_||!host_->pending())return GResult::invalid_argument;
    auto result=host_->poll_commit();if(result==GResult::busy)return result;
    if(host_->pending())return result; // Unknown completion: caller must quarantine.
    if(result!=GResult::ok)return fail();
    accepted_texture_=host_->texture();prepared_=false;fence_.reset();return GResult::ok;
}
void PacketPublication::cancel() noexcept {
    if(owner_!=std::this_thread::get_id())std::terminate();
    if(!prepared_)return;
    terminal_=true;
    if(host_&&host_->pending())fence_->cancelled=true;else fail();
}
ars_status PacketPublication::revoke(){return core_.revoke();}
PacketPublication::State PacketPublication::state(){
    State out;if(owner_!=std::this_thread::get_id()){out.core_status=ARS_E_WRONG_THREAD;return out;}
    std::vector<Parameter> values;out.core_status=core_.read(values,out.core_generation);if(out.core_status==ARS_OK)out.core_value=values.at(0).value;
    if(host_){out.host_generation=host_->pose_generation();out.epoch=host_->target_epoch();out.pending=host_->pending();out.target_matches_accepted=host_->texture()==accepted_texture_;}
    else out.target_matches_accepted=accepted_texture_==nullptr;
    out.physics_cursor=accepted_cursor_;out.terminal=terminal_;out.device_calls=device_calls_;out.gpu_creations=probe_.creations;return out;
}
std::vector<std::uint8_t> PacketPublication::read_pixels(){
    need(owner_==std::this_thread::get_id()&&host_&&host_->texture()&&host_->texture()==accepted_texture_);
    D3D11_TEXTURE2D_DESC desc{};host_->texture()->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;need(SUCCEEDED(device_->CreateTexture2D(&desc,nullptr,&staging)));context_->CopyResource(staging.Get(),host_->texture());
    std::vector<std::uint8_t> rgba(512*512*4);D3D11_MAPPED_SUBRESOURCE mapped{};need(SUCCEEDED(context_->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)));
    for(unsigned y=0;y<512;++y)std::copy_n(static_cast<std::uint8_t*>(mapped.pData)+y*mapped.RowPitch,512*4,rgba.data()+y*512*4);context_->Unmap(staging.Get(),0);return rgba;
}
GResult PacketPublication::close(){
    if(owner_!=std::this_thread::get_id())return GResult::wrong_thread;
    if(closed_)return GResult::ok;
    cancel();if(host_){
        if(host_->pending()){auto r=poll();if(r==GResult::busy||host_->pending())return r;}
        host_->begin_close();auto r=host_->poll_close();if(r!=GResult::ok)return r;
        Microsoft::WRL::ComPtr<ID3D11InfoQueue> messages;device_.As(&messages);
        for(UINT64 i=0;messages&&i<messages->GetNumStoredMessages();++i){SIZE_T n=0;messages->GetMessage(i,nullptr,&n);std::vector<unsigned char> data(n);auto* m=reinterpret_cast<D3D11_MESSAGE*>(data.data());
            if(FAILED(messages->GetMessage(i,m,&n))||m->Severity<=D3D11_MESSAGE_SEVERITY_WARNING)return GResult::failed;}
    }
    if(opened_&&core_.close()!=ARS_OK)return GResult::failed;closed_=true;return GResult::ok;
}
}
