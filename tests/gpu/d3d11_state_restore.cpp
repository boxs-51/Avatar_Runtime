#include "graphics/d3d11/host_device.hpp"
#include "graphics/d3d11/state_scope.hpp"
#include <d3dcompiler.h>
#include <array>
#include <cstdio>
#include <cstring>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
using ars::graphics::d3d11::HostDevice;
using ars::graphics::d3d11::StateScope;

namespace {
struct Pipeline {
    D3D11_PRIMITIVE_TOPOLOGY topology{};
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    UINT vs_class_count=0, ps_class_count=0;
    ComPtr<ID3D11RasterizerState> rasterizer;
    UINT viewport_count=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    std::array<D3D11_VIEWPORT,D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
    std::array<ComPtr<ID3D11RenderTargetView>,D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs{};
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11BlendState> blend;
    FLOAT factor[4]{};
    UINT mask{};
    ComPtr<ID3D11DepthStencilState> depth;
    UINT stencil{};
};
Pipeline capture(ID3D11DeviceContext *ctx) {
    Pipeline p;
    ctx->IAGetPrimitiveTopology(&p.topology);
    ctx->VSGetShader(&p.vs,nullptr,&p.vs_class_count);
    ctx->PSGetShader(&p.ps,nullptr,&p.ps_class_count);
    ctx->RSGetState(&p.rasterizer);
    ctx->RSGetViewports(&p.viewport_count,p.viewports.data());
    ID3D11RenderTargetView *views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,views,&p.dsv);
    for (std::size_t i=0;i<p.rtvs.size();++i) p.rtvs[i].Attach(views[i]);
    ctx->OMGetBlendState(&p.blend,p.factor,&p.mask);
    ctx->OMGetDepthStencilState(&p.depth,&p.stencil);
    return p;
}
bool equal(const Pipeline &a,const Pipeline &b) {
    if (a.topology!=b.topology || a.vs.Get()!=b.vs.Get() || a.ps.Get()!=b.ps.Get() ||
        a.vs_class_count!=b.vs_class_count || a.ps_class_count!=b.ps_class_count ||
        a.rasterizer.Get()!=b.rasterizer.Get() || a.viewport_count!=b.viewport_count ||
        a.dsv.Get()!=b.dsv.Get() || a.blend.Get()!=b.blend.Get() || a.mask!=b.mask ||
        a.depth.Get()!=b.depth.Get() || a.stencil!=b.stencil ||
        std::memcmp(a.factor,b.factor,sizeof(a.factor))!=0) return false;
    for (UINT i=0;i<a.viewport_count;++i) if (std::memcmp(&a.viewports[i],&b.viewports[i],sizeof(D3D11_VIEWPORT))!=0) return false;
    for (std::size_t i=0;i<a.rtvs.size();++i) if (a.rtvs[i].Get()!=b.rtvs[i].Get()) return false;
    return true;
}
}
int main() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_DEBUG,
        nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&ctx))) return 2;
    D3D11_TEXTURE2D_DESC color{};
    color.Width=16; color.Height=16; color.ArraySize=1; color.MipLevels=1;
    color.Format=DXGI_FORMAT_R8G8B8A8_UNORM; color.SampleDesc.Count=1;
    color.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> textures[3];
    ComPtr<ID3D11RenderTargetView> rtvs[3];
    for (int i=0;i<3;++i) if (FAILED(device->CreateTexture2D(&color,nullptr,&textures[i])) ||
        FAILED(device->CreateRenderTargetView(textures[i].Get(),nullptr,&rtvs[i]))) return 3;
    D3D11_TEXTURE2D_DESC depth_desc=color;
    depth_desc.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth_texture;
    ComPtr<ID3D11DepthStencilView> dsv;
    if (FAILED(device->CreateTexture2D(&depth_desc,nullptr,&depth_texture)) ||
        FAILED(device->CreateDepthStencilView(depth_texture.Get(),nullptr,&dsv))) return 4;
    D3D11_RASTERIZER_DESC raster_desc{};
    raster_desc.FillMode=D3D11_FILL_WIREFRAME; raster_desc.CullMode=D3D11_CULL_BACK;
    ComPtr<ID3D11RasterizerState> raster;
    if (FAILED(device->CreateRasterizerState(&raster_desc,&raster))) return 5;
    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable=TRUE;
    blend_desc.RenderTarget[0].SrcBlend=D3D11_BLEND_BLEND_FACTOR;
    blend_desc.RenderTarget[0].DestBlend=D3D11_BLEND_INV_BLEND_FACTOR;
    blend_desc.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    ComPtr<ID3D11BlendState> blend;
    if (FAILED(device->CreateBlendState(&blend_desc,&blend))) return 6;
    D3D11_DEPTH_STENCIL_DESC ds_desc{};
    ds_desc.DepthEnable=TRUE; ds_desc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
    ds_desc.DepthFunc=D3D11_COMPARISON_LESS; ds_desc.StencilEnable=FALSE;
    ComPtr<ID3D11DepthStencilState> ds;
    if (FAILED(device->CreateDepthStencilState(&ds_desc,&ds))) return 7;
    constexpr char shader[]="float4 VS(uint id:SV_VertexID):SV_Position {return float4(0,0,0,1);}";
    constexpr char pixel[]="float4 PS():SV_Target {return float4(1,0,0,1);}";
    ComPtr<ID3DBlob> vs_blob, ps_blob;
    if (FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"VS","vs_4_0",0,0,&vs_blob,nullptr)) ||
        FAILED(D3DCompile(pixel,sizeof(pixel)-1,nullptr,nullptr,nullptr,"PS","ps_4_0",0,0,&ps_blob,nullptr))) return 8;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    if (FAILED(device->CreateVertexShader(vs_blob->GetBufferPointer(),vs_blob->GetBufferSize(),nullptr,&vs)) ||
        FAILED(device->CreatePixelShader(ps_blob->GetBufferPointer(),ps_blob->GetBufferSize(),nullptr,&ps))) return 9;
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ctx->VSSetShader(vs.Get(),nullptr,0);
    ctx->PSSetShader(ps.Get(),nullptr,0);
    ctx->RSSetState(raster.Get());
    D3D11_VIEWPORT viewports[2]{{1,2,7,8,0,1},{9,2,7,8,0,1}};
    ctx->RSSetViewports(2,viewports);
    ID3D11RenderTargetView *host_views[]{rtvs[0].Get(),rtvs[1].Get()};
    ctx->OMSetRenderTargets(2,host_views,dsv.Get());
    FLOAT factor[4]{0.2f,0.3f,0.4f,0.5f};
    ctx->OMSetBlendState(blend.Get(),factor,0x12345678u);
    ctx->OMSetDepthStencilState(ds.Get(),17);
    const Pipeline before=capture(ctx.Get());
    HostDevice backend(device.Get(),ctx.Get(),55);
    if (backend.submit(rtvs[2].Get(),{55,16,16})!=Result::ok || !equal(before,capture(ctx.Get()))) return 10;
    if (backend.submit(rtvs[2].Get(),{56,16,16})!=Result::invalid_argument || !equal(before,capture(ctx.Get()))) return 11;
    const auto recoverable_error=[&] {
        StateScope restore(ctx.Get());
        ctx->VSSetShader(nullptr,nullptr,0);
        ctx->OMSetRenderTargets(0,nullptr,nullptr);
        return Result::failed;
    };
    if (recoverable_error()!=Result::failed || !equal(before,capture(ctx.Get()))) return 12;
    std::puts("PASS D04 full touched-state identity/value comparison; success, validation failure, scoped recoverable failure");
    return 0;
}
