#include "moc3/d3d11_renderer.hpp"
#include "rendering/d3d11_dual_mode_host.hpp"
#include <d3dcompiler.h>
#include <d3d11sdklayers.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <thread>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
using ars::graphics::TargetInfo;
using ars::graphics::d3d11::HostDevice;
using ars::graphics::d3d11::QueryRetirement;
using ars::moc3::AtlasSet;
using ars::moc3::Snapshot;
using ars::moc3::Drawable;
using ars::moc3::GpuProbe;
using ars::moc3::render_offscreen_moc3;
using ars::rendering::D3D11DualModeHost;
namespace {
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"F2_FAIL %s\n",message);std::exit(1);}}
struct Target{ComPtr<ID3D11Texture2D> texture,staging;ComPtr<ID3D11RenderTargetView> view;UINT w=0,h=0;};
Target target(ID3D11Device* d,UINT w,UINT h){
    Target t;t.w=w;t.h=h;D3D11_TEXTURE2D_DESC x{};x.Width=w;x.Height=h;
    x.MipLevels=x.ArraySize=x.SampleDesc.Count=1;x.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    x.Usage=D3D11_USAGE_DEFAULT;x.BindFlags=D3D11_BIND_RENDER_TARGET;
    check(SUCCEEDED(d->CreateTexture2D(&x,nullptr,&t.texture)),"target");
    check(SUCCEEDED(d->CreateRenderTargetView(t.texture.Get(),nullptr,&t.view)),"view");
    x.Usage=D3D11_USAGE_STAGING;x.BindFlags=0;x.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(SUCCEEDED(d->CreateTexture2D(&x,nullptr,&t.staging)),"staging");return t;
}
std::array<int,4> pixel(ID3D11DeviceContext* c,const Target& t,UINT x=0,UINT y=0){
    c->CopyResource(t.staging.Get(),t.texture.Get());D3D11_MAPPED_SUBRESOURCE m{};
    check(SUCCEEDED(c->Map(t.staging.Get(),0,D3D11_MAP_READ,0,&m)),"map");
    auto* p=static_cast<unsigned char*>(m.pData)+y*m.RowPitch+x*4;
    std::array<int,4> out{p[0],p[1],p[2],p[3]};c->Unmap(t.staging.Get(),0);return out;
}
void expect(std::array<int,4> p,std::array<int,4> ref,const char* label){
    for(int i=0;i<4;++i)if(std::abs(p[i]-ref[i])>3){
        std::fprintf(stderr,"F2_PIXEL %s actual=%d,%d,%d,%d expected=%d,%d,%d,%d\n",label,
            p[0],p[1],p[2],p[3],ref[0],ref[1],ref[2],ref[3]);check(false,label);}
}
Drawable quad(int atlas=0,float left=-1,float right=1){
    Drawable d;d.id="generated";d.texture_index=atlas;d.double_sided=true;d.screen={0,0,0,1};
    d.vertices={{left,-1,0,0},{right,-1,1,0},{left,1,0,1},{right,1,1,1}};
    d.indices={0,1,2,2,1,3};return d;
}
std::shared_ptr<Snapshot> scene(std::initializer_list<Drawable> draws){
    auto s=std::make_shared<Snapshot>();s->generation=1;s->drawables=draws;return s;
}
std::shared_ptr<AtlasSet> solid(std::initializer_list<std::array<unsigned char,4>> colors){
    auto a=std::make_shared<AtlasSet>();for(auto color:colors)a->images.push_back({1,1,{color.begin(),color.end()}});return a;
}
void retire(QueryRetirement& q){
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);Result r=Result::busy;
    while(r==Result::busy&&std::chrono::steady_clock::now()<end){r=q.poll(71);std::this_thread::yield();}
    check(r==Result::ok&&q.complete()&&!q.holds_pin(),"exact query retirement");
}
void draw(HostDevice& h,const Target& t,std::shared_ptr<const Snapshot> s,
          std::shared_ptr<const AtlasSet> a,GpuProbe* p=nullptr){
    QueryRetirement q;check(render_offscreen_moc3(h,t.view.Get(),{71,t.w,t.h},s,a,{},q,p)==Result::ok,"render");retire(q);
}
struct Gate:ars::graphics::d3d11::QueryOps{
    bool allow=false;
    HRESULT removed_reason(ID3D11Device* d)override{return d->GetDeviceRemovedReason();}
    HRESULT get_data(ID3D11DeviceContext* c,ID3D11Query* q,BOOL* done)override{
        return allow?c->GetData(q,done,sizeof(*done),D3D11_ASYNC_GETDATA_DONOTFLUSH):S_FALSE;}
    void flush(ID3D11DeviceContext* c)override{c->Flush();}
};
void commit(D3D11DualModeHost& h){
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);Result r=Result::busy;
    while(r==Result::busy&&std::chrono::steady_clock::now()<end){r=h.poll_commit();std::this_thread::yield();}
    check(r==Result::ok,"host publication query");
}
}
int main(int argc,char** argv){
    const bool hardware=argc==2&&std::string_view(argv[1])=="--hardware";
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    auto driver=hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP;
    auto hr=D3D11CreateDevice(nullptr,driver,nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
    bool debug=SUCCEEDED(hr);
    if(FAILED(hr))hr=D3D11CreateDevice(nullptr,driver,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
    if(FAILED(hr)&&hardware){std::puts("F2_HARDWARE_UNAVAILABLE");return 77;}
    check(SUCCEEDED(hr),"device");
    ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC desc{};
    check(SUCCEEDED(device.As(&dxgi))&&SUCCEEDED(dxgi->GetAdapter(&adapter))&&SUCCEEDED(adapter->GetDesc(&desc)),"adapter");
    char name[256]{};WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,name,sizeof(name),nullptr,nullptr);
    std::printf("F2_ADAPTER %s vendor=%04x device=%04x name=%s debug=%d\n",hardware?"HARDWARE":"WARP",desc.VendorId,desc.DeviceId,name,int(debug));
    if(hardware)check(desc.VendorId!=0x1414,"physical adapter");
    ComPtr<ID3D11InfoQueue> messages;if(debug)check(SUCCEEDED(device.As(&messages)),"debug queue");
    HostDevice host(device.Get(),context.Get(),71);auto t=target(device.Get(),4,4);
    auto a=solid({{255,0,0,255},{0,0,255,255}});
    auto red=quad(0),blue=quad(1);red.render_order=5;red.opacity=.5f;
    auto s=scene({red,blue});draw(host,t,s,a);expect(pixel(context.Get(),t),{128,0,127,255},"multi atlas and render order");
    red.render_order=0;blue.opacity=.5f;s=scene({red,blue});draw(host,t,s,a);
    expect(pixel(context.Get(),t),{64,0,128,192},"stable render-order tie and premultiplied alpha");
    auto uv=std::make_shared<AtlasSet>();uv->images.push_back({2,2,{255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255}});
    auto uv_target=target(device.Get(),2,2);draw(host,uv_target,scene({quad()}),uv);
    expect(pixel(context.Get(),uv_target,0,0),{255,0,0,255},"UV top left");expect(pixel(context.Get(),uv_target,1,0),{0,255,0,255},"UV top right");
    expect(pixel(context.Get(),uv_target,0,1),{0,0,255,255},"UV bottom left");expect(pixel(context.Get(),uv_target,1,1),{255,255,255,255},"UV bottom right");
    auto clamped=quad();for(auto& v:clamped.vertices){v.u=-2;v.v=3;}
    draw(host,t,scene({clamped}),uv);expect(pixel(context.Get(),t),{255,0,0,255},"UV clamp");
    auto edge=std::make_shared<AtlasSet>();edge->images.push_back({2,1,{255,0,0,255,0,0,255,0}});
    auto mid=quad();for(auto& v:mid.vertices){v.u=.5f;v.v=.5f;}mid.opacity=.5f;
    draw(host,t,scene({mid}),edge);expect(pixel(context.Get(),t),{32,0,32,64},"bilinear straight texture before premultiplication");
    auto colors=solid({{64,128,192,255},{128,64,32,128}});
    auto base=quad(0),top=quad(1);top.render_order=1;
    for(int mode=0;mode<3;++mode){top.blend_mode=mode;draw(host,t,scene({base,top}),colors);
        expect(pixel(context.Get(),t),mode==0?std::array<int,4>{96,96,112,255}:mode==1?std::array<int,4>{128,160,208,255}:std::array<int,4>{48,80,108,255},"enabled blend equation");}
    top.blend_mode=0;top.multiply={.5f,1,0,1};top.screen={0,.5f,1,1};
    draw(host,t,scene({top}),colors);expect(pixel(context.Get(),t),{32,80,128,128},"multiply and screen color");
    auto mask=quad(0,-1,0);mask.visible=false;mask.opacity=0;
    auto clip=quad(1);clip.masks={0};auto masked=scene({mask,clip});
    draw(host,t,masked,a);expect(pixel(context.Get(),t,0,1),{0,0,255,255},"mask left");expect(pixel(context.Get(),t,3,1),{0,0,0,0},"mask outside");
    masked->drawables[1].inverted_mask=true;draw(host,t,masked,a);
    expect(pixel(context.Get(),t,0,1),{0,0,0,0},"inverted mask inside");expect(pixel(context.Get(),t,3,1),{0,0,255,255},"inverted mask outside");
    auto union_a=solid({{255,255,255,128},{0,0,255,255}});
    mask=quad(0);mask.visible=false;mask.opacity=0;clip.masks={0,1};
    auto union_s=scene({mask,mask,clip});draw(host,t,union_s,union_a);
    expect(pixel(context.Get(),t),{0,0,192,192},"fractional mask union ignoring source visibility opacity");
    auto face=quad();face.double_sided=false;draw(host,t,scene({face}),a);expect(pixel(context.Get(),t),{255,0,0,255},"CCW front");
    for(std::size_t i=0;i<face.indices.size();i+=3)std::swap(face.indices[i],face.indices[i+1]);
    draw(host,t,scene({face}),a);expect(pixel(context.Get(),t),{0,0,0,0},"back face culled");
    face.double_sided=true;draw(host,t,scene({face}),a);expect(pixel(context.Get(),t),{255,0,0,255},"double sided");
    auto hidden=quad();hidden.visible=false;draw(host,t,scene({hidden}),a);expect(pixel(context.Get(),t),{0,0,0,0},"invisible clear");

    // Host slots used for mask and ordinary drawing must survive exactly.
    D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> old_texture;ComPtr<ID3D11ShaderResourceView> old_srv;
    check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&old_texture))&&SUCCEEDED(device->CreateShaderResourceView(old_texture.Get(),nullptr,&old_srv)),"host inputs");
    ID3D11ShaderResourceView* old_inputs[]{old_srv.Get(),old_srv.Get()};context->PSSetShaderResources(0,2,old_inputs);
    D3D11_VIEWPORT old_view{1,1,2,2,0,1};context->RSSetViewports(1,&old_view);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ComPtr<ID3DBlob> gs_code,errors;constexpr char gs_source[]="struct V{float4 p:SV_Position;};[maxvertexcount(3)]void GS(triangle V v[3],inout TriangleStream<V> s){}";
    check(SUCCEEDED(D3DCompile(gs_source,sizeof(gs_source)-1,nullptr,nullptr,nullptr,"GS","gs_4_0",0,0,&gs_code,&errors)),"host GS code");
    ComPtr<ID3D11GeometryShader> old_gs;check(SUCCEEDED(device->CreateGeometryShader(gs_code->GetBufferPointer(),gs_code->GetBufferSize(),nullptr,&old_gs)),"host GS");context->GSSetShader(old_gs.Get(),nullptr,0);
    D3D11_QUERY_DESC pred_desc{D3D11_QUERY_OCCLUSION_PREDICATE,0};ComPtr<ID3D11Predicate> predicate;
    check(SUCCEEDED(device->CreatePredicate(&pred_desc,&predicate)),"host predicate");context->Begin(predicate.Get());context->End(predicate.Get());context->Flush();
    BOOL pred_value=TRUE;while(context->GetData(predicate.Get(),&pred_value,sizeof(pred_value),0)==S_FALSE)std::this_thread::yield();
    context->SetPredication(predicate.Get(),TRUE);
    draw(host,t,union_s,union_a);expect(pixel(context.Get(),t),{0,0,192,192},"host GS and predicate isolated");
    ComPtr<ID3D11ShaderResourceView> in0,in1;context->PSGetShaderResources(0,1,&in0);context->PSGetShaderResources(1,1,&in1);
    D3D11_VIEWPORT restored{};UINT count=1;context->RSGetViewports(&count,&restored);D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);
    ComPtr<ID3D11GeometryShader> restored_gs;context->GSGetShader(&restored_gs,nullptr,nullptr);
    ComPtr<ID3D11Predicate> restored_pred;BOOL restored_value=FALSE;context->GetPredication(&restored_pred,&restored_value);
    check(in0.Get()==old_srv.Get()&&in1.Get()==old_srv.Get()&&restored.TopLeftX==1&&topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST&&restored_gs.Get()==old_gs.Get()&&restored_pred.Get()==predicate.Get()&&restored_value,"host state restored");
    context->ClearState();

    auto before=pixel(context.Get(),t);QueryRetirement reject;GpuProbe no_upload;
    auto invalid=scene({quad(9)});
    check(render_offscreen_moc3(host,t.view.Get(),{71,4,4},invalid,a,{},reject,&no_upload)==Result::invalid_argument&&no_upload.creations==0,"invalid atlas before uploads");
    invalid=scene({quad()});invalid->drawables[0].masks={99};
    check(render_offscreen_moc3(host,t.view.Get(),{71,4,4},invalid,a,{},reject)==Result::invalid_argument,"invalid mask");
    invalid=scene({quad()});invalid->drawables[0].indices[0]=99;
    check(render_offscreen_moc3(host,t.view.Get(),{71,4,4},invalid,a,{},reject)==Result::invalid_argument,"invalid stream");
    check(render_offscreen_moc3(host,t.view.Get(),{70,4,4},s,a,{},reject)==Result::invalid_argument,"stale device generation");
    Result thread_result=Result::ok;std::thread other([&]{QueryRetirement q;thread_result=render_offscreen_moc3(host,t.view.Get(),{71,4,4},s,a,{},q);});other.join();check(thread_result==Result::wrong_thread,"render owner");
    ComPtr<ID3D11Device> second_device;ComPtr<ID3D11DeviceContext> second_context;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&second_device,&level,&second_context)),"other device");
    auto other_target=target(second_device.Get(),4,4);
    check(render_offscreen_moc3(host,other_target.view.Get(),{71,4,4},s,a,{},reject)==Result::invalid_argument,"wrong device target");
    check(pixel(context.Get(),t)==before&&!reject.holds_pin(),"invalid work preserved target");

    Gate gate;QueryRetirement pinned(&gate);auto pinned_s=scene({quad()});auto pinned_a=solid({{255,0,0,255}});
    std::weak_ptr<const Snapshot> weak_s=pinned_s;std::weak_ptr<const AtlasSet> weak_a=pinned_a;
    check(render_offscreen_moc3(host,t.view.Get(),{71,4,4},pinned_s,pinned_a,{},pinned)==Result::ok,"pinned submission");
    pinned_s.reset();pinned_a.reset();check(!weak_s.expired()&&!weak_a.expired()&&pinned.poll(70)==Result::invalid_argument&&pinned.poll(71)==Result::busy,"pending owners and generation");
    gate.allow=true;retire(pinned);check(weak_s.expired()&&weak_a.expired(),"retired owners released");
    ars::moc3::AtlasResidency cache;auto cached_a=solid({{255,0,0,255}});
    auto cached_s=scene({quad()});GpuProbe cold,warm;
    QueryRetirement cold_query;
    check(render_offscreen_moc3(host,t.view.Get(),{71,4,4},cached_s,cached_a,{},cold_query,&cold,&cache)==Result::ok,"cold residency");
    retire(cold_query);auto cold_pixels=pixel(context.Get(),t);
    Gate cache_gate;QueryRetirement warm_query(&cache_gate);
    check(render_offscreen_moc3(host,t.view.Get(),{71,4,4},cached_s,cached_a,{},warm_query,&warm,&cache)==Result::ok&&
          warm.creations+2==cold.creations&&warm.resident_bytes==cold.resident_bytes,"warm atlas avoids GPU allocation");
    auto different=solid({{0,255,0,255}});QueryRetirement rejected_cache;GpuProbe cache_no_upload;
    check(render_offscreen_moc3(host,t.view.Get(),{71,4,4},cached_s,different,{},rejected_cache,&cache_no_upload,&cache)==Result::invalid_argument&&
          cache_no_upload.creations==0,"cache package mismatch rejects before upload");
    std::weak_ptr<const AtlasSet> cache_weak=cached_a;cached_a.reset();cache.state.reset();
    check(!cache_weak.expired()&&warm_query.poll(71)==Result::busy,"cache destruction retains pending frame pins");
    cache_gate.allow=true;retire(warm_query);
    check(cache_weak.expired()&&pixel(context.Get(),t)==cold_pixels,"cache retirement and pixel parity");

    Gate host_gate;D3D11DualModeHost transaction(device.Get(),context.Get(),71,&host_gate);host_gate.allow=true;
    auto pass=[&](HostDevice& h,ID3D11RenderTargetView* v,TargetInfo info,QueryRetirement& q){return render_offscreen_moc3(h,v,info,union_s,union_a,{},q);};
    check(transaction.request_moc(pass,1,4,4)==Result::ok,"host moc request");commit(transaction);
    auto old_epoch=transaction.target_epoch();auto* old_target=transaction.target();check(transaction.accepts(old_target,old_epoch),"published target");
    host_gate.allow=false;check(transaction.request_moc(pass,2,8,8)==Result::ok&&transaction.poll_commit()==Result::busy,"pending resize");
    check(transaction.target_epoch()==old_epoch&&transaction.target()==old_target,"uncompleted resize not published");
    host_gate.allow=true;commit(transaction);check(transaction.target_epoch()==old_epoch+1&&!transaction.accepts(old_target,old_epoch),"resize epoch retired");
    old_epoch=transaction.target_epoch();old_target=transaction.target();
    int invoked=0;
    auto count_pass=[&](HostDevice&,ID3D11RenderTargetView*,TargetInfo,QueryRetirement&){++invoked;return Result::ok;};
    check(transaction.request_moc(count_pass,3,8192,8192)==Result::invalid_argument&&invoked==0&&
          transaction.target()==old_target&&transaction.target_epoch()==old_epoch,"F2-R1 oversized target rejected before allocation or callback");
    check(transaction.request_moc(pass,3,4,4,true)==Result::out_of_budget&&transaction.target_epoch()==old_epoch,"target OOM rollback");
    GpuProbe full;auto measured=[&](HostDevice& h,ID3D11RenderTargetView* v,TargetInfo info,QueryRetirement& q){return render_offscreen_moc3(h,v,info,union_s,union_a,{},q,&full);};
    check(transaction.request_moc(measured,3,4,4)==Result::ok,"creation inventory");commit(transaction);
    old_epoch=transaction.target_epoch();old_target=transaction.target();
    for(int fault=0;fault<static_cast<int>(full.creations);++fault){
        GpuProbe probe;probe.fail_after=fault;
        auto fail=[&](HostDevice& h,ID3D11RenderTargetView* v,TargetInfo info,QueryRetirement& q){return render_offscreen_moc3(h,v,info,union_s,union_a,{},q,&probe);};
        check(transaction.request_moc(fail,4,4,4)==Result::out_of_budget&&transaction.target()==old_target&&transaction.target_epoch()==old_epoch,"all GPU creation checkpoint rollback");
    }
    host_gate.allow=false;check(transaction.request_moc(pass,4,4,4)==Result::ok,"close pending");
    check(transaction.begin_close()==Result::ok&&transaction.poll_close()==Result::busy,"close waits exact query");host_gate.allow=true;
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);Result closed=Result::busy;
    while(closed==Result::busy&&std::chrono::steady_clock::now()<end){closed=transaction.poll_close();std::this_thread::yield();}check(closed==Result::ok,"close drained");
    D3D11DualModeHost throwing(device.Get(),context.Get(),71);
    auto pin=std::make_shared<int>(1);std::weak_ptr<int> weak_pin=pin;
    auto bad_callback=[&](HostDevice& h,ID3D11RenderTargetView* v,TargetInfo info,QueryRetirement& q)->Result{
        check(q.begin(h.device(),h.context(),v,info.generation,pin)==Result::ok,"prepare throwing query");
        throw std::bad_alloc();
    };
    check(throwing.request_moc(bad_callback,1,4,4)==Result::out_of_budget&&throwing.lost()&&throwing.pending(),
          "F2-R3 callback exception retains query ownership");
    pin.reset();check(!weak_pin.expired(),"throwing callback pin quarantined");
    throwing.begin_close();check(throwing.detach_lost()==Result::ok&&weak_pin.expired(),"isolated callback fault teardown");
    D3D11DualModeHost null_pin_throwing(device.Get(),context.Get(),72);
    auto null_pin_callback=[](HostDevice& h,ID3D11RenderTargetView* v,TargetInfo info,QueryRetirement& q)->Result{
        check(q.begin(h.device(),h.context(),v,info.generation,{})==Result::ok,"prepare null-pin query");
        throw std::bad_alloc();
    };
    check(null_pin_throwing.request_moc(null_pin_callback,1,4,4)==Result::out_of_budget&&
          null_pin_throwing.lost()&&null_pin_throwing.pending(),"F2-R3 null-pin query quarantined");
    null_pin_throwing.begin_close();check(null_pin_throwing.detach_lost()==Result::ok,"null-pin fault teardown");
    for(UINT64 i=0;messages&&i<messages->GetNumStoredMessages();++i){
        SIZE_T size=0;messages->GetMessage(i,nullptr,&size);std::vector<unsigned char> data(size);auto* m=reinterpret_cast<D3D11_MESSAGE*>(data.data());
        check(SUCCEEDED(messages->GetMessage(i,m,&size)),"debug message");if(m->Severity<=D3D11_MESSAGE_SEVERITY_WARNING){std::fprintf(stderr,"F2_DEBUG %s\n",m->pDescription);check(false,"zero debug warnings/errors");}
    }
    std::puts("F2_GENERATED_PIXELS_AND_LIFECYCLE_PASS");
}
