#include "ars/ars.h"
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <wrl/client.h>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
#include <thread>
#include <string>
using Microsoft::WRL::ComPtr;
namespace test {
void check(bool ok,const char *message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
std::vector<std::uint8_t> file(const std::filesystem::path &path){
    std::ifstream input(path,std::ios::binary|std::ios::ate);check(bool(input),"open public fixture");
    const auto count=input.tellg();check(count>0,"nonempty public fixture");input.seekg(0);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(count));
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(bool(input),"read entire public fixture");return bytes;
}
}
namespace {
void check(ars_status expected,ars_status actual,const char *message){
    if(expected!=actual){std::printf("FAIL %s expected=%d actual=%d\n",message,expected,actual);std::exit(1);}
}
struct Target {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
    ars_render_target descriptor{};
    Target(ID3D11Device *device,ID3D11DeviceContext *context,unsigned width,unsigned height){
        D3D11_TEXTURE2D_DESC d{};d.Width=width;d.Height=height;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET;
        test::check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&texture)),"public target texture");
        test::check(SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&view)),"public target RTV");
        descriptor.struct_size=sizeof(descriptor);descriptor.abi_version=ARS_ABI_VERSION;
        descriptor.d3d11_device=device;descriptor.d3d11_immediate_context=context;descriptor.d3d11_rtv=view.Get();
        descriptor.device_generation=1;descriptor.width_px=width;descriptor.height_px=height;
        descriptor.dxgi_format=d.Format;descriptor.sample_count=1;descriptor.color_space=ARS_COLOR_SRGB;
        descriptor.alpha_mode=ARS_ALPHA_PREMULTIPLIED;
    }
    std::vector<std::uint8_t> pixels(ID3D11Device *device,ID3D11DeviceContext *context){
        D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;
        d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;test::check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"readback allocation");
        context->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE map{};
        test::check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map)),"pixel readback");
        std::vector<std::uint8_t> result(std::size_t(d.Width)*d.Height*4);
        for(unsigned y=0;y<d.Height;++y)std::memcpy(result.data()+std::size_t(y)*d.Width*4,
            static_cast<const std::uint8_t*>(map.pData)+std::size_t(y)*map.RowPitch,d.Width*4);
        context->Unmap(staging.Get(),0);return result;
    }
};
ars_status render(ars_runtime_h runtime,const ars_render_target &target){
    ars_status s=ARS_E_BUSY;for(unsigned i=0;i<5000&&s==ARS_E_BUSY;++i){s=ars_runtime_render(runtime,&target);
        if(s==ARS_E_BUSY)std::this_thread::sleep_for(std::chrono::milliseconds(1));}return s;
}
ars_status drain(ars_runtime_h runtime){
    ars_status s=ARS_E_BUSY;for(unsigned i=0;i<5000&&s==ARS_E_BUSY;++i){s=ars_runtime_drain_gpu(runtime);
        if(s==ARS_E_BUSY)std::this_thread::sleep_for(std::chrono::milliseconds(1));}return s;
}
int dynamic_pixels(const char *path,bool hardware){
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    test::check(SUCCEEDED(D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,
        nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"dynamic debug device");
    auto bytes=test::file(path);ars_runtime_desc rd{};rd.struct_size=sizeof(rd);rd.abi_version=ARS_ABI_VERSION;
    ars_runtime_h runtime{};check(ARS_OK,ars_runtime_create(&rd,&runtime),"dynamic runtime");
    ars_asset_source asset{sizeof(asset),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};ars_model_h model{};
    check(ARS_OK,ars_model_load(runtime,&asset,&model),"dynamic fixture admission");
    ars_character_desc cd{sizeof(cd),ARS_ABI_VERSION,ARS_MODE_2D,0};ars_character_h character{};
    check(ARS_OK,ars_character_create(runtime,model,&cd,&character),"dynamic character");
    check(ARS_OK,ars_runtime_update(runtime,1),"dynamic neutral producer");
    Target target(device.Get(),context.Get(),64,64);
    auto pixel=[](const auto &pixels,unsigned x,unsigned y,std::array<int,4> expected){
        for(unsigned c=0;c<4;++c)test::check(std::abs(int(pixels[(y*64+x)*4+c])-expected[c])<=1,"independent RGBA pixel oracle");
    };
    check(ARS_OK,render(runtime,target.descriptor),"neutral2D render");auto neutral=target.pixels(device.Get(),context.Get());
    pixel(neutral,16,16,{0,255,0,255});pixel(neutral,44,16,{0,0,0,0});
    ars_utf8_view ids[]{{"mouth.open",10},{"head.yaw_rad",12},{"camera.position",15}};
    ars_source_desc source{};source.struct_size=sizeof(source);source.abi_version=ARS_ABI_VERSION;
    source.allowed_channel_ids=ids;source.allowed_channel_count=3;source.max_channels_per_frame=3;
    source.max_frames_per_second=240;source.lease_ttl_ns=UINT64_C(3600000000000);ars_source_admin_h admin{};ars_ingress_cap cap{};
    check(ARS_OK,ars_source_register(runtime,character,&source,&admin,&cap),"dynamic public authority");
    std::uint64_t time=1,sequence=0;
    auto submit=[&](ars_motion_channel *channels,std::uint32_t count){
        ars_motion_frame frame{};frame.struct_size=sizeof(frame);frame.abi_version=ARS_ABI_VERSION;frame.sequence=++sequence;
        frame.timestamp_mode=ARS_TIMESTAMP_HOST_MONOTONIC;frame.channels=channels;frame.channel_count=count;frame.confidence=1;
        check(ARS_OK,ars_motion_submit(runtime,cap,&frame),"dynamic public ingress");
        check(ARS_OK,ars_runtime_update(runtime,++time),"dynamic public CPU commit");
    };
    ars_motion_channel mouth{{"mouth.open",10},ARS_VALUE_SCALAR,0,{.75f,0,0,0}};submit(&mouth,1);
    check(ARS_OK,render(runtime,target.descriptor),"actual committed deform render");auto deformed=target.pixels(device.Get(),context.Get());
    pixel(deformed,12,16,{255,0,0,255});pixel(deformed,44,16,{0,255,0,255});
    ars_layer_change layer{};layer.struct_size=sizeof(layer);layer.abi_version=ARS_ABI_VERSION;layer.layer_id={"face.mouth",10};
    layer.visible=1;layer.opacity=.5f;layer.z_index=5;
    check(ARS_OK,ars_character_set_visual_layer(runtime,character,&layer),"dynamic opacity");
    check(ARS_OK,render(runtime,target.descriptor),"opacity composite");pixel(target.pixels(device.Get(),context.Get()),24,16,{127,128,0,255});
    layer.opacity=1;layer.z_index=-1;check(ARS_OK,ars_character_set_visual_layer(runtime,character,&layer),"dynamic Z");
    check(ARS_OK,render(runtime,target.descriptor),"Z composite");pixel(target.pixels(device.Get(),context.Get()),24,16,{255,0,0,255});
    check(ARS_OK,ars_character_set_mode(runtime,character,ARS_MODE_3D),"actual posed3D mode");
    check(ARS_OK,render(runtime,target.descriptor),"posed3D baseline");const auto baseline3=target.pixels(device.Get(),context.Get());
    ars_motion_channel changed[]{{{"mouth.open",10},ARS_VALUE_SCALAR,0,{.25f,0,0,0}},
        {{"head.yaw_rad",12},ARS_VALUE_SCALAR,0,{.6f,0,0,0}},
        {{"camera.position",15},ARS_VALUE_VEC3,0,{.5f,0,0,0}}};submit(changed,3);
    check(ARS_OK,render(runtime,target.descriptor),"skin morph camera changed render");const auto changed3=target.pixels(device.Get(),context.Get());
    test::check(baseline3!=changed3,"actual3D committed morph/skin/camera cannot reuse old image");
    std::size_t before_alpha=0,after_alpha=0;for(std::size_t i=3;i<changed3.size();i+=4){before_alpha+=baseline3[i]!=0;after_alpha+=changed3[i]!=0;}
    test::check(before_alpha>100&&after_alpha>100,"nonempty discriminating3D controls");
    check(ARS_OK,ars_runtime_begin_close(runtime),"dynamic H close");check(ARS_OK,drain(runtime),"dynamic R drain");
    check(ARS_OK,ars_runtime_finish_close(runtime),"dynamic final CPU/GPU custody");
    std::puts("PASS public dynamic2D pixel displacement/opacity/Z and committed3D skin/morph/camera change");return 0;
}
}
int mip_pixels(const char *folder,bool hardware){
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    test::check(SUCCEEDED(D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,
        nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"mip debug device");
    // Independent scalar lighting oracle. The input texel yields linear red
    //1,.25,.0625 at levels0/1/2; fractional LOD interpolates those values.
    const double nl=1/std::sqrt(1.3125),nh=std::sqrt((1+nl)/2);
    const double fresnel=.04+.96*std::pow(1-nh,5),distribution=1/(3.14159265+.000001);
    const double geometry=(nl/(nl*.5+.5+.000001))*(1/(1+.000001));
    const double spec=distribution*geometry*fresnel/(4*nl+.000001);
    auto expected=[&](double red){const double lit=((1-fresnel)*red/3.14159265+spec)*nl*.75+red*.08+.0008;
        const double mapped=lit/(1+lit),encoded=mapped<.0031308?mapped*12.92:1.055*std::pow(mapped,1/2.4)-.055;
        return int(std::lround(encoded*255));};
    for(unsigned l=0;l<4;++l)for(const auto mode:{9728,9984,9986}){
        const unsigned lod=25+l*50;const auto path=std::filesystem::path(folder)/("committed-lod-"+std::to_string(lod)+"-"+std::to_string(mode)+".arschar");
        auto bytes=test::file(path);ars_runtime_desc rd{};rd.struct_size=sizeof(rd);rd.abi_version=ARS_ABI_VERSION;ars_runtime_h runtime{};
        check(ARS_OK,ars_runtime_create(&rd,&runtime),"mip runtime");ars_model_h model{};ars_character_h character{};
        ars_asset_source asset{sizeof(asset),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};
        check(ARS_OK,ars_model_load(runtime,&asset,&model),"mip fixture admission");
        ars_character_desc cd{sizeof(cd),ARS_ABI_VERSION,ARS_MODE_3D,0};check(ARS_OK,ars_character_create(runtime,model,&cd,&character),"mip character");
        check(ARS_OK,ars_runtime_update(runtime,1),"mip actual CPU commit");Target target(device.Get(),context.Get(),64,64);
        check(ARS_OK,render(runtime,target.descriptor),"mip actual public GPU commit");auto pixels=target.pixels(device.Get(),context.Get());
        constexpr std::array<double,4> nearest{1,.25,.25,.0625},linear{.8125,.4375,.203125,.109375};
        const double red=mode==9728?1:mode==9984?nearest[l]:linear[l];const auto offset=(32*64+32)*4;
        std::printf("PUBLIC_MIP lod=%u filter=%d actual=%u expected=%d\n",lod,mode,unsigned(pixels[offset]),expected(red));
        test::check(std::abs(int(pixels[offset])-expected(red))<=3&&std::abs(int(pixels[offset+1])-expected(0))<=2&&pixels[offset+3]==255,
            "fractional LOD and multiple generated levels match independent pixel oracle");
        check(ARS_OK,ars_runtime_begin_close(runtime),"mip H close");check(ARS_OK,drain(runtime),"mip R drain");
        check(ARS_OK,ars_runtime_finish_close(runtime),"mip CPU/GPU cleanup");
    }
    std::puts("PASS public fractional LOD0.25/0.75/1.25/1.75 and mip0/1/2 independent pixels");return 0;
}
int main(int argc,char **argv){
    if(argc>=3&&argc<=4&&std::string(argv[2])=="--mip")return mip_pixels(argv[1],argc==4);
    if(argc>=3&&argc<=4&&std::string(argv[2])=="--dynamic")return dynamic_pixels(argv[1],argc==4);
    if(argc<2||argc>3)return 2;const bool hardware=argc==3&&std::string(argv[2])=="--hardware";
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    test::check(SUCCEEDED(D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,
        nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"debug D3D11 device");
    ComPtr<ID3D11InfoQueue> messages;test::check(SUCCEEDED(device.As(&messages)),"debug info queue");
    auto bytes=test::file(argv[1]);ars_runtime_desc desc{};desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;
    ars_runtime_h runtime{};check(ARS_OK,ars_runtime_create(&desc,&runtime),"public runtime");
    ars_capabilities caps{};caps.struct_size=sizeof(caps);caps.abi_version=ARS_ABI_VERSION;
    check(ARS_OK,ars_runtime_get_capabilities(runtime,&caps),"profile caps");
    test::check(caps.feature_bits==(ARS_CAP_HOST_SCOPED_MOTION_INGRESS|ARS_CAP_D3D11_HOST_DEVICE|
        ARS_CAP_NATIVE_2D_PROFILE_B|ARS_CAP_GLTF2_GLB_SUBSET|ARS_CAP_DUAL_MODE),"exact opt-in capabilities");
    ars_asset_source source{sizeof(source),ARS_ABI_VERSION,ARS_ASSET_MEMORY_BUNDLE,0,bytes.data(),bytes.size()};
    ars_model_h model{};check(ARS_OK,ars_model_load(runtime,&source,&model),"public model");
    ars_character_desc cd{sizeof(cd),ARS_ABI_VERSION,ARS_MODE_2D,0};ars_character_h character{};
    check(ARS_OK,ars_character_create(runtime,model,&cd,&character),"public character");
    check(ARS_OK,ars_runtime_update(runtime,1),"commit neutral pose");
    {
        Target first(device.Get(),context.Get(),64,64),resized(device.Get(),context.Get(),96,80);
        auto invalid=first.descriptor;invalid.device_generation=0;
        check(ARS_E_INVALID_ARGUMENT,ars_runtime_render(runtime,&invalid),"invalid generation");
        check(ARS_OK,render(runtime,first.descriptor),"C ABI2D candidate/copy exact completion");
        const auto original=first.pixels(device.Get(),context.Get());bool visible=false;
        for(std::size_t i=3;i<original.size();i+=4)visible|=original[i]!=0;
        test::check(visible,"public2D produces nontransparent pixels");
        auto layers=[&](bool shown,float opacity){
            for(const auto *id:{"base.body","face.mouth"}){
                ars_layer_change change{};change.struct_size=sizeof(change);change.abi_version=ARS_ABI_VERSION;
                change.layer_id={id,static_cast<std::uint32_t>(std::strlen(id))};change.visible=shown?1:0;
                change.opacity=opacity;change.z_index=std::strcmp(id,"base.body")==0?0:5;
                check(ARS_OK,ars_character_set_visual_layer(runtime,character,&change),"committed local layer control");
            }
        };
        layers(false,1);check(ARS_OK,render(runtime,first.descriptor),"hidden layers render");
        const auto hidden=first.pixels(device.Get(),context.Get());
        for(auto byte:hidden)test::check(byte==0,"all hidden layers produce transparent target");
        layers(true,0.5f);check(ARS_OK,render(runtime,first.descriptor),"half opacity layers render");
        const auto half=first.pixels(device.Get(),context.Get());bool opaque_control=false;
        for(std::size_t i=3;i<original.size();i+=4)if(original[i]==255){
            opaque_control=true;test::check(half[i]>=191&&half[i]<=193,"two half-alpha layers compose to0.75");
        }
        test::check(opaque_control,"nonempty independent opaque control pixels");
        layers(true,1);check(ARS_OK,render(runtime,first.descriptor),"restore full opacity");
        test::check(first.pixels(device.Get(),context.Get())==original,"local controls restore exact original pixels");
        ars_status wrong=ARS_OK;std::thread other([&]{wrong=ars_runtime_render(runtime,&first.descriptor);});other.join();
        check(ARS_E_WRONG_THREAD,wrong,"public R ownership");
        invalid=first.descriptor;invalid.device_generation=2;
        check(ARS_E_INVALID_ARGUMENT,ars_runtime_render(runtime,&invalid),"old context with different generation refused");
        test::check(first.pixels(device.Get(),context.Get())==original,"invalid target preserves accepted pixels");
        check(ARS_E_BUSY,ars_runtime_render(runtime,&first.descriptor),"prepare candidate");
        check(ARS_OK,ars_character_set_mode(runtime,character,ARS_MODE_3D),"H config changes pending candidate");
        check(ARS_E_STALE_HANDLE,render(runtime,first.descriptor),"final fence refuses old config");
        test::check(first.pixels(device.Get(),context.Get())==original,"stale candidate preserves pixels");
        for(unsigned i=0;i<100;++i){
            check(ARS_OK,ars_character_set_mode(runtime,character,i%2?ARS_MODE_2D:ARS_MODE_3D),"switch public mode");
            check(ARS_OK,ars_runtime_update(runtime,i+2),"commit producer pose");
            auto &target=i%3?first:resized;check(ARS_OK,render(runtime,target.descriptor),"100 switches/resizes exact queries");
        }
        check(ARS_E_BUSY,ars_runtime_render(runtime,&first.descriptor),"pending borrowed target");
        check(ARS_E_BUSY,ars_runtime_render(runtime,&resized.descriptor),"replacement target waits for pending retirement");
        check(ARS_OK,drain(runtime),"live C ABI drain completes accepted presentation");
        // A second character is rendered by the same coherent runtime census.
        ars_character_h second{};cd.initial_mode=ARS_MODE_3D;
        check(ARS_OK,ars_character_create(runtime,model,&cd,&second),"second representation character");
        check(ARS_OK,ars_runtime_update(runtime,102),"multi-character CPU batch");
        check(ARS_OK,render(runtime,first.descriptor),"mixed runtime-wide3D then2D composition");
        check(ARS_OK,ars_character_destroy(runtime,second),"retire second character");
        check(ARS_E_BUSY,ars_runtime_render(runtime,&first.descriptor),"candidate before close");
        check(ARS_OK,ars_runtime_begin_close(runtime),"H begins close with GPU in flight");
        check(ARS_E_BUSY,ars_runtime_finish_close(runtime),"actual GPU/runtime pin blocks finish");
        check(ARS_OK,drain(runtime),"R drain retires without accepting new candidate after close");
        ars_close_state state{};state.struct_size=sizeof(state);state.abi_version=ARS_ABI_VERSION;
        check(ARS_OK,ars_runtime_query_close(runtime,&state),"close counts");
        test::check(state.state==ARS_FENCE_COMPLETE&&state.outstanding_gpu_retires==0&&state.outstanding_render_snapshot_pins==0,
            "GPU and original CPU snapshot custody retired");
    }
    check(ARS_OK,ars_runtime_finish_close(runtime),"finish only after R retirement");
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){
        SIZE_T bytes_needed=0;messages->GetMessage(i,nullptr,&bytes_needed);std::vector<std::uint8_t> storage(bytes_needed);
        auto *m=reinterpret_cast<D3D11_MESSAGE*>(storage.data());messages->GetMessage(i,m,&bytes_needed);
        if(m->Severity==D3D11_MESSAGE_SEVERITY_ERROR||m->Severity==D3D11_MESSAGE_SEVERITY_CORRUPTION){
            std::printf("D3D ERROR %s\n",m->pDescription);return 1;
        }
    }
    std::printf("PASS ABI20 %s actual CPU pose/2D pixels/100 switches-resizes/stale config/mixed characters/close custody\n",hardware?"HARDWARE":"WARP");
    return 0;
}
