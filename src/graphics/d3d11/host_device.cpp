#include "host_device.hpp"
#ifdef _WIN32
#include "state_scope.hpp"
#include <d3dcompiler.h>
#include <exception>

namespace ars::graphics::d3d11 {
namespace {
constexpr char shader_source[] = R"(
struct Out { float4 pos : SV_Position; };
Out VS(uint id : SV_VertexID) {
    Out o;
    float2 p = float2((id << 1) & 2, id & 2);
    o.pos = float4(p * float2(2,-2) + float2(-1,1), 0, 1);
    return o;
}
float4 PS() : SV_Target { return float4(0.25, 0.5, 0.75, 1); }
)";
bool same_device(ID3D11Device *a, ID3D11Device *b) {
    Microsoft::WRL::ComPtr<IUnknown> ca, cb;
    return a && b && SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&ca)))
        && SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&cb))) && ca.Get()==cb.Get();
}
bool input_conflicts(ID3D11DeviceContext *context, ID3D11Resource *target) {
    // OMSetRenderTargets can silently unbind host read/write bindings that
    // overlap this resource. This narrow spike rejects that situation before
    // touching the pipeline instead of claiming restoration we cannot prove.
    using Getter=void (STDMETHODCALLTYPE ID3D11DeviceContext::*)(UINT,UINT,ID3D11ShaderResourceView **);
    constexpr Getter getters[]{&ID3D11DeviceContext::VSGetShaderResources,
        &ID3D11DeviceContext::HSGetShaderResources,&ID3D11DeviceContext::DSGetShaderResources,
        &ID3D11DeviceContext::GSGetShaderResources,&ID3D11DeviceContext::PSGetShaderResources,
        &ID3D11DeviceContext::CSGetShaderResources};
    for (auto getter : getters) {
        ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
        (context->*getter)(0,D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT,views);
        bool conflict=false;
        for (auto *view : views) if (view) {
            Microsoft::WRL::ComPtr<ID3D11Resource> resource;
            view->GetResource(&resource);
            conflict |= resource.Get()==target;
            view->Release();
        }
        if (conflict) return true;
    }
    ID3D11UnorderedAccessView *cs[D3D11_PS_CS_UAV_REGISTER_COUNT]{};
    ID3D11UnorderedAccessView *om[D3D11_PS_CS_UAV_REGISTER_COUNT]{};
    context->CSGetUnorderedAccessViews(0,D3D11_PS_CS_UAV_REGISTER_COUNT,cs);
    context->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,D3D11_PS_CS_UAV_REGISTER_COUNT,om);
    bool any_uav=false;
    for (auto *view : cs) if (view) { any_uav=true; view->Release(); }
    for (auto *view : om) if (view) { any_uav=true; view->Release(); }
    ID3D11Buffer *so[D3D11_SO_BUFFER_SLOT_COUNT]{};
    context->SOGetTargets(D3D11_SO_BUFFER_SLOT_COUNT,so);
    for (auto *buffer : so) if (buffer) { any_uav=true; buffer->Release(); }
    return any_uav;
}
}
HostDevice::HostDevice(ID3D11Device *device, ID3D11DeviceContext *context, std::uint64_t generation)
    : device_(device), context_(context), owner_(std::this_thread::get_id()), generation_(generation) {}
HostDevice::~HostDevice() {
    if (!on_owner_thread()) std::terminate();
}
Result HostDevice::release_owned_references() {
    if (!on_owner_thread()) return Result::wrong_thread;
    vs_.Reset(); ps_.Reset(); rasterizer_.Reset(); blend_.Reset(); depth_.Reset();
    context_.Reset(); device_.Reset();
    return Result::ok;
}

Result HostDevice::validate(ID3D11RenderTargetView *target, TargetInfo info) const {
    if (!on_owner_thread()) return Result::wrong_thread;
    if (!device_ || !context_ || !target || !generation_ || !info.generation || !info.width || !info.height)
        return Result::invalid_argument;
    if (info.generation!=generation_) return Result::invalid_argument;
    if (FAILED(device_->GetDeviceRemovedReason())) return Result::device_lost;
    Microsoft::WRL::ComPtr<ID3D11Device> context_device, target_device;
    context_->GetDevice(&context_device);
    target->GetDevice(&target_device);
    if (!same_device(device_.Get(), context_device.Get()) || !same_device(device_.Get(), target_device.Get()))
        return Result::invalid_argument;
    D3D11_RENDER_TARGET_VIEW_DESC view_desc{};
    target->GetDesc(&view_desc);
    if (view_desc.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D || view_desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)
        return Result::unsupported;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    target->GetResource(&resource);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (!resource || FAILED(resource.As(&texture))) return Result::invalid_argument;
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    if (desc.Width!=info.width || desc.Height!=info.height || desc.SampleDesc.Count!=1 ||
        desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM || view_desc.Texture2D.MipSlice!=0)
        return Result::unsupported;
    if (input_conflicts(context_.Get(), resource.Get())) return Result::busy;
    return Result::ok;
}
Result HostDevice::init() {
    if (vs_ && ps_ && rasterizer_ && blend_ && depth_) return Result::ok;
    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob, ps_blob, errors;
    if (FAILED(D3DCompile(shader_source, sizeof(shader_source)-1, nullptr, nullptr, nullptr,
                          "VS", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs_blob, &errors)) ||
        FAILED(D3DCompile(shader_source, sizeof(shader_source)-1, nullptr, nullptr, nullptr,
                          "PS", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps_blob, &errors)))
        return Result::failed;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rs;
    Microsoft::WRL::ComPtr<ID3D11BlendState> bs;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> ds;
    if (FAILED(device_->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs)) ||
        FAILED(device_->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps))) return Result::failed;
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode=D3D11_FILL_SOLID; raster.CullMode=D3D11_CULL_NONE; raster.DepthClipEnable=TRUE;
    if (FAILED(device_->CreateRasterizerState(&raster, &rs))) return Result::failed;
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&blend, &bs))) return Result::failed;
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable=FALSE; depth.StencilEnable=FALSE;
    if (FAILED(device_->CreateDepthStencilState(&depth, &ds))) return Result::failed;
    vs_=std::move(vs); ps_=std::move(ps); rasterizer_=std::move(rs); blend_=std::move(bs); depth_=std::move(ds);
    return Result::ok;
}
Result HostDevice::submit(ID3D11RenderTargetView *target, TargetInfo info) {
    const auto valid=validate(target, info);
    if (valid!=Result::ok) return valid;
    const auto ready=init();
    if (ready!=Result::ok) return ready;
    StateScope restore(context_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->PSSetShader(ps_.Get(), nullptr, 0);
    context_->RSSetState(rasterizer_.Get());
    D3D11_VIEWPORT viewport{0, 0, static_cast<FLOAT>(info.width), static_cast<FLOAT>(info.height), 0, 1};
    context_->RSSetViewports(1, &viewport);
    ID3D11RenderTargetView *views[]={target};
    context_->OMSetRenderTargets(1, views, nullptr);
    constexpr FLOAT factor[4]{1,1,1,1};
    context_->OMSetBlendState(blend_.Get(), factor, 0xffffffffu);
    context_->OMSetDepthStencilState(depth_.Get(), 0);
    context_->Draw(3, 0);
    return FAILED(device_->GetDeviceRemovedReason()) ? Result::device_lost : Result::ok;
}
}
#endif
