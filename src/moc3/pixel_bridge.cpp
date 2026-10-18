#include "d3d11_renderer.hpp"
#include "../rendering/d3d11_dual_mode_host.hpp"
#include <d3d11sdklayers.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <thread>
#include <fcntl.h>
#include <io.h>

using Microsoft::WRL::ComPtr;
namespace {
using ars::graphics::Result;
using ars::moc3::AtlasSet;
using ars::moc3::Session;
template<class T> bool read(T& value){return bool(std::cin.read(reinterpret_cast<char*>(&value),sizeof(value)));}
bool bytes(std::vector<std::uint8_t>& value,std::size_t size){value.resize(size);return bool(std::cin.read(reinterpret_cast<char*>(value.data()),size));}
bool save(const std::filesystem::path& file,const void* data,std::size_t size){
    std::ofstream out(file,std::ios::binary);return bool(out.write(static_cast<const char*>(data),size));
}
ars::moc3::View fit(const ars::moc3::Snapshot& snapshot){
    float x0=std::numeric_limits<float>::max(),y0=x0,x1=-x0,y1=-x0;
    for(const auto& d:snapshot.drawables)for(const auto& v:d.vertices){x0=std::min(x0,v.x);x1=std::max(x1,v.x);y0=std::min(y0,v.y);y1=std::max(y1,v.y);}
    auto span=std::max(x1-x0,y1-y0);if(!std::isfinite(span)||span<=0)return {};
    auto scale=1.8f/span;return {scale,scale,-(x0+x1)*.5f*scale,-(y0+y1)*.5f*scale};
}
}
int main(int argc,char** argv){
    if(argc!=2&&argc!=3)return 2;
    const bool hardware=argc==3&&std::string(argv[2])=="--hardware";
    const auto output=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(output);
    _setmode(_fileno(stdin),_O_BINARY);
    try{
        char magic[8]{};std::uint32_t moc_size=0,count=0;
        if(!std::cin.read(magic,8)||std::string_view(magic,8)!="ARSF2PK1"||!read(moc_size)||!read(count)||
            moc_size<4||moc_size>16*1024*1024||count==0||count>16)return 2;
        std::vector<std::uint8_t> moc;if(!bytes(moc,moc_size))return 2;
        auto atlases=std::make_shared<AtlasSet>();std::size_t total=0;
        for(std::uint32_t i=0;i<count;++i){
            ars::moc3::Atlas image;if(!read(image.width)||!read(image.height)||!image.width||!image.height||image.width>8192||image.height>8192)return 2;
            auto size=std::uint64_t(image.width)*image.height*4;if(size>512*1024*1024-total)return 2;
            total+=static_cast<std::size_t>(size);if(!bytes(image.rgba,static_cast<std::size_t>(size)))return 2;atlases->images.push_back(std::move(image));
        }
        std::unique_ptr<Session> session;
        if(Session::open_blob(moc,atlases->images.size(),ars::moc3::Budget{},session)!=ars::moc3::Result::ok)return 3;
        moc.clear();moc.shrink_to_fit();
        const auto view=fit(*session->current());
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
        const auto driver=hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP;
        auto hr=D3D11CreateDevice(nullptr,driver,nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
        bool debug=SUCCEEDED(hr);
        if(FAILED(hr))hr=D3D11CreateDevice(nullptr,driver,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
        if(FAILED(hr))return hardware?77:4;
        ComPtr<ID3D11InfoQueue> messages;if(debug)device.As(&messages);
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC adapter_desc{};
        if(FAILED(device.As(&dxgi))||FAILED(dxgi->GetAdapter(&adapter))||FAILED(adapter->GetDesc(&adapter_desc)))return 4;
        if(hardware&&adapter_desc.VendorId==0x1414)return 77;
        std::ofstream adapter_file(output/"adapter.json");
        adapter_file<<"{\"hardware\":"<<(hardware?"true":"false")<<",\"vendor\":"<<adapter_desc.VendorId
                    <<",\"device\":"<<adapter_desc.DeviceId<<",\"feature_level\":"<<unsigned(level)<<",\"debug\":"<<(debug?"true":"false")<<"}";
        adapter_file.close();
        ars::rendering::D3D11DualModeHost host(device.Get(),context.Get(),101);
        ars::moc3::AtlasResidency residency;
        std::cout<<"READY"<<std::endl;
        std::string line;std::uint32_t frames=0;
        while(std::getline(std::cin,line)){
            if(line=="QUIT")break;
            std::istringstream input(line);std::string command,extra;std::size_t n=0;
            if(line.size()>64||!(input>>command>>n)||command!="FRAME"||n>4096||input>>extra||frames>=60)return 2;
            std::vector<ars::moc3::Parameter> parameters;
            for(std::size_t i=0;i<n;++i){
                if(!std::getline(std::cin,line)||line.size()>512)return 2;
                std::istringstream parameter(line);ars::moc3::Parameter p;
                if(!(parameter>>p.id>>p.value)||parameter>>extra)return 2;parameters.push_back(std::move(p));
            }
            if(session->update_complete(parameters)!=ars::moc3::Result::ok){std::cout<<"ERR parameter"<<std::endl;continue;}
            auto snapshot=session->current();ars::moc3::GpuProbe probe;
            auto pass=[&](ars::graphics::d3d11::HostDevice& backend,ID3D11RenderTargetView* target,
                          ars::graphics::TargetInfo info,ars::graphics::d3d11::QueryRetirement& query){
                return ars::moc3::render_offscreen_moc3(backend,target,info,snapshot,atlases,view,query,&probe,&residency);};
            auto result=host.request_moc(pass,snapshot->generation,512,512);
            if(result!=Result::ok){std::cout<<"ERR render"<<std::endl;std::exit(5);}
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);result=Result::busy;
            while(result==Result::busy&&std::chrono::steady_clock::now()<deadline){result=host.poll_commit();std::this_thread::yield();}
            if(result!=Result::ok){std::cout<<"ERR query"<<std::endl;std::exit(5);}
            D3D11_TEXTURE2D_DESC desc{};host.texture()->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;
            desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;
            if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging)))return 5;
            context->CopyResource(staging.Get(),host.texture());D3D11_MAPPED_SUBRESOURCE mapped{};
            if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return 5;
            std::vector<unsigned char> rgba(std::size_t(desc.Width)*desc.Height*4);
            for(UINT y=0;y<desc.Height;++y)std::copy_n(static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch,
                std::size_t(desc.Width)*4,rgba.data()+std::size_t(y)*desc.Width*4);
            context->Unmap(staging.Get(),0);
            ++frames;auto stem="frame-"+std::to_string(frames);
            if(!save(output/(stem+".rgba"),rgba.data(),rgba.size()))return 6;
            // Presenter checker is composed after the clean RGBA readback.
            std::ostringstream header;header<<"P6\n"<<desc.Width<<" "<<desc.Height<<"\n255\n";
            auto text=header.str();std::vector<unsigned char> ppm(text.begin(),text.end());ppm.reserve(ppm.size()+rgba.size()*3/4);
            for(UINT y=0;y<desc.Height;++y)for(UINT x=0;x<desc.Width;++x){
                auto p=(std::size_t(y)*desc.Width+x)*4;unsigned bg=((x/16+y/16)%2)?180:220;
                for(int channel=0;channel<3;++channel)ppm.push_back(static_cast<unsigned char>(std::min(255u,unsigned(rgba[p+channel])+bg*(255-rgba[p+3])/255)));
            }
            if(!save(output/(stem+".ppm"),ppm.data(),ppm.size()))return 6;
            std::ofstream meta(output/"latest.json");
            meta<<"{\"width\":"<<desc.Width<<",\"height\":"<<desc.Height<<",\"frame_generation\":"<<frames
                <<",\"snapshot_generation\":"<<snapshot->generation<<",\"target_epoch\":"<<host.target_epoch()
                <<",\"device_generation\":101,\"backend\":\""<<(hardware?"D3D11_HARDWARE":"D3D11_WARP")
                <<"\",\"rgba\":\""<<stem<<".rgba\",\"image\":\""<<stem<<".ppm\",\"resident_bytes\":"<<probe.resident_bytes
                <<",\"staging_bytes\":"<<probe.staging_bytes<<"}";
            meta.close();if(!meta)return 6;
            std::cout<<"FRAME "<<frames<<" "<<snapshot->generation<<" "<<host.target_epoch()<<std::endl;
        }
        host.begin_close();if(host.poll_close()!=Result::ok)return 5;
        for(UINT64 i=0;messages&&i<messages->GetNumStoredMessages();++i){
            SIZE_T size=0;messages->GetMessage(i,nullptr,&size);std::vector<unsigned char> data(size);
            auto* message=reinterpret_cast<D3D11_MESSAGE*>(data.data());
            if(FAILED(messages->GetMessage(i,message,&size))||message->Severity<=D3D11_MESSAGE_SEVERITY_WARNING)return 7;
        }
        std::cout<<"BYE"<<std::endl;return 0;
    }catch(const std::bad_alloc&){return 8;}
}
