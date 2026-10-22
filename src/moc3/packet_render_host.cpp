#include "drawable_packet.hpp"
#include "d3d11_renderer.hpp"
#include "../rendering/d3d11_dual_mode_host.hpp"
#include <d3d11sdklayers.h>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#include <fcntl.h>
#include <io.h>
using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
namespace {
struct QueryFault:ars::graphics::d3d11::QueryOps {
    bool loss=false,armed=false;
    HRESULT removed_reason(ID3D11Device*) override {return loss&&armed?DXGI_ERROR_DEVICE_REMOVED:S_OK;}
    HRESULT get_data(ID3D11DeviceContext*,ID3D11Query*,BOOL*) override {armed=true;return loss?DXGI_ERROR_DEVICE_REMOVED:S_FALSE;}
    void flush(ID3D11DeviceContext* c) override {c->Flush();}
};
ars::moc3::View fit(const ars::moc3::Snapshot& snapshot){
    float x0=std::numeric_limits<float>::max(),y0=x0,x1=-x0,y1=-x0;
    for(auto& d:snapshot.drawables)for(auto& v:d.vertices){x0=std::min(x0,v.x);x1=std::max(x1,v.x);y0=std::min(y0,v.y);y1=std::max(y1,v.y);}
    auto span=std::max(x1-x0,y1-y0);if(!std::isfinite(span)||span<=0)return {};
    auto scale=1.8f/span;return {scale,scale,-(x0+x1)*.5f*scale,-(y0+y1)*.5f*scale};
}
}
int main(int argc,char** argv){
    if(argc!=4&&argc!=5&&argc!=6)return 2;std::uint64_t nonce=0;auto end=argv[2]+std::char_traits<char>::length(argv[2]);auto parsed=std::from_chars(argv[2],end,nonce);
    if(parsed.ec!=std::errc{}||parsed.ptr!=end||!nonce)return 2;
    std::uint32_t frame_index=0;bool sequence=argc==6&&std::string_view(argv[4])=="--sequence";
    if(argc==6){if(!sequence)return 2;auto end_index=argv[5]+std::char_traits<char>::length(argv[5]);auto p=std::from_chars(argv[5],end_index,frame_index);if(p.ec!=std::errc{}||p.ptr!=end_index||frame_index>=8)return 2;}
    QueryFault fault;bool injected=argc==5;
    if(injected){auto mode=std::string_view(argv[4]);if(mode!="--query-timeout"&&mode!="--query-loss")return 2;fault.loss=mode=="--query-loss";}
    bool hardware=std::string_view(argv[3])=="--hardware";if(!hardware&&std::string_view(argv[3])!="--warp")return 2;
    auto output=std::filesystem::path(argv[1]);std::filesystem::create_directories(output);
    std::uint64_t device_calls=0;ars::moc3::GpuProbe probe;
    auto proof=[&](const char* status,std::uint32_t flags=0){std::ofstream f(output/"admission.json");f<<"{\"status\":\""<<status<<"\",\"device_creation_calls\":"<<device_calls<<",\"gpu_resource_creations\":"<<probe.creations<<",\"flags\":"<<flags<<",\"physics\":\""<<((flags&128)?"SIMULATED_BOUNDED_INDEPENDENT":(flags&3)==3?"VALIDATED_NOT_SIMULATED":"ABSENT")<<"\"}";};
    try{
        _setmode(_fileno(stdin),_O_BINARY);std::vector<std::uint8_t> bytes;char chunk[16384];
        while(std::cin){std::cin.read(chunk,sizeof(chunk));auto n=std::cin.gcount();if(std::size_t(n)>ars::moc3::admission::packet_cap-bytes.size()){proof("REJECTED");return 3;}bytes.insert(bytes.end(),chunk,chunk+n);}
        if(!std::cin.eof()||std::cin.bad()){proof("REJECTED");return 3;}
        ars::moc3::admission::VerifiedScene scene;
        ars::moc3::admission::VerifiedSequence batch;
        bool valid=false;
        if(sequence){valid=ars::moc3::admission::decode_sequence(bytes,nonce,batch)&&frame_index<batch.size();if(valid)scene=batch.frame(frame_index);}
        else valid=ars::moc3::admission::decode(bytes,nonce,scene);
        if(!valid){proof("REJECTED");std::cout<<"REJECTED GPU_CREATIONS=0\n";return 3;}
        // This is the first reachable GPU operation, after full independent two-pass decoding.
        bytes.clear();bytes.shrink_to_fit();auto view=fit(sequence?*batch.frame(0).snapshot():*scene.snapshot());
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
        ++device_calls;auto hr=D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
        bool debug=SUCCEEDED(hr);if(FAILED(hr)){++device_calls;hr=D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);}
        if(FAILED(hr)){proof("DEVICE_FAILED",scene.flags());return 4;}
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
        if(FAILED(device.As(&dxgi))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetDesc(&desc))||(hardware&&desc.VendorId==0x1414)){proof("ADAPTER_FAILED",scene.flags());return 4;}
        ComPtr<ID3D11InfoQueue> messages;if(debug)device.As(&messages);
        ars::rendering::D3D11DualModeHost host(device.Get(),context.Get(),101,injected?&fault:nullptr);
        auto pass=[&](ars::graphics::d3d11::HostDevice& backend,ID3D11RenderTargetView* target,ars::graphics::TargetInfo info,ars::graphics::d3d11::QueryRetirement& query){return ars::moc3::render_offscreen_moc3(backend,target,info,scene.snapshot(),scene.atlases(),view,query,&probe);};
        auto result=host.request_moc(pass,1,512,512);if(result!=Result::ok){proof("RENDER_FAILED",scene.flags());if(host.pending())ExitProcess(5);return 5;}
        auto deadline=std::chrono::steady_clock::now()+(injected?std::chrono::seconds(1):std::chrono::seconds(15));result=Result::busy;
        while(result==Result::busy&&std::chrono::steady_clock::now()<deadline){result=host.poll_commit();std::this_thread::yield();}
        if(result!=Result::ok){proof("QUERY_QUARANTINED_PROCESS_EXIT",scene.flags());
            // A timed-out query is NOT completion or physical device loss. Keep all pins
            // alive until this dedicated diagnostic process exits; never unwind them.
            ExitProcess(5);}
        D3D11_TEXTURE2D_DESC td{};host.texture()->GetDesc(&td);td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;
        if(FAILED(device->CreateTexture2D(&td,nullptr,&staging)))return 5;context->CopyResource(staging.Get(),host.texture());D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return 5;
        std::vector<std::uint8_t> rgba(512*512*4);for(unsigned y=0;y<512;++y)std::copy_n(static_cast<std::uint8_t*>(mapped.pData)+y*mapped.RowPitch,512*4,rgba.data()+y*512*4);context->Unmap(staging.Get(),0);
        {std::ofstream f(output/"frame.rgba",std::ios::binary);f.write(reinterpret_cast<char*>(rgba.data()),rgba.size());if(!f)return 6;}
        {std::ofstream f(output/"frame.ppm",std::ios::binary);f<<"P6\n512 512\n255\n";for(unsigned y=0;y<512;++y)for(unsigned x=0;x<512;++x){auto p=(y*512+x)*4;unsigned bg=((x/16+y/16)%2)?180:220;for(int c=0;c<3;++c)f.put(static_cast<char>(std::min(255u,unsigned(rgba[p+c])+bg*(255-rgba[p+3])/255)));}if(!f)return 6;}
        host.begin_close();if(host.poll_close()!=Result::ok)return 5;
        for(UINT64 i=0;messages&&i<messages->GetNumStoredMessages();++i){SIZE_T size=0;messages->GetMessage(i,nullptr,&size);std::vector<unsigned char> buffer(size);auto* m=reinterpret_cast<D3D11_MESSAGE*>(buffer.data());if(FAILED(messages->GetMessage(i,m,&size))||m->Severity<=D3D11_MESSAGE_SEVERITY_WARNING){proof("DEBUG_FAILED",scene.flags());return 7;}}
        proof("RENDERED",scene.flags());std::cout<<"ADMISSION_RENDER_PASS\n";return 0;
    }catch(...){proof("REJECTED");return 8;}
}
