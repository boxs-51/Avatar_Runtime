#include "rendering/d3d11_scene3d.hpp"
#include "../assets/asset_test_support.hpp"
#include <d3d11sdklayers.h>
#include <dxgi.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <thread>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
namespace {
void check(bool ok,const char *message) { test::check(ok,message); }
bool near_pixel(std::array<unsigned char,4> value,std::array<int,4> expected,int tolerance=3) {
    for (int i=0;i<4;++i)
        if (std::abs(int(value[i])-expected[i])>tolerance) return false;
    return true;
}
struct Target {
    ComPtr<ID3D11Texture2D> image,staging;
    ComPtr<ID3D11RenderTargetView> view;
};
Target target(ID3D11Device *device,UINT size) {
    Target t;
    D3D11_TEXTURE2D_DESC d{};
    d.Width=d.Height=size; d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.Usage=D3D11_USAGE_DEFAULT;
    d.BindFlags=D3D11_BIND_RENDER_TARGET;
    check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&t.image)),"color target");
    check(SUCCEEDED(device->CreateRenderTargetView(t.image.Get(),nullptr,&t.view)),"target view");
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&t.staging)),"staging color");
    return t;
}
std::array<unsigned char,4> pixel(ID3D11DeviceContext *context,const Target &t,UINT x,UINT y) {
    context->CopyResource(t.staging.Get(),t.image.Get());
    D3D11_MAPPED_SUBRESOURCE m{};
    check(SUCCEEDED(context->Map(t.staging.Get(),0,D3D11_MAP_READ,0,&m)),"color map");
    const auto *p=static_cast<const unsigned char *>(m.pData)+y*m.RowPitch+x*4;
    std::array<unsigned char,4> result{p[0],p[1],p[2],p[3]};
    context->Unmap(t.staging.Get(),0);return result;
}
float depth_pixel(ID3D11Device *device,ID3D11DeviceContext *context,
                  ID3D11Texture2D *depth,UINT x,UINT y) {
    D3D11_TEXTURE2D_DESC d{};depth->GetDesc(&d);
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"staging depth");
    context->CopyResource(staging.Get(),depth);
    D3D11_MAPPED_SUBRESOURCE m{};
    check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)),"depth map");
    const float result=*reinterpret_cast<const float *>(static_cast<const unsigned char *>(m.pData)+
                                                          y*m.RowPitch+x*sizeof(float));
    context->Unmap(staging.Get(),0);return result;
}
} // namespace

int main(int argc,char **argv) {
    check(argc==2 || (argc==3 && std::string_view(argv[2])=="--hardware"),"corpus and mode");
    const bool hardware=argc==3;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature{};
    const auto driver=hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP;
    auto hr=D3D11CreateDevice(nullptr,driver,nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,
                              D3D11_SDK_VERSION,&device,&feature,&context);
    const bool debug_enabled=SUCCEEDED(hr);
    if (FAILED(hr)) {
        hr=D3D11CreateDevice(nullptr,driver,nullptr,0,nullptr,0,
                             D3D11_SDK_VERSION,&device,&feature,&context);
        std::puts("M2C_DEBUG_LAYER=HOLD");
    }
    check(SUCCEEDED(hr),"D3D11 device");
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapter_info{};
    check(SUCCEEDED(device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(&adapter)) &&
          SUCCEEDED(adapter->GetDesc(&adapter_info)),"adapter identity");
    if (hardware) check(adapter_info.VendorId!=0x1414,"physical adapter required");
    char adapter_name[256]{};
    WideCharToMultiByte(CP_UTF8,0,adapter_info.Description,-1,adapter_name,
                        sizeof(adapter_name),nullptr,nullptr);
    std::printf("M2C_ADAPTER=%s vendor=%04x device=%04x feature=%04x name=%s debug=%u\n",
                hardware?"HARDWARE":"WARP",adapter_info.VendorId,adapter_info.DeviceId,
                unsigned(feature),adapter_name,unsigned(debug_enabled));
    ComPtr<ID3D11InfoQueue> debug_queue;
    if (debug_enabled) check(SUCCEEDED(device.As(&debug_queue)),"D3D11 debug queue");
    ars::graphics::d3d11::HostDevice backend(device.Get(),context.Get(),37);
    auto t=target(device.Get(),8);
    test::Host host;
    {
        ars::assets::Resource resource(test::Host::allocate,test::Host::release,&host);
        std::shared_ptr<const ars::assets::GlbModel> model;
        auto bytes=test::file(std::filesystem::path(argv[1])/"positive-m2-3d-flat.glb");
        check(ars::assets::ingest_glb(bytes,resource,model)==ARS_OK,"C2 GLB ingestion");
        ars::rendering::PreparedScene3D scene;
        check(ars::rendering::prepare_static_3d(model,scene)==ARS_OK,"C0 pose projection");
        ars::rendering::Camera3D camera{{1,0,0,0,0,-1,0,0,0,0,-1,0,-0.5,1,0.5,1}};
        ars::graphics::d3d11::QueryRetirement retirement;
        ars::graphics::TargetInfo info{37,8,8};
        ComPtr<ID3D11Texture2D> depth;
        D3D11_VIEWPORT sentinel{1,1,2,2,0,1};context->RSSetViewports(1,&sentinel);
        D3D11_TEXTURE2D_DESC sentinel_desc{};
        sentinel_desc.Width=sentinel_desc.Height=sentinel_desc.MipLevels=
            sentinel_desc.ArraySize=sentinel_desc.SampleDesc.Count=1;
        sentinel_desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        sentinel_desc.Usage=D3D11_USAGE_IMMUTABLE;
        sentinel_desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const std::array<unsigned char,4> sentinel_data{1,2,3,4};
        D3D11_SUBRESOURCE_DATA sentinel_initial{sentinel_data.data(),4,0};
        ComPtr<ID3D11Texture2D> sentinel_texture;
        ComPtr<ID3D11ShaderResourceView> sentinel_view;
        check(SUCCEEDED(device->CreateTexture2D(&sentinel_desc,&sentinel_initial,&sentinel_texture)) &&
              SUCCEEDED(device->CreateShaderResourceView(sentinel_texture.Get(),nullptr,&sentinel_view)),
              "host sentinel texture");
        ID3D11ShaderResourceView *sentinel_raw=sentinel_view.Get();
        context->PSSetShaderResources(1,1,&sentinel_raw);
        check(ars::rendering::render_offscreen_3d(backend,t.view.Get(),info,scene,camera,
                                                   retirement,&depth)==Result::ok,"C1 draw");
        Result polled=Result::busy;
        for (int i=0;i<10000 && polled==Result::busy;++i) {
            polled=retirement.poll(info.generation);
            if (polled==Result::busy) std::this_thread::yield();
        }
        check(polled==Result::ok && retirement.complete() && !retirement.holds_pin(),"exact query");
        UINT count=1;D3D11_VIEWPORT restored{};context->RSGetViewports(&count,&restored);
        check(count==1 && restored.TopLeftX==1 && restored.Width==2,"host viewport restored");
        ComPtr<ID3D11ShaderResourceView> restored_view;
        context->PSGetShaderResources(1,1,restored_view.GetAddressOf());
        check(restored_view.Get()==sentinel_view.Get(),"host PS texture slot one restored");
        const auto inside=pixel(context.Get(),t,4,4),outside=pixel(context.Get(),t,7,7);
        std::printf("M2C_INSIDE=%u,%u,%u,%u OUTSIDE=%u,%u,%u,%u\n",
            unsigned(inside[0]),unsigned(inside[1]),unsigned(inside[2]),unsigned(inside[3]),
            unsigned(outside[0]),unsigned(outside[1]),unsigned(outside[2]),unsigned(outside[3]));
        check(near_pixel(inside,{0,75,0,255}),
              "fixed PBR green GPU pixel");
        check(outside==std::array<unsigned char,4>{0,0,0,0},"transparent outside pixel");
        check(std::abs(depth_pixel(device.Get(),context.Get(),depth.Get(),4,4)-0.5f)<0.001f,
              "actual depth readback");
        check(std::abs(depth_pixel(device.Get(),context.Get(),depth.Get(),7,7)-1.0f)<0.001f,
              "cleared background depth");
        auto limited=scene;
        limited.gpu_budget=1;
        ars::graphics::d3d11::QueryRetirement over_budget;
        check(ars::rendering::render_offscreen_3d(backend,t.view.Get(),info,limited,camera,
                                                   over_budget)==Result::out_of_budget &&
              pixel(context.Get(),t,4,4)==inside,"GPU budget preflight keeps old target");
        auto resized=target(device.Get(),16);
        ars::graphics::d3d11::QueryRetirement resize_query;
        const ars::graphics::TargetInfo resized_info{37,16,16};
        check(ars::rendering::render_offscreen_3d(backend,resized.view.Get(),resized_info,scene,camera,
                                                   resize_query)==Result::ok,"resized target submit");
        Result resize_result=Result::busy;
        for (int i=0;i<10000 && resize_result==Result::busy;++i) {
            resize_result=resize_query.poll(resized_info.generation);
            if (resize_result==Result::busy) std::this_thread::yield();
        }
        check(resize_result==Result::ok && near_pixel(pixel(context.Get(),resized,9,8),{0,75,0,255}),
              "resized target pixel");
        ars::graphics::d3d11::QueryRetirement bad;
        check(ars::rendering::render_offscreen_3d(backend,t.view.Get(),{38,8,8},scene,camera,bad)==
              Result::invalid_argument,"wrong generation rejected");
        ComPtr<ID3D11Device> foreign_device;
        ComPtr<ID3D11DeviceContext> foreign_context;
        D3D_FEATURE_LEVEL foreign_feature{};
        check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
                                          D3D11_SDK_VERSION,&foreign_device,&foreign_feature,
                                          &foreign_context)),"foreign WARP device");
        auto foreign_target=target(foreign_device.Get(),8);
        ars::graphics::d3d11::QueryRetirement foreign_query;
        check(ars::rendering::render_offscreen_3d(backend,foreign_target.view.Get(),info,
                                                   scene,camera,foreign_query)==Result::invalid_argument,
              "foreign device target rejected");
        Result wrong_thread=Result::ok;
        std::thread worker([&]{ars::graphics::d3d11::QueryRetirement q;
            wrong_thread=ars::rendering::render_offscreen_3d(backend,t.view.Get(),info,scene,camera,q);});
        worker.join();check(wrong_thread==Result::wrong_thread,"wrong R thread rejected");
        auto load=[&](const char *filename) {
            auto source_bytes=test::file(std::filesystem::path(argv[1])/filename);
            std::shared_ptr<const ars::assets::GlbModel> source;
            check(ars::assets::ingest_glb(source_bytes,resource,source)==ARS_OK,"material fixture C2");
            ars::rendering::PreparedScene3D prepared;
            check(ars::rendering::prepare_static_3d(source,prepared)==ARS_OK,"material fixture C0");
            return prepared;
        };
        auto submit=[&](const ars::rendering::PreparedScene3D &prepared,
                        ComPtr<ID3D11Texture2D> *capture=nullptr) {
            ars::graphics::d3d11::QueryRetirement q;
            check(ars::rendering::render_offscreen_3d(backend,t.view.Get(),info,prepared,camera,q,capture)==Result::ok,
                  "material GPU submit");
            Result result=Result::busy;
            for (int i=0;i<10000 && result==Result::busy;++i) {
                result=q.poll(info.generation);
                if (result==Result::busy) std::this_thread::yield();
            }
            check(result==Result::ok && q.complete(),"material exact query");
        };
        auto textured=load("positive-m2-3d-texture.glb");
        check(textured.draws[0].material.base_texture.wrap_t==33071 &&
              textured.draws[0].material.metallic_roughness_texture.image==0,
              "typed sampler and MR projection");
        submit(textured);
        const auto red=pixel(context.Get(),t,4,4);
        std::printf("M2C_TEXTURE=%u,%u,%u,%u\n",unsigned(red[0]),unsigned(red[1]),
                    unsigned(red[2]),unsigned(red[3]));
        check(near_pixel(red,{129,3,3,255}),
              "base sRGB and linear MR pixel");
        auto repeat=load("positive-m2-3d-repeat.glb");
        auto clamp=load("positive-m2-3d-clamp.glb");
        submit(repeat);
        const auto wrapped=pixel(context.Get(),t,4,4);
        submit(clamp);
        const auto clamped=pixel(context.Get(),t,4,4);
        std::printf("M2C_SAMPLER_REPEAT=%u,%u,%u CLAMP=%u,%u,%u\n",
            unsigned(wrapped[0]),unsigned(wrapped[1]),unsigned(wrapped[2]),
            unsigned(clamped[0]),unsigned(clamped[1]),unsigned(clamped[2]));
        check(near_pixel(wrapped,{129,10,10,255}) &&
              near_pixel(clamped,{10,129,10,255}),"declared wrap mode pixels");
        auto point_filter=load("positive-m2-3d-filter-point.glb");
        auto linear_filter=load("positive-m2-3d-filter-linear.glb");
        submit(point_filter);
        const auto point_pixel=pixel(context.Get(),t,4,4);
        submit(linear_filter);
        const auto linear_pixel=pixel(context.Get(),t,4,4);
        std::printf("M2C_FILTER_POINT=%u,%u,%u LINEAR=%u,%u,%u\n",
            unsigned(point_pixel[0]),unsigned(point_pixel[1]),unsigned(point_pixel[2]),
            unsigned(linear_pixel[0]),unsigned(linear_pixel[1]),unsigned(linear_pixel[2]));
        check(near_pixel(point_pixel,{10,129,10,255}) &&
              near_pixel(linear_pixel,{99,99,10,255}),
              "declared point and linear filtering pixels");
        auto masked=load("positive-m2-3d-mask.glb");
        submit(masked);
        check(pixel(context.Get(),t,4,4)[0]>80,"MASK cutoff inclusive boundary");
        masked.draws[0].material.alpha_cutoff=0.5001f;
        submit(masked);
        check(pixel(context.Get(),t,4,4)==std::array<unsigned char,4>{0,0,0,0},
              "MASK cutoff exclusion");
        auto blended=load("positive-m2-3d-blend.glb");
        ars::rendering::PreparedScene3D combined;
        combined.source=blended.source;
        combined.gpu_budget=256*ars::assets::MiB;
        combined.draws=scene.draws;
        combined.draws.push_back(blended.draws[0]);
        for (auto &v:combined.draws.back().vertices) v.position[2]+=0.1f;
        ComPtr<ID3D11Texture2D> blend_depth;
        submit(combined,&blend_depth);
        const auto overlap=pixel(context.Get(),t,4,4);
        std::printf("M2C_BLEND=%u,%u,%u,%u\n",unsigned(overlap[0]),unsigned(overlap[1]),
                    unsigned(overlap[2]),unsigned(overlap[3]));
        check(near_pixel(overlap,{64,38,1,255}),"premultiplied sorted overlap");
        check(std::abs(depth_pixel(device.Get(),context.Get(),blend_depth.Get(),4,4)-0.5f)<0.001f,
              "transparent pass reads depth without writing it");
        auto orthographic=load("positive-m2-3d-camera-orthographic.glb");
        check(ars::rendering::select_gltf_camera(orthographic,3,8,8,camera)==ARS_OK,
              "validated glTF orthographic camera");
        submit(orthographic);
        const auto camera_pixel=pixel(context.Get(),t,4,3);
        std::printf("M2C_CAMERA=%u,%u,%u,%u\n",unsigned(camera_pixel[0]),
                    unsigned(camera_pixel[1]),unsigned(camera_pixel[2]),unsigned(camera_pixel[3]));
        check(near_pixel(camera_pixel,{0,75,0,255}),"glTF camera drives actual pixels");
        camera.clip_from_world[12]+=3;
        submit(orthographic);
        check(pixel(context.Get(),t,4,3)==std::array<unsigned char,4>{0,0,0,0},
              "camera shift moves geometry outside target");
        auto unsupported_bytes=test::file(std::filesystem::path(argv[1])/"positive-m2-3d-unsupported.glb");
        std::shared_ptr<const ars::assets::GlbModel> unsupported;
        check(ars::assets::ingest_glb(unsupported_bytes,resource,unsupported)==ARS_OK,
              "valid unsupported material C2");
        auto old=textured.source;
        check(ars::rendering::prepare_static_3d(unsupported,textured)==ARS_E_UNSUPPORTED &&
              textured.source==old,"unsupported material preflight rollback");
        old.reset();unsupported.reset();textured={};masked={};blended={};combined={};
        repeat={};clamp={};orthographic={};point_filter={};linear_filter={};
        scene={};model.reset();depth.Reset();
    }
    check(host.live==0,"GLB ownership released");
    if (debug_queue) {
        for (UINT64 i=0;i<debug_queue->GetNumStoredMessages();++i) {
            SIZE_T length=0;
            debug_queue->GetMessage(i,nullptr,&length);
            std::vector<unsigned char> buffer(length);
            check(SUCCEEDED(debug_queue->GetMessage(i,
                reinterpret_cast<D3D11_MESSAGE *>(buffer.data()),&length)),"debug message");
            const auto *message=reinterpret_cast<const D3D11_MESSAGE *>(buffer.data());
            check(message->Severity!=D3D11_MESSAGE_SEVERITY_ERROR &&
                  message->Severity!=D3D11_MESSAGE_SEVERITY_CORRUPTION,
                  "D3D debug error queue");
        }
    }
    std::printf("PASS M2-C C0-C2 adapter=%s feature=%04x color/depth/restore\n",
                hardware?"HARDWARE":"WARP",unsigned(feature));
}
