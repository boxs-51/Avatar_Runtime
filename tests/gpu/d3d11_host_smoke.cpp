#include "graphics/d3d11/host_device.hpp"
#include <dxgi.h>
#include <cstdio>
#include <thread>
#include <array>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
using ars::graphics::TargetInfo;
using ars::graphics::d3d11::HostDevice;

int main() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    const HRESULT created=D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context);
    if (FAILED(created)) {
        std::printf("HOLD physical D3D11 device unavailable hr=0x%08lx\n", static_cast<unsigned long>(created));
        return 2;
    }
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapter_desc{};
    if (FAILED(device.As(&dxgi)) || FAILED(dxgi->GetAdapter(&adapter)) || FAILED(adapter->GetDesc(&adapter_desc))) return 3;
    std::wprintf(L"GPU=%ls vendor=%04x device=%04x feature=%04x\n",
        adapter_desc.Description, adapter_desc.VendorId, adapter_desc.DeviceId, static_cast<unsigned>(level));
    if (adapter_desc.VendorId==0x1414) { std::puts("HOLD software adapter"); return 4; }
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=16; desc.Height=16; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;
    ComPtr<ID3D11RenderTargetView> rtv;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &target)) ||
        FAILED(device->CreateRenderTargetView(target.Get(), nullptr, &rtv))) return 5;
    HostDevice backend(device.Get(), context.Get(), 17);
    const TargetInfo good{17,16,16};
    if (backend.validate(rtv.Get(), good)!=Result::ok ||
        backend.validate(nullptr, good)!=Result::invalid_argument ||
        backend.validate(rtv.Get(), {18,16,16})!=Result::invalid_argument ||
        backend.validate(rtv.Get(), {17,15,16})!=Result::unsupported) return 6;
    ComPtr<ID3D11Device> other_device;
    ComPtr<ID3D11DeviceContext> other_context;
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
                                 D3D11_SDK_VERSION,&other_device,nullptr,&other_context))) return 6;
    ComPtr<ID3D11Texture2D> other_texture;
    ComPtr<ID3D11RenderTargetView> other_rtv;
    if (FAILED(other_device->CreateTexture2D(&desc,nullptr,&other_texture)) ||
        FAILED(other_device->CreateRenderTargetView(other_texture.Get(),nullptr,&other_rtv)) ||
        backend.validate(other_rtv.Get(),good)!=Result::invalid_argument) return 6;
    Result cross_thread=Result::ok;
    std::thread other([&] { cross_thread=backend.submit(rtv.Get(), good); });
    other.join();
    if (cross_thread!=Result::wrong_thread) return 7;
    // Host state is intentionally nondefault and must survive the private pass.
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    D3D11_VIEWPORT prior{1,2,7,8,0,1};
    context->RSSetViewports(1, &prior);
    ComPtr<ID3D11RasterizerState> host_rs;
    D3D11_RASTERIZER_DESC rs_desc{};
    rs_desc.FillMode=D3D11_FILL_WIREFRAME; rs_desc.CullMode=D3D11_CULL_BACK;
    if (FAILED(device->CreateRasterizerState(&rs_desc,&host_rs))) return 8;
    context->RSSetState(host_rs.Get());
    ComPtr<ID3D11Texture2D> host_texture;
    ComPtr<ID3D11RenderTargetView> host_rtv;
    if (FAILED(device->CreateTexture2D(&desc,nullptr,&host_texture)) ||
        FAILED(device->CreateRenderTargetView(host_texture.Get(),nullptr,&host_rtv))) return 8;
    ID3D11RenderTargetView *host_views[]{host_rtv.Get()};
    context->OMSetRenderTargets(1,host_views,nullptr);
    FLOAT host_factor[4]{0.2f,0.3f,0.4f,0.5f};
    context->OMSetBlendState(nullptr,host_factor,0x12345678u);
    context->OMSetDepthStencilState(nullptr,17);
    if (backend.submit(rtv.Get(), good)!=Result::ok) return 8;
    D3D11_PRIMITIVE_TOPOLOGY after{};
    context->IAGetPrimitiveTopology(&after);
    D3D11_VIEWPORT restored{};
    UINT count=1;
    context->RSGetViewports(&count, &restored);
    ComPtr<ID3D11RasterizerState> restored_rs;
    context->RSGetState(&restored_rs);
    ComPtr<ID3D11RenderTargetView> restored_rtv;
    context->OMGetRenderTargets(1,&restored_rtv,nullptr);
    ComPtr<ID3D11BlendState> restored_blend;
    FLOAT restored_factor[4]{};
    UINT restored_mask=0;
    context->OMGetBlendState(&restored_blend,restored_factor,&restored_mask);
    ComPtr<ID3D11DepthStencilState> restored_depth;
    UINT restored_stencil=0;
    context->OMGetDepthStencilState(&restored_depth,&restored_stencil);
    ComPtr<ID3D11VertexShader> restored_vs;
    ComPtr<ID3D11PixelShader> restored_ps;
    context->VSGetShader(&restored_vs,nullptr,nullptr);
    context->PSGetShader(&restored_ps,nullptr,nullptr);
    if (after!=D3D11_PRIMITIVE_TOPOLOGY_LINELIST || count!=1 ||
        restored.TopLeftX!=prior.TopLeftX || restored.TopLeftY!=prior.TopLeftY ||
        restored.Width!=prior.Width || restored.Height!=prior.Height ||
        restored_rs.Get()!=host_rs.Get() || restored_rtv.Get()!=host_rtv.Get() ||
        restored_blend || restored_depth || restored_stencil!=17 ||
        restored_mask!=0x12345678u || restored_vs || restored_ps) return 9;
    for (unsigned i=0;i<4;++i) if (restored_factor[i]!=host_factor[i]) return 9;
    D3D11_TEXTURE2D_DESC conflict_desc=desc;
    conflict_desc.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> conflict_texture;
    ComPtr<ID3D11RenderTargetView> conflict_rtv;
    ComPtr<ID3D11ShaderResourceView> conflict_srv;
    if (FAILED(device->CreateTexture2D(&conflict_desc,nullptr,&conflict_texture)) ||
        FAILED(device->CreateRenderTargetView(conflict_texture.Get(),nullptr,&conflict_rtv)) ||
        FAILED(device->CreateShaderResourceView(conflict_texture.Get(),nullptr,&conflict_srv))) return 9;
    ID3D11ShaderResourceView *input[]{conflict_srv.Get()};
    context->PSSetShaderResources(0,1,input);
    if (backend.submit(conflict_rtv.Get(),good)!=Result::busy) return 9;
    ComPtr<ID3D11ShaderResourceView> preserved_srv;
    context->PSGetShaderResources(0,1,&preserved_srv);
    if (preserved_srv.Get()!=conflict_srv.Get()) return 9;
    ID3D11ShaderResourceView *none[]{nullptr};
    context->PSSetShaderResources(0,1,none);
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) return 10;
    context->CopyResource(staging.Get(), target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return 11;
    const auto *pixel=static_cast<const unsigned char *>(mapped.pData);
    const bool color=pixel[0]>=62 && pixel[0]<=66 && pixel[1]>=126 && pixel[1]<=129 &&
        pixel[2]>=190 && pixel[2]<=193 && pixel[3]==255;
    std::printf("readback=%u,%u,%u,%u restored=%d\n",pixel[0],pixel[1],pixel[2],pixel[3],after==D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context->Unmap(staging.Get(), 0);
    if (!color) return 12;
    std::puts("PASS D1 physical host draw/readback and selected state restore");
    return 0;
}
