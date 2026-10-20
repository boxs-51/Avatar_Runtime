#include "lease_publication.hpp"
#ifdef _WIN32
#include <algorithm>
#include <bit>
#include <d3d11sdklayers.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace ars::moc3 {
using GResult=graphics::Result;using admission::LeaseKey;
namespace {
void need(bool v){if(!v)throw std::runtime_error("LEASE_PUBLICATION_FAILURE");}
View fit(const Snapshot& s){float x0=std::numeric_limits<float>::max(),y0=x0,x1=-x0,y1=-x0;
    for(const auto& d:s.drawables)for(const auto& v:d.vertices){x0=std::min(x0,v.x);x1=std::max(x1,v.x);y0=std::min(y0,v.y);y1=std::max(y1,v.y);}
    auto width=std::max(x1-x0,y1-y0);need(std::isfinite(width)&&width>0);float scale=1.8f/width;return {scale,scale,-(x0+x1)*.5f*scale,-(y0+y1)*.5f*scale};}
}
struct LeasePublication::Fence final:rendering::D3D11DualModeHost::PublicationFence {
    LeasePublication* owner;admission::LeaseCommand command;
    std::chrono::steady_clock::time_point deadline;bool cancelled=false;View view{};
    std::shared_ptr<const admission::LeaseState> state;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
    Fence(LeasePublication* p,admission::LeaseCommand c,std::uint32_t ms):owner(p),command(std::move(c)),deadline(std::chrono::steady_clock::now()+std::chrono::milliseconds(ms)){}
    GResult commit() noexcept override {
        if(cancelled||std::chrono::steady_clock::now()>deadline){owner->core_.discard();return GResult::failed;}
        if(owner->core_.publish()!=ARS_OK){owner->core_.discard();return GResult::failed;}
        // All allocations/validation preceded the actual core publish.
        owner->accepted_=state;owner->bytes_=bytes;owner->generation_=command.key.generation;owner->view_=view;
        return GResult::ok;
    }
};
LeasePublication::~LeasePublication(){if(!closed_&&close()!=GResult::ok)std::terminate();}
ars_status LeasePublication::open(std::uint64_t session){
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(opened_||closed_||closing_||!session)return ARS_E_INVALID_ARGUMENT;
    std::array<ParameterInfo,32> mapping;for(std::size_t i=0;i<mapping.size();++i)mapping[i]={"manual."+std::to_string(i),-1,0,1};
    auto result=core_.open(mapping,true);if(result==ARS_OK){opened_=true;session_=session;}return result;
}
GResult LeasePublication::reject() noexcept {
    core_.discard();if(fence_){auto epoch=fence_->command.key.epoch;minimum_epoch_=epoch+1;}
    fence_.reset();pending_=false;return GResult::failed;
}
ars_status LeasePublication::begin(std::span<const std::uint8_t> body,std::uint32_t ms){
    if(std::this_thread::get_id()!=owner_)return ARS_E_WRONG_THREAD;
    if(!opened_||closed_||closing_||terminal_)return ARS_E_CLOSED;if(pending_||(host_&&host_->pending()))return ARS_E_BUSY;
    if(ms<100||ms>2000)return ARS_E_INVALID_ARGUMENT;
    try{auto request=admission::decode_lease_command(body);const auto& k=request.key;
        if(k.session!=session_||k.epoch<minimum_epoch_||k.epoch==UINT64_MAX||k.generation!=generation_+1)return ARS_E_INVALID_ARGUMENT;
        bool replacement=accepted_&&k.package!=accepted_->key.package;
        if(replacement){if(k.package!=accepted_->key.package+1||k.epoch<=accepted_->key.epoch||!request.bindings.empty()||request.control!=3||!admission::lease_successor(nullptr,k,admission::sha256({}),generation_))return ARS_E_INVALID_ARGUMENT;}
        else if(accepted_){if(!admission::lease_successor(accepted_.get(),k,admission::sha256(*bytes_)))return ARS_E_INVALID_ARGUMENT;}
        else if(k.package!=1||!admission::lease_successor(nullptr,k,admission::sha256({}),generation_))return ARS_E_INVALID_ARGUMENT;
        auto fence=std::make_shared<Fence>(this,std::move(request),ms);std::vector<Parameter> input;
        for(std::size_t i=0;i<fence->command.bindings.size();++i)input.push_back({"manual."+std::to_string(i),fence->command.bindings[i].second});
        if(ingress_==UINT64_MAX)return ARS_E_CLOSED;std::vector<Parameter> result;std::uint64_t actual=0;
        auto status=core_.prepare(++ingress_,input,result,actual);if(status!=ARS_OK)return status;
        // Base/generation comes from actual CoreConsumer, never caller metadata.
        if(actual!=k.generation){core_.discard();return ARS_E_INVALID_ARGUMENT;}
        fence_=std::move(fence);pending_=true;return ARS_OK;
    }catch(const std::bad_alloc&){return ARS_E_OUT_OF_BUDGET;}catch(...){return ARS_E_INVALID_ARGUMENT;}
}
GResult LeasePublication::stage(std::span<const std::uint8_t> packet,bool fail_target){
    if(std::this_thread::get_id()!=owner_)return GResult::wrong_thread;
    if(closing_||closed_||!pending_||terminal_||(host_&&host_->pending()))return GResult::invalid_argument;
    admission::VerifiedLease candidate;
    if(!admission::decode_lease_candidate(packet,fence_->command.key,candidate)||std::chrono::steady_clock::now()>fence_->deadline)return reject();
    if(accepted_&&accepted_->key.package==candidate.state()->key.package){const auto& a=accepted_->physics.particles;const auto& b=candidate.state()->physics.particles;
        if(a.size()!=b.size())return reject();for(std::size_t i=0;i<a.size();++i)if(a[i].size()!=b[i].size())return reject();}
    try{
        fence_->state=candidate.state();fence_->bytes=candidate.checkpoint_bytes();
        fence_->view=accepted_&&accepted_->key.package==candidate.state()->key.package?view_:fit(*candidate.scene().snapshot());
        if(!host_){D3D_FEATURE_LEVEL level{};auto type=hardware_?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP;
            auto hr=D3D11CreateDevice(nullptr,type,nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_);
            if(FAILED(hr))hr=D3D11CreateDevice(nullptr,type,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_);
            if(FAILED(hr))return reject();
            if(hardware_){Microsoft::WRL::ComPtr<IDXGIDevice> dxgi;Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
                if(FAILED(device_.As(&dxgi))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetDesc(&desc))||desc.VendorId==0x1414)return reject();}
            host_=std::make_unique<rendering::D3D11DualModeHost>(device_.Get(),context_.Get(),101,ops_);}
        auto pass=[&](graphics::d3d11::HostDevice& backend,ID3D11RenderTargetView* target,graphics::TargetInfo info,graphics::d3d11::QueryRetirement& query){return render_offscreen_moc3(backend,target,info,candidate.scene().snapshot(),candidate.scene().atlases(),fence_->view,query,&probe_);};
        auto result=host_->request_moc(pass,fence_->command.key.generation,512,512,fail_target,fence_);
        if(result!=GResult::ok){if(host_->pending()){terminal_=true;return result;}return reject();}return result;
    }catch(...){if(host_&&host_->pending()){terminal_=true;return GResult::failed;}return reject();}
}
GResult LeasePublication::poll(){
    if(std::this_thread::get_id()!=owner_)return GResult::wrong_thread;
    if(!pending_||!host_||!host_->pending())return GResult::invalid_argument;
    auto result=host_->poll_commit();if(result==GResult::busy)return result;
    if(host_->pending()){terminal_=true;return result;}
    if(result!=GResult::ok)return reject();pending_=false;fence_.reset();return GResult::ok;
}
void LeasePublication::cancel() noexcept {if(std::this_thread::get_id()!=owner_)std::terminate();if(!pending_)return;if(host_&&host_->pending())fence_->cancelled=true;else reject();}
ars_status LeasePublication::revoke(){return core_.revoke();}
LeasePublication::State LeasePublication::state(){
    State out;if(std::this_thread::get_id()!=owner_){out.core_status=ARS_E_WRONG_THREAD;return out;}std::vector<Parameter> values;out.core_status=core_.read(values,out.generation);
    out.pending=pending_;out.terminal=terminal_;out.host_generation=host_?host_->pose_generation():0;out.creations=probe_.creations;out.device_calls=device_?1:0;out.ingress=ingress_;
    if(accepted_){out.epoch=accepted_->key.epoch;out.package=accepted_->key.package;out.clock=accepted_->key.clock;}return out;
}
const std::vector<std::uint8_t>& LeasePublication::checkpoint() const {need(std::this_thread::get_id()==owner_&&!closing_&&!closed_);static const std::vector<std::uint8_t> empty;return bytes_?*bytes_:empty;}
std::vector<std::uint8_t> LeasePublication::read_pixels(){
    need(std::this_thread::get_id()==owner_&&!closing_&&!closed_&&host_&&accepted_&&host_->pose_generation()==generation_);
    D3D11_TEXTURE2D_DESC desc{};host_->texture()->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;need(SUCCEEDED(device_->CreateTexture2D(&desc,nullptr,&staging)));context_->CopyResource(staging.Get(),host_->texture());
    std::vector<std::uint8_t> rgba(512*512*4);D3D11_MAPPED_SUBRESOURCE m{};need(SUCCEEDED(context_->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)));
    for(unsigned y=0;y<512;++y)std::copy_n(static_cast<std::uint8_t*>(m.pData)+y*m.RowPitch,512*4,rgba.data()+y*512*4);context_->Unmap(staging.Get(),0);return rgba;
}
std::vector<std::uint8_t> LeasePublication::core_bytes(){
    need(std::this_thread::get_id()==owner_&&!closing_&&!closed_);std::vector<Parameter> values;std::uint64_t generation=0;need(core_.read(values,generation)==ARS_OK);
    std::vector<std::uint8_t> bytes;for(unsigned i=0;i<8;++i)bytes.push_back(static_cast<std::uint8_t>(generation>>(8*i)));
    for(const auto& value:values){auto bits=std::bit_cast<std::uint32_t>(value.value);for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}return bytes;
}
GResult LeasePublication::close(){
    if(std::this_thread::get_id()!=owner_)return GResult::wrong_thread;if(closed_)return GResult::ok;
    closing_=true;cancel();if(host_){if(host_->pending()){auto r=poll();if(r==GResult::busy||host_->pending())return r;}host_->begin_close();auto r=host_->poll_close();if(r!=GResult::ok)return r;host_.reset();}
    if(device_){Microsoft::WRL::ComPtr<ID3D11InfoQueue> messages;device_.As(&messages);
        for(UINT64 i=0;messages&&i<messages->GetNumStoredMessages();++i){SIZE_T n=0;messages->GetMessage(i,nullptr,&n);std::vector<unsigned char> data(n);auto* message=reinterpret_cast<D3D11_MESSAGE*>(data.data());
            if(FAILED(messages->GetMessage(i,message,&n))||message->Severity<=D3D11_MESSAGE_SEVERITY_WARNING)return GResult::failed;}}
    fence_.reset();context_.Reset();device_.Reset();if(opened_&&core_.close()!=ARS_OK)return GResult::failed;closed_=true;return GResult::ok;
}
}
#endif
