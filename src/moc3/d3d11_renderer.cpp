#include "d3d11_renderer.hpp"
#ifdef _WIN32
#include "../graphics/d3d11/state_scope.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <new>

namespace ars::moc3 {
using Microsoft::WRL::ComPtr;
struct ImageGpu { ComPtr<ID3D11Texture2D> texture; ComPtr<ID3D11ShaderResourceView> srv; };
struct AtlasResidency::State {
    std::shared_ptr<graphics::d3d11::ContextOwnership> ownership;
    graphics::d3d11::ContextOwnership::Participant gpu_owner;
    ComPtr<ID3D11Device> device;
    std::uint64_t generation=0;
    std::shared_ptr<const AtlasSet> atlases;
    std::vector<ImageGpu> images;
};
namespace {
using GfxResult = graphics::Result;
constexpr std::size_t MiB=1024*1024;
struct PackedVertex { float x,y,u,v; Color multiply,screen; float opacity,clip; };
struct Range { UINT count=0,start=0; };
struct CpuPins {std::shared_ptr<const Snapshot> snapshot;std::shared_ptr<const AtlasSet> atlases;};
struct Frame {
    graphics::d3d11::ContextOwnership::Participant gpu_owner;
    std::shared_ptr<const Snapshot> snapshot;
    std::shared_ptr<const AtlasSet> atlases;
    std::vector<ImageGpu> images;
    ImageGpu mask;
    ComPtr<ID3D11RenderTargetView> mask_rtv;
    ComPtr<ID3D11Buffer> vertices,indices,constants;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> color_ps,mask_ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11SamplerState> linear,point;
    std::array<ComPtr<ID3D11RasterizerState>,2> raster;
    std::array<ComPtr<ID3D11BlendState>,3> blend;
    ComPtr<ID3D11DepthStencilState> depth;
    std::vector<Range> ranges;
    std::vector<std::size_t> order;
};
constexpr char shader[]=R"(
cbuffer View : register(b0) { float2 viewScale; float2 viewOffset; };
Texture2D atlas : register(t0);
Texture2D coverage : register(t1);
SamplerState linearClamp : register(s0);
SamplerState pixelClamp : register(s1);
struct In { float2 p:POSITION; float2 uv:TEXCOORD0; float4 mul:TEXCOORD1;
    float4 screen:TEXCOORD2; float2 controls:TEXCOORD3; };
struct Out { float4 p:SV_Position; float2 uv:TEXCOORD0; float4 mul:TEXCOORD1;
    float4 screen:TEXCOORD2; float2 controls:TEXCOORD3; float2 maskUV:TEXCOORD4; };
Out VS(In v) {
    Out o; float2 p=v.p*viewScale+viewOffset;
    o.p=float4(p,0,1); o.uv=float2(v.uv.x,1-v.uv.y);
    o.mul=v.mul; o.screen=v.screen; o.controls=v.controls;
    o.maskUV=float2(p.x*.5+.5,.5-p.y*.5); return o;
}
float4 PS(Out i):SV_Target {
    float4 t=atlas.SampleLevel(linearClamp,i.uv,0);
    float3 c=t.rgb*i.mul.rgb;
    c=c+i.screen.rgb-c*i.screen.rgb;
    float m=1;
    if (i.controls.y>.5) {
        m=coverage.SampleLevel(pixelClamp,i.maskUV,0).r;
        if (i.controls.y>1.5) m=1-m;
    }
    float a=t.a*i.controls.x*m;
    return float4(c*a,a);
}
float4 MaskPS(Out i):SV_Target {
    float a=atlas.SampleLevel(linearClamp,i.uv,0).a;
    return float4(a,a,a,a);
}
)";

bool finite(const Color& c) {
    return std::isfinite(c.r)&&std::isfinite(c.g)&&std::isfinite(c.b)&&std::isfinite(c.a);
}
GfxResult validate(const Snapshot& s,const AtlasSet& a,View v,graphics::TargetInfo info) {
    if (!s.generation || !info.width || !info.height || info.width>2048 || info.height>2048 ||
        !std::isfinite(v.sx)||!std::isfinite(v.sy)||!std::isfinite(v.tx)||!std::isfinite(v.ty) ||
        v.sx<=0 || v.sy<=0 || a.images.empty() || a.images.size()>16 || s.drawables.size()>4096)
        return GfxResult::invalid_argument;
    std::uint64_t texture_bytes=0,vertices=0,indices=0,masks=0,work=0;
    for (const auto& image:a.images) {
        const auto bytes=std::uint64_t(image.width)*image.height*4;
        if (!image.width||!image.height||image.width>8192||image.height>8192 || bytes!=image.rgba.size())
            return GfxResult::invalid_argument;
        texture_bytes+=bytes;
        if (texture_bytes>512*MiB) return GfxResult::out_of_budget;
    }
    for (const auto& d:s.drawables) {
        if (d.texture_index<0 || std::size_t(d.texture_index)>=a.images.size() ||
            !std::isfinite(d.opacity)||d.opacity<0||d.opacity>1 || !finite(d.multiply)||!finite(d.screen) ||
            d.indices.size()%3 || d.blend_mode<0 || d.blend_mode>2) return GfxResult::invalid_argument;
        vertices+=d.vertices.size(); indices+=d.indices.size(); masks+=d.masks.size();
        if (vertices>2'000'000 || indices>12'000'000 || masks>2'000'000 ||
            vertices*sizeof(PackedVertex)+indices*4>192*MiB) return GfxResult::out_of_budget;
        for (const auto& p:d.vertices)
            if (!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.u)||!std::isfinite(p.v) ||
                !std::isfinite(p.x*v.sx+v.tx)||!std::isfinite(p.y*v.sy+v.ty)) return GfxResult::invalid_argument;
        for (auto i:d.indices) if (i>=d.vertices.size()) return GfxResult::invalid_argument;
        for (auto i:d.masks) {
            if (i>=s.drawables.size()) return GfxResult::invalid_argument;
            work+=s.drawables[i].indices.size();
            if (work>64*MiB) return GfxResult::out_of_budget;
        }
    }
    return GfxResult::ok;
}

GfxResult build(ID3D11Device* device,graphics::TargetInfo info,View view,Frame& f,GpuProbe& probe,
               const AtlasResidency::State* cached) {
    auto created=[&](auto action) {
        if (!probe.next()) return GfxResult::out_of_budget;
        HRESULT hr=action();
        if (SUCCEEDED(hr)) return GfxResult::ok;
        if (FAILED(device->GetDeviceRemovedReason())) return GfxResult::device_lost;
        return hr==E_OUTOFMEMORY?GfxResult::out_of_budget:GfxResult::failed;
    };
#define MAKE(action) do { auto r=created([&] {return (action);}); if(r!=GfxResult::ok)return r; } while(false)
    f.images.resize(f.atlases->images.size());
    D3D11_TEXTURE2D_DESC tex{};
    tex.MipLevels=tex.ArraySize=tex.SampleDesc.Count=1; tex.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    tex.Usage=D3D11_USAGE_IMMUTABLE; tex.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    if(cached) f.images=cached->images;
    for (std::size_t i=0;i<f.images.size();++i) {
        const auto& image=f.atlases->images[i]; tex.Width=image.width; tex.Height=image.height;
        D3D11_SUBRESOURCE_DATA data{}; data.pSysMem=image.rgba.data(); data.SysMemPitch=image.width*4;
        if(!cached){
            MAKE(device->CreateTexture2D(&tex,&data,&f.images[i].texture));
            MAKE(device->CreateShaderResourceView(f.images[i].texture.Get(),nullptr,&f.images[i].srv));
        }
        probe.resident_bytes+=image.rgba.size();
    }
    bool masked=false;
    for (const auto& d:f.snapshot->drawables) masked|=!d.masks.empty();
    if (masked) {
        tex.Width=info.width; tex.Height=info.height; tex.Usage=D3D11_USAGE_DEFAULT;
        tex.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        MAKE(device->CreateTexture2D(&tex,nullptr,&f.mask.texture));
        MAKE(device->CreateShaderResourceView(f.mask.texture.Get(),nullptr,&f.mask.srv));
        MAKE(device->CreateRenderTargetView(f.mask.texture.Get(),nullptr,&f.mask_rtv));
        probe.resident_bytes+=std::size_t(info.width)*info.height*4;
    }
    std::vector<PackedVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::size_t vc=0,ic=0;
    for(const auto& d:f.snapshot->drawables){vc+=d.vertices.size();ic+=d.indices.size();}
    vertices.reserve(vc);indices.reserve(ic);f.ranges.reserve(f.snapshot->drawables.size());
    for(const auto& d:f.snapshot->drawables){
        auto base=static_cast<UINT>(vertices.size()),start=static_cast<UINT>(indices.size());
        for(const auto& p:d.vertices)vertices.push_back({p.x,p.y,p.u,p.v,d.multiply,d.screen,
            d.opacity,d.masks.empty()?0.0f:d.inverted_mask?2.0f:1.0f});
        for(auto i:d.indices)indices.push_back(base+i);
        f.ranges.push_back({static_cast<UINT>(indices.size()-start),start});
    }
    probe.staging_bytes=vertices.size()*sizeof(PackedVertex)+indices.size()*4;
    auto buffer=[&](UINT flag,UINT size,const void* data,ComPtr<ID3D11Buffer>& out) {
        D3D11_BUFFER_DESC desc{};desc.ByteWidth=size;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=flag;
        D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=data;
        return created([&]{return device->CreateBuffer(&desc,&initial,&out);});
    };
    if(!vertices.empty()){auto r=buffer(D3D11_BIND_VERTEX_BUFFER,static_cast<UINT>(vertices.size()*sizeof(PackedVertex)),vertices.data(),f.vertices);if(r!=GfxResult::ok)return r;}
    if(!indices.empty()){auto r=buffer(D3D11_BIND_INDEX_BUFFER,static_cast<UINT>(indices.size()*4),indices.data(),f.indices);if(r!=GfxResult::ok)return r;}
    auto r=buffer(D3D11_BIND_CONSTANT_BUFFER,sizeof(View),&view,f.constants);if(r!=GfxResult::ok)return r;
    ComPtr<ID3DBlob> vs,ps,mask,errors;
    MAKE(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"VS","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vs,&errors));
    MAKE(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"PS","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&ps,&errors));
    MAKE(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"MaskPS","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&mask,&errors));
    MAKE(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&f.vs));
    MAKE(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&f.color_ps));
    MAKE(device->CreatePixelShader(mask->GetBufferPointer(),mask->GetBufferSize(),nullptr,&f.mask_ps));
    const D3D11_INPUT_ELEMENT_DESC layout[]={
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",3,DXGI_FORMAT_R32G32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0}};
    MAKE(device->CreateInputLayout(layout,5,vs->GetBufferPointer(),vs->GetBufferSize(),&f.layout));
    D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=0;
    MAKE(device->CreateSamplerState(&sampler,&f.linear));
    sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;MAKE(device->CreateSamplerState(&sampler,&f.point));
    D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.DepthClipEnable=TRUE;
    raster.FrontCounterClockwise=TRUE;
    for(int i=0;i<2;++i){raster.CullMode=i?D3D11_CULL_NONE:D3D11_CULL_BACK;MAKE(device->CreateRasterizerState(&raster,&f.raster[i]));}
    for(int i=0;i<3;++i){
        D3D11_BLEND_DESC blend{};auto& b=blend.RenderTarget[0];b.BlendEnable=TRUE;
        b.SrcBlend=i==2?D3D11_BLEND_DEST_COLOR:D3D11_BLEND_ONE;
        b.DestBlend=i==1?D3D11_BLEND_ONE:D3D11_BLEND_INV_SRC_ALPHA;
        b.BlendOp=b.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        b.SrcBlendAlpha=i?D3D11_BLEND_ZERO:D3D11_BLEND_ONE;
        b.DestBlendAlpha=i?D3D11_BLEND_ONE:D3D11_BLEND_INV_SRC_ALPHA;
        b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        MAKE(device->CreateBlendState(&blend,&f.blend[i]));
    }
    D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=FALSE;depth.StencilEnable=FALSE;
    MAKE(device->CreateDepthStencilState(&depth,&f.depth));
    f.order.resize(f.snapshot->drawables.size());std::iota(f.order.begin(),f.order.end(),0);
    std::stable_sort(f.order.begin(),f.order.end(),[&](auto a,auto b){
        return f.snapshot->drawables[a].render_order<f.snapshot->drawables[b].render_order;});
#undef MAKE
    return GfxResult::ok;
}

// Extra shader stages and predication are untouched by the older host pass;
// isolate them here so hostile host pipeline state cannot suppress this draw.
struct ExtraScope {
    ID3D11DeviceContext* c;
    ComPtr<ID3D11GeometryShader> gs;ComPtr<ID3D11HullShader> hs;ComPtr<ID3D11DomainShader> ds;
    ComPtr<ID3D11Predicate> predicate;BOOL predicate_value=FALSE;
    std::array<ID3D11ClassInstance*,256> gc{},hc{},dc{};UINT gn=256,hn=256,dn=256;
    explicit ExtraScope(ID3D11DeviceContext* context):c(context){
        c->GSGetShader(&gs,gc.data(),&gn);c->HSGetShader(&hs,hc.data(),&hn);c->DSGetShader(&ds,dc.data(),&dn);
        c->GetPredication(&predicate,&predicate_value);c->SetPredication(nullptr,FALSE);
        c->GSSetShader(nullptr,nullptr,0);c->HSSetShader(nullptr,nullptr,0);c->DSSetShader(nullptr,nullptr,0);
    }
    ~ExtraScope(){
        c->GSSetShader(gs.Get(),gc.data(),gn);c->HSSetShader(hs.Get(),hc.data(),hn);c->DSSetShader(ds.Get(),dc.data(),dn);
        c->SetPredication(predicate.Get(),predicate_value);
        for(UINT i=0;i<gn;++i)if(gc[i])gc[i]->Release();
        for(UINT i=0;i<hn;++i)if(hc[i])hc[i]->Release();
        for(UINT i=0;i<dn;++i)if(dc[i])dc[i]->Release();
    }
};
}

graphics::Result render_offscreen_moc3(graphics::d3d11::HostDevice& host,
    ID3D11RenderTargetView* target,graphics::TargetInfo info,
    std::shared_ptr<const Snapshot> snapshot,std::shared_ptr<const AtlasSet> atlases,
    View view,graphics::d3d11::QueryRetirement& query,GpuProbe* probe,AtlasResidency* residency){
    if(!host.on_owner_thread())return GfxResult::wrong_thread;
    auto operation=host.enter_context();if(operation.result()!=GfxResult::ok)return operation.result();
    auto valid=host.validate(target,info);if(valid!=GfxResult::ok)return valid;
    if(!snapshot||!atlases)return GfxResult::invalid_argument;
    valid=validate(*snapshot,*atlases,view,info);if(valid!=GfxResult::ok)return valid;
    try{
        auto f=std::make_shared<Frame>();
        if(host.ownership()){const auto r=host.ownership()->retain(f->gpu_owner);if(r!=GfxResult::ok)return r;}
        f->snapshot=std::move(snapshot);f->atlases=std::move(atlases);
        GpuProbe local;auto& p=probe?*probe:local;
        const auto cached=residency?residency->state:nullptr;
        // A different package/device must use a separate cache so old+candidate
        // atlas residency cannot silently double the per-pass budget.
        if(cached&&(cached->ownership!=host.ownership()||cached->device.Get()!=host.device()||cached->generation!=host.generation()||
                    cached->atlases!=f->atlases))return GfxResult::invalid_argument;
        auto ready=build(host.device(),info,view,*f,p,cached.get());if(ready!=GfxResult::ok)return ready;
        if(residency&&!cached){
            auto state=std::make_shared<AtlasResidency::State>();
            if(host.ownership()){const auto r=host.ownership()->retain(state->gpu_owner);if(r!=GfxResult::ok)return r;}
            state->ownership=host.ownership();state->device=host.device();
            state->generation=host.generation();state->atlases=f->atlases;state->images=f->images;
            residency->state=std::move(state);
        }
        auto cpu=std::make_shared<CpuPins>(CpuPins{f->snapshot,f->atlases});
        ready=query.prepare_split(host.device(),host.context(),target,host.generation(),cpu,f);
        if(ready!=GfxResult::ok)return ready;
        {
            graphics::d3d11::StateScope restore(host.context(),host.ownership());
            if(restore.result()!=GfxResult::ok){query.cancel_unsubmitted();return restore.result();}
            ExtraScope extra(host.context());
            auto* c=host.context();constexpr FLOAT zero[4]{};constexpr FLOAT factor[4]{1,1,1,1};
            c->ClearRenderTargetView(target,zero);
            c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->IASetInputLayout(f->layout.Get());
            ID3D11Buffer* vb=f->vertices.Get();constexpr UINT stride=sizeof(PackedVertex),offset=0;
            c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(f->indices.Get(),DXGI_FORMAT_R32_UINT,0);
            c->VSSetShader(f->vs.Get(),nullptr,0);ID3D11Buffer* cb=f->constants.Get();c->VSSetConstantBuffers(0,1,&cb);
            ID3D11SamplerState* samplers[]{f->linear.Get(),f->point.Get()};c->PSSetSamplers(0,2,samplers);
            D3D11_VIEWPORT viewport{0,0,float(info.width),float(info.height),0,1};c->RSSetViewports(1,&viewport);
            c->OMSetDepthStencilState(f->depth.Get(),0);
            auto draw=[&](std::size_t i){
                const auto& d=f->snapshot->drawables[i];const auto& range=f->ranges[i];
                ID3D11ShaderResourceView* srv=f->images[std::size_t(d.texture_index)].srv.Get();
                c->PSSetShaderResources(0,1,&srv);c->RSSetState(f->raster[d.double_sided?1:0].Get());
                if(range.count)c->DrawIndexed(range.count,range.start,0);
            };
            for(auto i:f->order){
                const auto& d=f->snapshot->drawables[i];if(!d.visible||d.opacity==0)continue;
                if(!d.masks.empty()){
                    ID3D11ShaderResourceView* none=nullptr;c->PSSetShaderResources(1,1,&none);
                    ID3D11RenderTargetView* mask=f->mask_rtv.Get();c->OMSetRenderTargets(1,&mask,nullptr);
                    c->ClearRenderTargetView(mask,zero);c->OMSetBlendState(f->blend[0].Get(),factor,0xffffffffu);
                    c->PSSetShader(f->mask_ps.Get(),nullptr,0);
                    for(auto m:d.masks)draw(m);
                }
                c->OMSetRenderTargets(1,&target,nullptr);
                ID3D11ShaderResourceView* mask=d.masks.empty()?nullptr:f->mask.srv.Get();
                c->PSSetShaderResources(1,1,&mask);c->PSSetShader(f->color_ps.Get(),nullptr,0);
                c->OMSetBlendState(f->blend[std::size_t(d.blend_mode)].Get(),factor,0xffffffffu);draw(i);
            }
        }
        if(FAILED(host.device()->GetDeviceRemovedReason())){query.mark_lost();return GfxResult::device_lost;}
        return query.end();
    }catch(const std::bad_alloc&){return GfxResult::out_of_budget;}
}
}
#endif
