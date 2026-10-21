#include "d3d11_scene2d.hpp"
#ifdef _WIN32
#include "../graphics/d3d11/state_scope.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>

namespace ars::rendering {
namespace {
using Microsoft::WRL::ComPtr;
using graphics::Result;
struct Vertex {
    float x,y,u,v,rx,ry,rw,rh,opacity;
};
struct Constants { float canvas_x,canvas_y,atlas_x,atlas_y; };
struct Range { UINT count, start; };
struct FrameResources {
    graphics::d3d11::ContextOwnership::Participant gpu_owner;
    std::shared_ptr<const void> snapshot_owner;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    ComPtr<ID3D11Buffer> vertices, indices, constants;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11DepthStencilState> depth;
    std::vector<Range> ranges;
};
constexpr char shader[] = R"(
cbuffer Dimensions : register(b0) { float2 canvas; float2 atlasSize; };
Texture2D atlasImage : register(t0);
SamplerState nearestClamp : register(s0);
struct Input { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 rect : TEXCOORD1; float opacity : TEXCOORD2; };
struct Output { float4 pos : SV_Position; float2 uv : TEXCOORD0; float opacity : TEXCOORD1; };
Output VS(Input v) {
    Output o;
    o.pos=float4(2.0*v.pos.x/canvas.x-1.0,1.0-2.0*v.pos.y/canvas.y,0.0,1.0);
    o.uv=(v.rect.xy+0.5+saturate(v.uv)*max(v.rect.zw-1.0,0.0))/atlasSize;
    o.opacity=v.opacity;
    return o;
}
float4 PS(Output i) : SV_Target {
    float4 texel=atlasImage.Sample(nearestClamp,i.uv);
    float a=texel.a*i.opacity;
    return float4(texel.rgb*a,a);
}
)";
bool valid_scene(const PreparedScene2D &scene,const assets::Image &atlas) {
    if (!scene.canvas[0] || !scene.canvas[1] || !atlas.width || !atlas.height ||
        scene.atlas_width!=atlas.width || scene.atlas_height!=atlas.height ||
        scene.draws.size()>128 || atlas.width>8192 || atlas.height>8192 ||
        std::uint64_t(atlas.width)*atlas.height*4!=atlas.rgba.size()) return false;
    std::uint64_t vertices=0, triangles=0;
    for (const auto &draw : scene.draws) {
        if (!std::isfinite(draw.opacity) || draw.opacity<0 || draw.opacity>1 ||
            draw.uv.size()!=draw.vertices.size() || !draw.atlas_rect[2] || !draw.atlas_rect[3] ||
            draw.atlas_rect[0]>atlas.width || draw.atlas_rect[1]>atlas.height ||
            draw.atlas_rect[2]>atlas.width-draw.atlas_rect[0] ||
            draw.atlas_rect[3]>atlas.height-draw.atlas_rect[1]) return false;
        vertices+=draw.vertices.size(); triangles+=draw.triangles.size();
        if (vertices>2'000'000 || triangles>2'000'000) return false;
        for (std::size_t i=0;i<draw.vertices.size();++i)
            if (!std::isfinite(draw.vertices[i].x) || !std::isfinite(draw.vertices[i].y) ||
                !std::isfinite(draw.uv[i].x) || !std::isfinite(draw.uv[i].y)) return false;
        for (const auto &tri : draw.triangles)
            if (tri[0]>=draw.vertices.size() || tri[1]>=draw.vertices.size() ||
                tri[2]>=draw.vertices.size()) return false;
    }
    return true;
}
template<class Interface, class Descriptor>
bool create_state(ID3D11Device *device,const Descriptor &desc,
                  HRESULT (STDMETHODCALLTYPE ID3D11Device::*method)(const Descriptor *,Interface **),
                  ComPtr<Interface> &result) {
    return SUCCEEDED((device->*method)(&desc,result.GetAddressOf()));
}
Result build(ID3D11Device *device,const PreparedScene2D &scene,const assets::Image &atlas,
             FrameResources &frame) {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::size_t vertex_count=0,index_count=0;
    for (const auto &draw : scene.draws) {
        vertex_count+=draw.vertices.size(); index_count+=draw.triangles.size()*3;
    }
    vertices.reserve(vertex_count); indices.reserve(index_count); frame.ranges.reserve(scene.draws.size());
    for (const auto &draw : scene.draws) {
        const auto base=static_cast<std::uint32_t>(vertices.size());
        const auto start=static_cast<UINT>(indices.size());
        for (std::size_t i=0;i<draw.vertices.size();++i)
            vertices.push_back({draw.vertices[i].x,draw.vertices[i].y,draw.uv[i].x,draw.uv[i].y,
                static_cast<float>(draw.atlas_rect[0]),static_cast<float>(draw.atlas_rect[1]),
                static_cast<float>(draw.atlas_rect[2]),static_cast<float>(draw.atlas_rect[3]),draw.opacity});
        for (const auto &tri : draw.triangles)
            for (auto index : tri) indices.push_back(base+index);
        frame.ranges.push_back({static_cast<UINT>(indices.size()-start),start});
    }
    D3D11_TEXTURE2D_DESC image{};
    image.Width=atlas.width; image.Height=atlas.height; image.MipLevels=1; image.ArraySize=1;
    image.Format=DXGI_FORMAT_R8G8B8A8_UNORM; image.SampleDesc.Count=1;
    image.Usage=D3D11_USAGE_IMMUTABLE; image.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA pixels{};
    pixels.pSysMem=atlas.rgba.data(); pixels.SysMemPitch=atlas.width*4;
    if (FAILED(device->CreateTexture2D(&image,&pixels,&frame.texture)) ||
        FAILED(device->CreateShaderResourceView(frame.texture.Get(),nullptr,&frame.view))) return Result::failed;
    if (!vertices.empty()) {
        D3D11_BUFFER_DESC desc{};
        desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        desc.ByteWidth=static_cast<UINT>(vertices.size()*sizeof(Vertex));
        D3D11_SUBRESOURCE_DATA data{}; data.pSysMem=vertices.data();
        if (FAILED(device->CreateBuffer(&desc,&data,&frame.vertices))) return Result::failed;
    }
    if (!indices.empty()) {
        D3D11_BUFFER_DESC desc{};
        desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_INDEX_BUFFER;
        desc.ByteWidth=static_cast<UINT>(indices.size()*sizeof(std::uint32_t));
        D3D11_SUBRESOURCE_DATA data{}; data.pSysMem=indices.data();
        if (FAILED(device->CreateBuffer(&desc,&data,&frame.indices))) return Result::failed;
    }
    const Constants constants{static_cast<float>(scene.canvas[0]),static_cast<float>(scene.canvas[1]),
                              static_cast<float>(atlas.width),static_cast<float>(atlas.height)};
    D3D11_BUFFER_DESC desc{};
    desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    desc.ByteWidth=sizeof(Constants);
    D3D11_SUBRESOURCE_DATA data{}; data.pSysMem=&constants;
    if (FAILED(device->CreateBuffer(&desc,&data,&frame.constants))) return Result::failed;
    ComPtr<ID3DBlob> vs_code,ps_code,errors;
    if (FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"VS","vs_4_0",
                          D3DCOMPILE_ENABLE_STRICTNESS,0,&vs_code,&errors)) ||
        FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"PS","ps_4_0",
                          D3DCOMPILE_ENABLE_STRICTNESS,0,&ps_code,&errors)) ||
        FAILED(device->CreateVertexShader(vs_code->GetBufferPointer(),vs_code->GetBufferSize(),nullptr,&frame.vs)) ||
        FAILED(device->CreatePixelShader(ps_code->GetBufferPointer(),ps_code->GetBufferSize(),nullptr,&frame.ps)))
        return Result::failed;
    const D3D11_INPUT_ELEMENT_DESC layout[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",2,DXGI_FORMAT_R32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if (FAILED(device->CreateInputLayout(layout,4,vs_code->GetBufferPointer(),vs_code->GetBufferSize(),&frame.layout)))
        return Result::failed;
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD=D3D11_FLOAT32_MAX;
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode=D3D11_FILL_SOLID; raster.CullMode=D3D11_CULL_NONE; raster.DepthClipEnable=TRUE;
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable=TRUE;
    blend.RenderTarget[0].SrcBlend=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable=FALSE; depth.StencilEnable=FALSE;
    if (!create_state(device,sampler,&ID3D11Device::CreateSamplerState,frame.sampler) ||
        !create_state(device,raster,&ID3D11Device::CreateRasterizerState,frame.raster) ||
        !create_state(device,blend,&ID3D11Device::CreateBlendState,frame.blend) ||
        !create_state(device,depth,&ID3D11Device::CreateDepthStencilState,frame.depth)) return Result::failed;
    return Result::ok;
}
} // namespace

graphics::Result render_offscreen_2d(graphics::d3d11::HostDevice &backend,
    ID3D11RenderTargetView *target,graphics::TargetInfo info,
    const PreparedScene2D &scene,const assets::Image &atlas,
    graphics::d3d11::QueryRetirement &retirement,
    std::shared_ptr<const void> snapshot_owner) {
    if (!backend.on_owner_thread()) return Result::wrong_thread;
    auto operation=backend.enter_context();
    if(operation.result()!=Result::ok)return operation.result();
    const auto valid=backend.validate(target,info);
    if (valid!=Result::ok) return valid;
    if (!valid_scene(scene,atlas)) return Result::invalid_argument;
    try {
        auto frame=std::make_shared<FrameResources>();
        if(backend.ownership()){const auto r=backend.ownership()->retain(frame->gpu_owner);if(r!=Result::ok)return r;}
        frame->snapshot_owner=std::move(snapshot_owner);
        const auto ready=build(backend.device(),scene,atlas,*frame);
        if (ready!=Result::ok) return ready;
        const auto prepared=retirement.prepare_split(backend.device(),backend.context(),target,
                                                backend.generation(),frame->snapshot_owner,frame);
        if (prepared!=Result::ok) return prepared;
        {
            graphics::d3d11::StateScope restore(backend.context(),backend.ownership());
            if(restore.result()!=Result::ok){retirement.cancel_unsubmitted();return restore.result();}
            auto *context=backend.context();
            constexpr FLOAT transparent[4]{0,0,0,0};
            context->ClearRenderTargetView(target,transparent);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->IASetInputLayout(frame->layout.Get());
            ID3D11Buffer *vb=frame->vertices.Get();
            constexpr UINT stride=sizeof(Vertex),offset=0;
            context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
            context->IASetIndexBuffer(frame->indices.Get(),DXGI_FORMAT_R32_UINT,0);
            context->VSSetShader(frame->vs.Get(),nullptr,0);
            context->PSSetShader(frame->ps.Get(),nullptr,0);
            ID3D11Buffer *cb=frame->constants.Get();
            context->VSSetConstantBuffers(0,1,&cb);
            ID3D11ShaderResourceView *srv=frame->view.Get();
            ID3D11SamplerState *sampler=frame->sampler.Get();
            context->PSSetShaderResources(0,1,&srv);
            context->PSSetSamplers(0,1,&sampler);
            context->RSSetState(frame->raster.Get());
            D3D11_VIEWPORT viewport{0,0,static_cast<FLOAT>(info.width),static_cast<FLOAT>(info.height),0,1};
            context->RSSetViewports(1,&viewport);
            ID3D11RenderTargetView *views[]={target};
            context->OMSetRenderTargets(1,views,nullptr);
            constexpr FLOAT factor[4]{1,1,1,1};
            context->OMSetBlendState(frame->blend.Get(),factor,0xffffffffu);
            context->OMSetDepthStencilState(frame->depth.Get(),0);
            for (const auto &range : frame->ranges)
                if (range.count) context->DrawIndexed(range.count,range.start,0);
        }
        if (FAILED(backend.device()->GetDeviceRemovedReason())) {
            retirement.mark_lost();
            return Result::device_lost;
        }
        return retirement.end();
    } catch (const std::bad_alloc &) {
        return Result::failed;
    } catch (...) {
        return Result::failed;
    }
}
} // namespace ars::rendering
#endif
