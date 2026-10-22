#include "rendering/d3d11_scene2d.hpp"
#include "../assets/asset_test_support.hpp"
#include "../../src/assets/validated_cpu_model.hpp"
#include <d3d11sdklayers.h>
#include <dxgi.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory_resource>
#include <string_view>
#include <thread>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
using ars::graphics::TargetInfo;
using ars::graphics::d3d11::HostDevice;
using ars::graphics::d3d11::QueryRetirement;
using ars::rendering::PreparedScene2D;
using ars::rendering::prepare_static_2d;
using ars::rendering::render_offscreen_2d;
namespace {
void check(bool ok,const char *message) {
    if (!ok) { std::fprintf(stderr,"FAIL %s\n",message); std::exit(1); }
}
struct Target {
    ComPtr<ID3D11Texture2D> texture,staging;
    ComPtr<ID3D11RenderTargetView> rtv;
    UINT width=0,height=0;
};
Target make_target(ID3D11Device *device,UINT width,UINT height) {
    Target result; result.width=width; result.height=height;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width; desc.Height=height; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&result.texture)),"target texture");
    check(SUCCEEDED(device->CreateRenderTargetView(result.texture.Get(),nullptr,&result.rtv)),"target RTV");
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&result.staging)),"staging texture");
    return result;
}
std::array<unsigned char,4> pixel(ID3D11DeviceContext *context,const Target &target,UINT x,UINT y) {
    context->CopyResource(target.staging.Get(),target.texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(SUCCEEDED(context->Map(target.staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"readback map");
    const auto *bytes=static_cast<const unsigned char *>(mapped.pData)+y*mapped.RowPitch+x*4;
    std::array<unsigned char,4> result{bytes[0],bytes[1],bytes[2],bytes[3]};
    context->Unmap(target.staging.Get(),0);
    return result;
}
bool near_pixel(std::array<unsigned char,4> actual,std::array<int,4> expected,int tolerance=2) {
    for (int i=0;i<4;++i)
        if (std::abs(int(actual[i])-expected[i])>tolerance) return false;
    return true;
}
void render_and_retire(HostDevice &backend,ID3D11DeviceContext *context,const Target &target,
                       const PreparedScene2D &scene,const ars::assets::Image &atlas) {
    QueryRetirement retirement;
    const TargetInfo info{backend.generation(),target.width,target.height};
    check(render_offscreen_2d(backend,target.rtv.Get(),info,scene,atlas,retirement)==Result::ok,
          "private 2D render");
    Result result=Result::busy;
    for (int i=0;i<10000 && result==Result::busy;++i) {
        result=retirement.poll(info.generation);
        if (result==Result::busy) std::this_thread::yield();
    }
    check(result==Result::ok && retirement.complete() && !retirement.holds_pin(),"exact query retirement");
    (void)context;
}
} // namespace

int main(int argc,char **argv) {
    const bool sample=argc==4 && std::string_view(argv[1])=="--sample";
    const bool hardware=argc==2 && std::string_view(argv[1])=="--hardware";
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    const auto driver=hardware ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP;
    HRESULT hr=D3D11CreateDevice(nullptr,driver,nullptr,D3D11_CREATE_DEVICE_DEBUG,
        nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
    bool debug_enabled=SUCCEEDED(hr);
    if (FAILED(hr) && !hardware) {
        hr=D3D11CreateDevice(nullptr,driver,nullptr,0,
            nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
        std::puts("M2B_DEBUG_LAYER=HOLD (runner unavailable)");
    }
    check(SUCCEEDED(hr),"D3D11 device");
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapter_info{};
    check(SUCCEEDED(device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(&adapter)) &&
          SUCCEEDED(adapter->GetDesc(&adapter_info)),"adapter identity");
    if (hardware) check(adapter_info.VendorId!=0x1414,"named physical adapter required");
    char adapter_name[256]{};
    WideCharToMultiByte(CP_UTF8,0,adapter_info.Description,-1,adapter_name,
                        sizeof(adapter_name),nullptr,nullptr);
    std::printf("M2B_ADAPTER=%s vendor=%04x device=%04x feature=%04x name=%s\n",
        hardware?"HARDWARE":"WARP",adapter_info.VendorId,adapter_info.DeviceId,
        unsigned(level),adapter_name);
    ComPtr<ID3D11InfoQueue> debug_queue;
    if (debug_enabled) check(SUCCEEDED(device.As(&debug_queue)),"D3D11 debug info queue");
    HostDevice backend(device.Get(),context.Get(),19);
    if (sample) {
        ars::validation::SchemaClient schema({std::filesystem::absolute(argv[2]).string()});
        test::Host host;
        {
            ars::assets::Resource resource(test::Host::allocate,test::Host::release,&host);
            auto bytes=test::file(argv[3]);
            std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
            check(ars::assets::validate_bundle(bytes,schema,resource,model)==ARS_OK && bool(model),
                  "sample native ingestion");
            PreparedScene2D scene;
            check(prepare_static_2d(ars::rendering::scene_input(*model),scene)==ARS_OK &&
                  scene.draws.size()==1,"sample scene preparation");
            auto sample_target=make_target(device.Get(),model->image().width,model->image().height);
            render_and_retire(backend,context.Get(),sample_target,scene,model->image());
            for (const auto point : {std::array<UINT,2>{82,124},{128,35},{128,230}}) {
                const auto x=point[0],y=point[1];
                check(x<sample_target.width && y<sample_target.height,"sample pixel coordinate");
                const auto actual=pixel(context.Get(),sample_target,x,y);
                const auto offset=(std::size_t(y)*model->image().width+x)*4;
                const auto &rgba=model->image().rgba;
                check(near_pixel(actual,{rgba[offset],rgba[offset+1],rgba[offset+2],rgba[offset+3]},10),
                      "sample portrait pixel readback");
                std::printf("CUBISM_PREVIEW_PIXEL x=%u y=%u rgba=%u,%u,%u,%u\n",
                            x,y,actual[0],actual[1],actual[2],actual[3]);
            }
        }
        check(host.live==0,"sample host allocations released");
        return 0;
    }
    auto target=make_target(device.Get(),4,4);
    auto *memory=std::pmr::get_default_resource();
    ars::assets::Image atlas(memory);
    atlas.width=3; atlas.height=1; atlas.rgba={255,0,0,255,0,0,255,128,0,255,0,255};
    std::vector<ars::assets::Layer> layers;
    layers.emplace_back(memory); layers.emplace_back(memory);
    layers[0].z=0; layers[1].z=1; layers[1].opacity=0.5f;
    std::vector<ars::assets::Part> parts;
    for (int i=0;i<2;++i) {
        parts.emplace_back(memory);
        auto &part=parts.back();
        part.layer=UINT(i); part.rect={UINT(i),0,1,1};
        part.vertices={{0,0},{4,0},{0,4},{4,4}};
        part.uv={{0,0},{1,0},{0,1},{1,1}};
        part.triangles={{0,1,2},{2,1,3}};
    }
    const ars::rendering::Scene2DInput input{layers,parts,{4,4},&atlas};
    PreparedScene2D scene;
    check(prepare_static_2d(input,scene)==ARS_OK,"prepare two layers");
    // Host state changed by this pass must be restored, including texture slots.
    D3D11_VIEWPORT old_viewport{1,1,2,2,0,1};
    context->RSSetViewports(1,&old_viewport);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    D3D11_BUFFER_DESC old_buffer{};
    old_buffer.ByteWidth=16; old_buffer.Usage=D3D11_USAGE_DEFAULT;
    old_buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    ComPtr<ID3D11Buffer> old_vertex,old_index,old_constant;
    check(SUCCEEDED(device->CreateBuffer(&old_buffer,nullptr,&old_vertex)),"host vertex buffer");
    old_buffer.BindFlags=D3D11_BIND_INDEX_BUFFER;
    check(SUCCEEDED(device->CreateBuffer(&old_buffer,nullptr,&old_index)),"host index buffer");
    old_buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    check(SUCCEEDED(device->CreateBuffer(&old_buffer,nullptr,&old_constant)),"host constant buffer");
    ID3D11Buffer *old_vb=old_vertex.Get(),*old_cb=old_constant.Get();
    UINT old_stride=4,old_offset=4;
    context->IASetVertexBuffers(0,1,&old_vb,&old_stride,&old_offset);
    context->IASetIndexBuffer(old_index.Get(),DXGI_FORMAT_R16_UINT,2);
    context->VSSetConstantBuffers(0,1,&old_cb);
    D3D11_SAMPLER_DESC old_sampler_desc{};
    old_sampler_desc.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
    old_sampler_desc.AddressU=old_sampler_desc.AddressV=old_sampler_desc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
    old_sampler_desc.MaxLOD=D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> old_sampler;
    check(SUCCEEDED(device->CreateSamplerState(&old_sampler_desc,&old_sampler)),"host sampler");
    ID3D11SamplerState *old_s=old_sampler.Get();
    context->PSSetSamplers(0,1,&old_s);
    render_and_retire(backend,context.Get(),target,scene,atlas);
    check(near_pixel(pixel(context.Get(),target,0,0),{191,0,64,255}),"order and premultiplied opacity");
    check(near_pixel(pixel(context.Get(),target,3,3),{191,0,64,255}),"last pixel center");
    D3D11_PRIMITIVE_TOPOLOGY topology{};
    context->IAGetPrimitiveTopology(&topology);
    D3D11_VIEWPORT restored{}; UINT count=1;
    context->RSGetViewports(&count,&restored);
    check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST && count==1 &&
          restored.TopLeftX==1 && restored.Width==2,"host topology and viewport restored");
    ComPtr<ID3D11Buffer> restored_vb,restored_ib,restored_cb;
    ComPtr<ID3D11SamplerState> restored_sampler;
    ComPtr<ID3D11ShaderResourceView> restored_resource;
    UINT stride=0,offset=0,index_offset=0; DXGI_FORMAT index_format=DXGI_FORMAT_UNKNOWN;
    context->IAGetVertexBuffers(0,1,&restored_vb,&stride,&offset);
    context->IAGetIndexBuffer(&restored_ib,&index_format,&index_offset);
    context->VSGetConstantBuffers(0,1,&restored_cb);
    context->PSGetSamplers(0,1,&restored_sampler);
    context->PSGetShaderResources(0,1,&restored_resource);
    check(restored_vb.Get()==old_vertex.Get() && stride==4 && offset==4 &&
          restored_ib.Get()==old_index.Get() && index_format==DXGI_FORMAT_R16_UINT &&
          index_offset==2 && restored_cb.Get()==old_constant.Get() &&
          restored_sampler.Get()==old_sampler.Get() && !restored_resource,
          "host IA, VS constant and PS input state restored");
    layers[1].visible=false;
    parts[0].rect={0,0,2,1};
    parts[0].uv={{-2,0},{3,0},{-2,1},{3,1}};
    check(prepare_static_2d(input,scene)==ARS_OK,"UV rectangle prepared");
    render_and_retire(backend,context.Get(),target,scene,atlas);
    check(near_pixel(pixel(context.Get(),target,0,0),{255,0,0,255}) &&
          near_pixel(pixel(context.Get(),target,3,0),{0,0,128,128}),
          "out-of-range UV clamps within rectangle without green atlas bleed");
    parts[0].rect={0,0,1,1};
    parts[0].uv={{0,0},{1,0},{0,1},{1,1}};
    check(prepare_static_2d(input,scene)==ARS_OK,"hidden prepared");
    render_and_retire(backend,context.Get(),target,scene,atlas);
    check(near_pixel(pixel(context.Get(),target,2,2),{255,0,0,255}),"hidden layer excluded");
    layers[0].visible=false;
    check(prepare_static_2d(input,scene)==ARS_OK,"empty prepared");
    render_and_retire(backend,context.Get(),target,scene,atlas);
    check(near_pixel(pixel(context.Get(),target,2,2),{0,0,0,0}),"transparent offscreen clear");
    layers[0].visible=true;
    auto larger=make_target(device.Get(),8,8);
    check(prepare_static_2d(input,scene)==ARS_OK,"resize prepared");
    render_and_retire(backend,context.Get(),larger,scene,atlas);
    check(near_pixel(pixel(context.Get(),larger,7,7),{255,0,0,255}),"resized last pixel");
    auto before=pixel(context.Get(),larger,0,0);
    scene.draws[0].atlas_rect[2]=99;
    QueryRetirement rejected;
    check(render_offscreen_2d(backend,larger.rtv.Get(),{19,8,8},scene,atlas,rejected)==
          Result::invalid_argument,"malformed scene rejected");
    check(pixel(context.Get(),larger,0,0)==before,"reject before target mutation");
    for (UINT64 i=0;debug_queue && i<debug_queue->GetNumStoredMessages();++i) {
        SIZE_T size=0;
        check(SUCCEEDED(debug_queue->GetMessage(i,nullptr,&size)) && size,"debug message size");
        std::vector<unsigned char> bytes(size);
        auto *message=reinterpret_cast<D3D11_MESSAGE *>(bytes.data());
        check(SUCCEEDED(debug_queue->GetMessage(i,message,&size)),"debug message");
        if (message->Severity<=D3D11_MESSAGE_SEVERITY_WARNING) {
            std::fprintf(stderr,"D3D11 debug: %s\n",message->pDescription);
            check(false,"zero D3D11 warnings/errors");
        }
    }
    if (debug_queue)
        std::printf("M2B_DEBUG_MESSAGES=%llu\n",
                    static_cast<unsigned long long>(debug_queue->GetNumStoredMessages()));
    std::puts("PASS M2-B private D3D11 pixel, alpha, hide, resize, rollback, retirement and state");
}
