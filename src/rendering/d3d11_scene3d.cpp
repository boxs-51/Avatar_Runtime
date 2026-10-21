#include "d3d11_scene3d.hpp"
#ifdef _WIN32
#include "../graphics/d3d11/state_scope.hpp"
#include "../graphics/d3d11/creation_status.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace ars::rendering {
namespace {
using Microsoft::WRL::ComPtr;
using graphics::Result;
struct Vertex {
    float clip[4], normal[3], uv[2], base[4];
    float metallic,roughness,cutoff,mode;
    float has_base,has_mr,double_sided;
};
static_assert(sizeof(Vertex)==80);
struct Range {
    UINT start=0,count=0;
    float depth=0;
    Material3D::Alpha alpha=Material3D::Alpha::opaque;
    ComPtr<ID3D11Texture2D> base_texture,mr_texture;
    ComPtr<ID3D11ShaderResourceView> base_view,mr_view;
    ComPtr<ID3D11SamplerState> base_sampler,mr_sampler;
};
struct Frame {
    graphics::d3d11::ContextOwnership::Participant gpu_owner;
    std::shared_ptr<const PreparedScene3D> snapshot;
    std::shared_ptr<const assets::GlbModel> source;
    ComPtr<ID3D11Buffer> vertices,indices;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11BlendState> opaque_blend,alpha_blend;
    ComPtr<ID3D11DepthStencilState> depth_write,depth_read;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11SamplerState> fallback_sampler;
    std::vector<Range> ranges;
};
template<class Create>
Result create_resource(ID3D11Device *device,GpuCreateProbe *probe,Create &&create) {
    const bool injected=probe && !probe->allow();
    const HRESULT hr=injected?probe->failure:create();
    if (SUCCEEDED(hr)) return Result::ok;
    const HRESULT reason=injected && FAILED(probe->removed_reason)?
        probe->removed_reason:device->GetDeviceRemovedReason();
    return graphics::d3d11::creation_status(hr,reason);
}
constexpr char shader[]=R"(
Texture2D baseTexture : register(t0);
Texture2D mrTexture : register(t1);
SamplerState baseSampler : register(s0);
SamplerState mrSampler : register(s1);
struct Input {
    float4 clip : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0;
    float4 base : COLOR0; float4 properties : TEXCOORD1;
    float3 flags : TEXCOORD2;
};
struct Output {
    float4 clip : SV_Position; float3 normal : NORMAL; float2 uv : TEXCOORD0;
    float4 base : COLOR0; float4 properties : TEXCOORD1;
    float3 flags : TEXCOORD2;
};
Output VS(Input i) {
    Output o; o.clip=i.clip; o.normal=i.normal; o.uv=i.uv;
    o.base=i.base;o.properties=i.properties;o.flags=i.flags;return o;
}
float4 PS(Output i,bool front : SV_IsFrontFace) : SV_Target {
    if (!front && i.flags.z<0.5) discard;
    float4 sampled=i.flags.x>0.5?baseTexture.Sample(baseSampler,i.uv):float4(1,1,1,1);
    // The sRGB SRV decodes sampled RGB before this multiplication.
    float4 base=float4(i.base.rgb*sampled.rgb,i.base.a*sampled.a);
    float metallic=i.properties.x,roughness=i.properties.y;
    if (i.flags.y>0.5) {
        float4 mr=mrTexture.Sample(mrSampler,i.uv);
        metallic*=mr.b;roughness*=mr.g;
    }
    metallic=saturate(metallic);roughness=clamp(roughness,0.04,1.0);
    int mode=(int)(i.properties.w+0.5);
    if (mode==1 && base.a<i.properties.z) discard;
    float alpha=mode==2?base.a:1.0;
    float3 N=normalize(i.normal)*(front?1.0:-1.0);
    float3 L=normalize(float3(0.25,0.5,1.0));
    float3 V=float3(0,0,1);
    float3 H=normalize(L+V);
    float NL=saturate(dot(N,L)),NV=saturate(dot(N,V));
    float NH=saturate(dot(N,H)),VH=saturate(dot(V,H));
    float3 F0=lerp(float3(0.04,0.04,0.04),base.rgb,metallic);
    float3 F=F0+(1.0-F0)*pow(1.0-VH,5.0);
    float a=roughness*roughness,a2=a*a;
    float d=NH*NH*(a2-1.0)+1.0;
    float D=a2/(3.14159265*d*d+0.000001);
    float k=(roughness+1.0)*(roughness+1.0)/8.0;
    float G=(NL/(NL*(1.0-k)+k+0.000001))*(NV/(NV*(1.0-k)+k+0.000001));
    float3 spec=D*G*F/(4.0*NL*NV+0.000001);
    float3 diffuse=(1.0-F)*(1.0-metallic)*base.rgb/3.14159265;
    float3 lit=(diffuse+spec)*NL*0.75+base.rgb*(1.0-metallic)*0.08+F0*0.02;
    float3 mapped=lit/(1.0+lit);
    float3 encoded=lerp(mapped*12.92,1.055*pow(mapped,1.0/2.4)-0.055,
                        step(0.0031308,mapped));
    return float4(saturate(encoded)*alpha,alpha);
}
)";
Result preflight(const PreparedScene3D &scene,const Camera3D &camera,
               graphics::TargetInfo info) {
    if (!scene.source || !info.width || !info.height || scene.draws.empty() ||
        !scene.gpu_budget || scene.gpu_budget>256*assets::MiB) return Result::invalid_argument;
    if (!std::all_of(camera.clip_from_world.begin(),camera.clip_from_world.end(),
                     [](double n){return std::isfinite(n);})) return Result::invalid_argument;
    std::size_t vertices=0,indices=0;
    for (const auto &draw:scene.draws) {
        if (draw.indices.size()%3) return Result::invalid_argument;
        if (draw.vertices.size()>2'000'000-vertices || draw.indices.size()>6'000'000-indices)
            return Result::out_of_budget;
        vertices+=draw.vertices.size(); indices+=draw.indices.size();
        for (const auto &v:draw.vertices)
            for (float n:v.position) if (!std::isfinite(n)) return Result::invalid_argument;
        for (const auto &v:draw.vertices) {
            for (float n:v.normal) if (!std::isfinite(n)) return Result::invalid_argument;
            for (float n:v.uv) if (!std::isfinite(n)) return Result::invalid_argument;
            for (float n:v.color)
                if (!std::isfinite(n) || n<0 || n>1) return Result::invalid_argument;
        }
        for (float n:draw.material.base_color)
            if (!std::isfinite(n) || n<0 || n>1) return Result::invalid_argument;
        if (!std::isfinite(draw.material.metallic) || draw.material.metallic<0 ||
            draw.material.metallic>1 || !std::isfinite(draw.material.roughness) ||
            draw.material.roughness<0 || draw.material.roughness>1 ||
            !std::isfinite(draw.material.alpha_cutoff) || draw.material.alpha_cutoff<0)
            return Result::invalid_argument;
        for (auto index:draw.indices) if (index>=draw.vertices.size()) return Result::invalid_argument;
    }
    // All renderer-owned resources, including depth, have an explicit ceiling.
    std::uint64_t texture_bytes=0;
    for (const auto &draw:scene.draws)
        for (const auto &ref:{draw.material.base_texture,draw.material.metallic_roughness_texture})
            if (ref.image!=assets::glb_none) {
                if (ref.image>=scene.source->data().images.size()) return Result::invalid_argument;
                const auto &image=scene.source->data().images[ref.image];
                if (!image.width || !image.height ||
                    std::uint64_t(image.width)*image.height*4!=image.rgba.size()) return Result::invalid_argument;
                texture_bytes+=std::uint64_t(image.width)*image.height*6; // mip chain upper bound
            }
    const std::uint64_t bytes=std::uint64_t(vertices)*sizeof(Vertex)+
        std::uint64_t(indices)*sizeof(std::uint32_t)+
        std::uint64_t(info.width)*info.height*4+texture_bytes;
    return bytes<=scene.gpu_budget?Result::ok:Result::out_of_budget;
}
Result make_texture(ID3D11Device *device,ID3D11DeviceContext *context,
                  const assets::Image &image,const Texture3D &ref,bool srgb,
                  ComPtr<ID3D11Texture2D> &texture,
                  ComPtr<ID3D11ShaderResourceView> &view,
                  ComPtr<ID3D11SamplerState> &sampler,GpuCreateProbe *probe) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=image.width;desc.Height=image.height;desc.MipLevels=0;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateTexture2D(&desc,nullptr,&texture); });
        created!=Result::ok) return created;
    const auto format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format=format;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MostDetailedMip=0;srv.Texture2D.MipLevels=UINT(-1);
    if (const auto created=create_resource(device,probe,[&] { return device->CreateShaderResourceView(texture.Get(),&srv,&view); });
        created!=Result::ok) return created;
    context->UpdateSubresource(texture.Get(),0,nullptr,image.rgba.data(),image.width*4,0);
    context->GenerateMips(view.Get());
    D3D11_SAMPLER_DESC s{};
    const bool min_linear=ref.min_filter==9729 || ref.min_filter==9985 || ref.min_filter==9987;
    const bool mag_linear=ref.mag_filter==9729;
    const bool mip_linear=ref.min_filter==9986 || ref.min_filter==9987;
    s.Filter=static_cast<D3D11_FILTER>((min_linear?0x10:0)|(mag_linear?0x04:0)|(mip_linear?0x01:0));
    if (ref.min_filter==9728 || ref.min_filter==9729) s.MaxLOD=0;
    else s.MaxLOD=D3D11_FLOAT32_MAX;
    auto address=[](std::uint32_t wrap) {
        return wrap==33071?D3D11_TEXTURE_ADDRESS_CLAMP:
               wrap==33648?D3D11_TEXTURE_ADDRESS_MIRROR:D3D11_TEXTURE_ADDRESS_WRAP;
    };
    s.AddressU=address(ref.wrap_s);s.AddressV=address(ref.wrap_t);
    s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    return create_resource(device,probe,[&] { return device->CreateSamplerState(&s,&sampler); });
}
Result build(ID3D11Device *device,const PreparedScene3D &scene,const Camera3D &camera,
             graphics::TargetInfo info,ID3D11DeviceContext *context,Frame &frame,
             GpuCreateProbe *probe) {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    for (const auto &draw:scene.draws) {
        const auto base=static_cast<std::uint32_t>(vertices.size());
        Range range;
        range.start=static_cast<UINT>(indices.size());
        range.alpha=draw.material.alpha;
        double center_depth=0;
        for (const auto &v:draw.vertices) {
            Vertex result{};
            const auto &m=camera.clip_from_world;
            for (int row=0;row<4;++row) {
                const double n=m[row]*v.position[0]+m[4+row]*v.position[1]+
                    m[8+row]*v.position[2]+m[12+row];
                if (!std::isfinite(n) || std::abs(n)>(std::numeric_limits<float>::max)())
                    return Result::unsupported;
                result.clip[row]=static_cast<float>(n);
            }
            if (std::abs(result.clip[3])<1e-10f) return Result::unsupported;
            center_depth+=result.clip[2]/result.clip[3];
            std::copy(v.normal.begin(),v.normal.end(),result.normal);
            std::copy(v.uv.begin(),v.uv.end(),result.uv);
            for (std::size_t c=0;c<4;++c) result.base[c]=draw.material.base_color[c]*v.color[c];
            result.metallic=draw.material.metallic;
            result.roughness=draw.material.roughness;
            result.cutoff=draw.material.alpha_cutoff;
            result.mode=draw.material.alpha==Material3D::Alpha::opaque?0.0f:
                draw.material.alpha==Material3D::Alpha::mask?1.0f:2.0f;
            result.has_base=draw.material.base_texture.image==assets::glb_none?0.0f:1.0f;
            result.has_mr=draw.material.metallic_roughness_texture.image==assets::glb_none?0.0f:1.0f;
            result.double_sided=draw.material.double_sided?1.0f:0.0f;
            vertices.push_back(result);
        }
        for (auto index:draw.indices) indices.push_back(base+index);
        range.count=static_cast<UINT>(indices.size()-range.start);
        range.depth=draw.vertices.empty()?0:static_cast<float>(center_depth/draw.vertices.size());
        for (int type=0;type<2;++type) {
            const auto &ref=type?draw.material.metallic_roughness_texture:draw.material.base_texture;
            if (ref.image==assets::glb_none) continue;
            const auto &image=scene.source->data().images[ref.image];
            Result created=type?
                make_texture(device,context,image,ref,false,range.mr_texture,range.mr_view,
                             range.mr_sampler,probe):
                make_texture(device,context,image,ref,true,range.base_texture,
                             range.base_view,range.base_sampler,probe);
            if (created!=Result::ok) return created;
        }
        frame.ranges.push_back(std::move(range));
    }
    std::stable_sort(frame.ranges.begin(),frame.ranges.end(),[](const Range &a,const Range &b) {
        const bool transparent_a=a.alpha==Material3D::Alpha::blend;
        const bool transparent_b=b.alpha==Material3D::Alpha::blend;
        if (transparent_a!=transparent_b) return !transparent_a;
        return transparent_a && a.depth>b.depth;
    });
    frame.source=scene.source;
    D3D11_BUFFER_DESC desc{};
    D3D11_SUBRESOURCE_DATA data{};
    desc.Usage=D3D11_USAGE_IMMUTABLE;
    if (!vertices.empty()) {
        desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        desc.ByteWidth=static_cast<UINT>(vertices.size()*sizeof(Vertex));
        data.pSysMem=vertices.data();
        if (const auto created=create_resource(device,probe,[&] { return device->CreateBuffer(&desc,&data,&frame.vertices); });
        created!=Result::ok) return created;
    }
    if (!indices.empty()) {
        desc.BindFlags=D3D11_BIND_INDEX_BUFFER;
        desc.ByteWidth=static_cast<UINT>(indices.size()*sizeof(std::uint32_t));
        data.pSysMem=indices.data();
        if (const auto created=create_resource(device,probe,[&] { return device->CreateBuffer(&desc,&data,&frame.indices); });
        created!=Result::ok) return created;
    }
    ComPtr<ID3DBlob> vs_code,ps_code,errors;
    if (FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"VS","vs_4_0",
                          D3DCOMPILE_ENABLE_STRICTNESS,0,&vs_code,&errors)) ||
        FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"PS","ps_4_0",
                          D3DCOMPILE_ENABLE_STRICTNESS,0,&ps_code,&errors)))
        return Result::failed;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateVertexShader(vs_code->GetBufferPointer(),vs_code->GetBufferSize(),nullptr,&frame.vs); });
        created!=Result::ok) return created;
    if (const auto created=create_resource(device,probe,[&] { return device->CreatePixelShader(ps_code->GetBufferPointer(),ps_code->GetBufferSize(),nullptr,&frame.ps); });
        created!=Result::ok) return created;
    const D3D11_INPUT_ELEMENT_DESC layout[]{
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,clip),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,normal),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(Vertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,base),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(Vertex,metallic),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",2,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,has_base),D3D11_INPUT_PER_VERTEX_DATA,0}};
    if (const auto created=create_resource(device,probe,[&] { return device->CreateInputLayout(layout,6,vs_code->GetBufferPointer(),vs_code->GetBufferSize(),&frame.layout); });
        created!=Result::ok) return created;
    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode=D3D11_FILL_SOLID; raster.CullMode=D3D11_CULL_NONE; raster.DepthClipEnable=TRUE;
    raster.FrontCounterClockwise=camera.front_counter_clockwise?TRUE:FALSE;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateRasterizerState(&raster,&frame.raster); });
        created!=Result::ok) return created;
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateBlendState(&blend,&frame.opaque_blend); });
        created!=Result::ok) return created;
    blend.RenderTarget[0].BlendEnable=TRUE;
    blend.RenderTarget[0].SrcBlend=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateBlendState(&blend,&frame.alpha_blend); });
        created!=Result::ok) return created;
    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable=TRUE; depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc=D3D11_COMPARISON_LESS; depth.StencilEnable=FALSE;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateDepthStencilState(&depth,&frame.depth_write); });
        created!=Result::ok) return created;
    depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateDepthStencilState(&depth,&frame.depth_read); });
        created!=Result::ok) return created;
    D3D11_TEXTURE2D_DESC texture{};
    texture.Width=info.width;texture.Height=info.height;texture.MipLevels=1;texture.ArraySize=1;
    texture.Format=DXGI_FORMAT_D32_FLOAT;texture.SampleDesc.Count=1;
    texture.Usage=D3D11_USAGE_DEFAULT;texture.BindFlags=D3D11_BIND_DEPTH_STENCIL;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateTexture2D(&texture,nullptr,&frame.depth); });
        created!=Result::ok) return created;
    if (const auto created=create_resource(device,probe,[&] { return device->CreateDepthStencilView(frame.depth.Get(),nullptr,&frame.dsv); });
        created!=Result::ok) return created;
    // Both shader sampler slots must be bound even when a material has no
    // texture. Keep the fallback owned by this exact frame/query generation.
    D3D11_SAMPLER_DESC fallback{};
    fallback.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    fallback.AddressU=fallback.AddressV=fallback.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    fallback.MaxAnisotropy=1;fallback.ComparisonFunc=D3D11_COMPARISON_NEVER;
    fallback.MaxLOD=D3D11_FLOAT32_MAX;
    if(const auto created=create_resource(device,probe,[&]{return device->CreateSamplerState(&fallback,&frame.fallback_sampler);});
       created!=Result::ok)return created;
    return Result::ok;
}
} // namespace

graphics::Result render_offscreen_3d(graphics::d3d11::HostDevice &backend,
    ID3D11RenderTargetView *target,graphics::TargetInfo info,
    std::shared_ptr<const PreparedScene3D> scene,const Camera3D &camera,
    graphics::d3d11::QueryRetirement &retirement,
    ComPtr<ID3D11Texture2D> *depth_capture,GpuCreateProbe *probe) {
    if (!backend.on_owner_thread()) return Result::wrong_thread;
    auto operation=backend.enter_context();
    if(operation.result()!=Result::ok)return operation.result();
    const auto valid=backend.validate(target,info);
    if (valid!=Result::ok) return valid;
    if (!scene || (backend.ownership()&&depth_capture)) return Result::invalid_argument;
    const auto admission=preflight(*scene,camera,info);
    if (admission!=Result::ok) return admission;
    try {
        auto frame=std::make_shared<Frame>();
        if(backend.ownership()){const auto r=backend.ownership()->retain(frame->gpu_owner);if(r!=Result::ok)return r;}
        frame->snapshot=scene;
        const auto ready=build(backend.device(),*scene,camera,info,backend.context(),*frame,probe);
        if (ready!=Result::ok) {
            if (ready==Result::device_lost) backend.mark_lost();
            return ready;
        }
        const auto prepared=retirement.prepare_split(backend.device(),backend.context(),target,
                                                backend.generation(),frame->snapshot,frame);
        if (prepared!=Result::ok) {
            if (prepared==Result::device_lost) backend.mark_lost();
            return prepared;
        }
        {
            graphics::d3d11::StateScope restore(backend.context(),backend.ownership());
            if(restore.result()!=Result::ok){retirement.cancel_unsubmitted();return restore.result();}
            auto *context=backend.context();
            constexpr FLOAT clear[4]{0,0,0,0};
            context->ClearRenderTargetView(target,clear);
            context->ClearDepthStencilView(frame->dsv.Get(),D3D11_CLEAR_DEPTH,1,0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->IASetInputLayout(frame->layout.Get());
            ID3D11Buffer *vb=frame->vertices.Get();
            constexpr UINT stride=sizeof(Vertex),offset=0;
            context->IASetVertexBuffers(0,1,&vb,&stride,&offset);
            context->IASetIndexBuffer(frame->indices.Get(),DXGI_FORMAT_R32_UINT,0);
            context->VSSetShader(frame->vs.Get(),nullptr,0);
            context->PSSetShader(frame->ps.Get(),nullptr,0);
            context->RSSetState(frame->raster.Get());
            D3D11_VIEWPORT viewport{0,0,static_cast<FLOAT>(info.width),static_cast<FLOAT>(info.height),0,1};
            context->RSSetViewports(1,&viewport);
            ID3D11RenderTargetView *views[]{target};
            context->OMSetRenderTargets(1,views,frame->dsv.Get());
            constexpr FLOAT factor[4]{1,1,1,1};
            for (const auto &range:frame->ranges) {
                const bool transparent=range.alpha==Material3D::Alpha::blend;
                context->OMSetBlendState(transparent?frame->alpha_blend.Get():frame->opaque_blend.Get(),
                                         factor,0xffffffffu);
                context->OMSetDepthStencilState(transparent?frame->depth_read.Get():frame->depth_write.Get(),0);
                ID3D11ShaderResourceView *shader_views[]{range.base_view.Get(),range.mr_view.Get()};
                ID3D11SamplerState *samplers[]{range.base_sampler?range.base_sampler.Get():frame->fallback_sampler.Get(),
                    range.mr_sampler?range.mr_sampler.Get():frame->fallback_sampler.Get()};
                context->PSSetShaderResources(0,2,shader_views);
                context->PSSetSamplers(0,2,samplers);
                if (range.count) context->DrawIndexed(range.count,range.start,0);
            }
        }
        if (FAILED(backend.device()->GetDeviceRemovedReason())) {
            retirement.mark_lost();backend.mark_lost();return Result::device_lost;
        }
        if (depth_capture) *depth_capture=frame->depth;
        return retirement.end();
    } catch (const std::bad_alloc &) { return Result::out_of_budget;
    } catch (...) { return Result::failed; }
}
graphics::Result render_offscreen_3d(graphics::d3d11::HostDevice &backend,
    ID3D11RenderTargetView *target,graphics::TargetInfo info,
    const PreparedScene3D &scene,const Camera3D &camera,
    graphics::d3d11::QueryRetirement &retirement,
    ComPtr<ID3D11Texture2D> *depth_capture,GpuCreateProbe *probe) {
    if (!backend.on_owner_thread()) return Result::wrong_thread;
    try {
        std::shared_ptr<const PreparedScene3D> snapshot=std::make_shared<PreparedScene3D>(scene);
        return render_offscreen_3d(backend,target,info,std::move(snapshot),camera,
                                   retirement,depth_capture,probe);
    } catch (const std::bad_alloc &) { return Result::out_of_budget;
    } catch (...) { return Result::failed; }
}
} // namespace ars::rendering
#endif
