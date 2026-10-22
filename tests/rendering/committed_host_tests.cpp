#include "committed_test_support.hpp"
#include "../../src/rendering/public_d3d11.hpp"
#include "../../src/rendering/d3d11_scene3d.hpp"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <d3d11sdklayers.h>
#include <iostream>
#include <d3dcompiler.h>
using namespace ars;
using namespace ars::testing39;
using Microsoft::WRL::ComPtr;
namespace {
using Host=rendering::PublicD3D11Host;
using Hooks=rendering::PublicD3D11Hooks;
struct Ops:graphics::d3d11::QueryOps {
    bool loss=false,hold=false;
    HRESULT removed_reason(ID3D11Device *device) override{return loss?DXGI_ERROR_DEVICE_REMOVED:device->GetDeviceRemovedReason();}
    HRESULT get_data(ID3D11DeviceContext *context,ID3D11Query *query,BOOL *done) override{
        if(loss)return DXGI_ERROR_DEVICE_REMOVED;if(hold)return S_FALSE;
        return context->GetData(query,done,sizeof(*done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    }
    void flush(ID3D11DeviceContext *context) override{context->Flush();}
};
struct Gpu {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11RenderTargetView> view;
    ars_render_target target{};
    explicit Gpu(bool hardware){D3D_FEATURE_LEVEL level{};
        test::check(SUCCEEDED(D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,
            nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"native host debug device");
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=64;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET;
        test::check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&texture))&&SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&view)),"native host target");
        target.struct_size=sizeof(target);target.abi_version=ARS_ABI_VERSION;target.device_generation=1;
        target.d3d11_device=device.Get();target.d3d11_immediate_context=context.Get();target.d3d11_rtv=view.Get();
        target.width_px=target.height_px=64;target.dxgi_format=td.Format;target.sample_count=1;
        target.color_space=ARS_COLOR_SRGB;target.alpha_mode=ARS_ALPHA_PREMULTIPLIED;
        ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC description{};
        test::check(SUCCEEDED(device.As(&dxgi))&&SUCCEEDED(dxgi->GetAdapter(&adapter))&&SUCCEEDED(adapter->GetDesc(&description)),"adapter identity");
        std::wcout<<L"adapter="<<description.Description<<L" vendor="<<description.VendorId<<L'\n';
    }
    void report(const char *marker){context->ClearState();context->Flush();ComPtr<ID3D11Debug> debug;
        test::check(SUCCEEDED(device.As(&debug)),"required live-object debug interface");OutputDebugStringA(marker);
        test::check(SUCCEEDED(debug->ReportLiveDeviceObjects(static_cast<D3D11_RLDO_FLAGS>(D3D11_RLDO_DETAIL|D3D11_RLDO_IGNORE_INTERNAL))),"live-object report");}
    std::vector<unsigned char> pixels(){D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;test::check(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"rollback readback allocation");
        context->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        test::check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"rollback readback");
        std::vector<unsigned char> result(64*64*4);for(unsigned y=0;y<64;++y)
            std::memcpy(result.data()+y*64*4,static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch,64*4);
        context->Unmap(staging.Get(),0);return result;
    }
};
template<class F> ars_status until(F fn){ars_status result=ARS_E_BUSY;for(unsigned i=0;i<5000&&result==ARS_E_BUSY;++i){
    result=fn();if(result==ARS_E_BUSY)std::this_thread::sleep_for(std::chrono::milliseconds(1));}return result;}
std::shared_ptr<core::RuntimeLease> pin(Fixture &f){auto p=std::make_shared<core::RuntimeLease>();
    check(ARS_OK,core::global_router().acquire(f.runtime,core::LeaseKind::work,p.get()),"same actual runtime work custody");return p;}
void close(Fixture &f,Host &host){check(ARS_OK,ars_runtime_begin_close(f.runtime),"H close actual runtime");
    check(ARS_E_BUSY,ars_runtime_finish_close(f.runtime),"graphics pin blocks finish");
    check(ARS_OK,until([&]{return host.drain(true);}),"native R exact/surrender drain");
    // Fixture performs finish; begin_close is intentionally idempotent only
    // through Fixture's closed-state-aware cleanup below.
}
void oom_and_loss(const std::filesystem::path &path,Gpu &gpu,bool loss,bool two_d=false){
    Fixture f(path);check(ARS_OK,ars_character_set_mode(f.runtime,f.character,two_d?ARS_MODE_2D:ARS_MODE_3D),"fault representation");
    Ops ops;rendering::GpuCreateProbe creates;Hooks hooks{&ops,&creates};Host host(&hooks);
    auto work=pin(f);auto render=[&]{return host.render(core::global_router(),f.runtime,gpu.target,work);};
    check(ARS_OK,until(render),"fault baseline actual producer render");
    const auto original=*host.accepted();const auto original_pixels=gpu.pixels();const auto attempts=creates.attempts;
    test::check(attempts>2,"observed actual GPU creation points");
    if(!loss){
        for(std::size_t i=1;i<=attempts;++i){creates.attempts=0;creates.fail_at=i;
            check(ARS_E_OUT_OF_BUDGET,render(),"OOM each observed creation retains accepted frame");
            test::check(host.accepted()->target_epoch==original.target_epoch&&
                host.accepted()->characters[0].pose==original.characters[0].pose,"OOM preserves accepted full identity");
            test::check(gpu.pixels()==original_pixels,"every failed GPU allocation preserves destination pixels");}
        creates.fail_at=0;creates.attempts=0;check(ARS_OK,until(render),"healthy retry after every allocation failure");
        std::printf("PASS actual runtime OOM all%zu GPU creation sites\n",attempts);
    }else{
        ops.hold=true;check(ARS_E_BUSY,render(),"hold actual query");
        ops.loss=true;check(ARS_E_DEVICE_LOST,render(),"injected query removal, no OS reset");
        test::check(host.accepted()->target_epoch==original.target_epoch,"loss preserves accepted identity");
        test::check(gpu.pixels()==original_pixels,"injected query loss preserved destination pixels");
    }
    work.reset();close(f,host);
    test::check(!host.active(),"all registered GPU refs and original snapshot pins retired");
    if(loss)std::puts("PASS actual runtime injected loss/unknown query/ordered GPU then CPU surrender");
}
struct Rendezvous {
    std::mutex mutex;std::condition_variable cv;bool entered=false,released=false;
    Hooks::Fence selected=Hooks::Fence::draw_ready;
    static void observe(void *p,Hooks::Fence phase) noexcept{auto &r=*static_cast<Rendezvous*>(p);if(phase!=r.selected)return;
        std::unique_lock lock(r.mutex);r.entered=true;r.cv.notify_all();r.cv.wait(lock,[&]{return r.released;});}
    void wait(){std::unique_lock lock(mutex);test::check(cv.wait_for(lock,std::chrono::seconds(10),[&]{return entered;}),"bounded R fence rendezvous");}
    void release(){std::lock_guard lock(mutex);released=true;cv.notify_all();}
};
void races(const std::filesystem::path &path,Gpu &gpu,unsigned scenario){
    Fixture f(path);Rendezvous meeting;Hooks hooks;
    hooks.observe=Rendezvous::observe;hooks.user=&meeting;
    if(scenario==3)meeting.selected=Hooks::Fence::copy_ready;
    std::mutex end_mutex;std::condition_variable end_cv;bool end=false,render_done=false;
    ars_status result=ARS_E_INTERNAL;std::uint64_t accepted_pose=0;
    std::thread render([&]{Host host(&hooks);auto work=pin(f);
        result=until([&]{return host.render(core::global_router(),f.runtime,gpu.target,work);});
        if(host.accepted())accepted_pose=host.accepted()->characters[0].pose;
        {std::unique_lock lock(end_mutex);render_done=true;end_cv.notify_all();end_cv.wait(lock,[&]{return end;});}
        work.reset();check(ARS_OK,until([&]{return host.drain(true);}),"race R drain");
    });
    meeting.wait();
    if(scenario==0){check(ARS_OK,ars_character_set_mode(f.runtime,f.character,ARS_MODE_3D),"H changes mode before publication");}
    else if(scenario==1){check(ARS_OK,ars_character_destroy(f.runtime,f.character),"H destroys before publication");}
    else if(scenario==2){check(ARS_OK,ars_runtime_begin_close(f.runtime),"H closes before publication");}
    else {f.submit({channel("mouth.open",{.75f,0,0,0})});}
    meeting.release();
    {std::unique_lock lock(end_mutex);test::check(end_cv.wait_for(lock,std::chrono::seconds(10),[&]{return render_done;}),"bounded render transaction result");}
    // Mutation scenarios must fail while runtime is still open; close cannot
    // mask a missing configuration/character publication guard.
    if(scenario<2)check(ARS_E_STALE_HANDLE,result,"mutation alone refuses stale draw candidate");
    if(scenario==2)check(ARS_E_CLOSED,result,"close alone refuses late draw candidate");
    if(scenario!=2)check(ARS_OK,ars_runtime_begin_close(f.runtime),"H close after mutation");
    {std::lock_guard lock(end_mutex);end=true;end_cv.notify_all();}render.join();
    if(scenario==3){check(ARS_OK,result,"already authorized copy completes with historical identity");
        test::check(accepted_pose==1,"new S pose cannot relabel already authorized copy");}
    std::printf("PASS actual H/S/R fence race%u\n",scenario);
}
void pipeline(const std::filesystem::path &path,Gpu &gpu){
    Fixture f(path);Host host;auto work=pin(f);
    auto render=[&]{return host.render(core::global_router(),f.runtime,gpu.target,work);};
    check(ARS_OK,until(render),"pipeline clean-context control");const auto baseline=gpu.pixels();
    constexpr char shader[]=R"(
struct V {float4 pos:SV_Position;float2 uv:TEXCOORD0;float opacity:TEXCOORD1;};
[maxvertexcount(3)] void GS(triangle V input[3],inout TriangleStream<V> stream) {}
)";
    ComPtr<ID3DBlob> code,errors;test::check(SUCCEEDED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,
        "GS","gs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors)),"host drop-GS compilation");
    ComPtr<ID3D11GeometryShader> gs;test::check(SUCCEEDED(gpu.device->CreateGeometryShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&gs)),"host GS sentinel");
    gpu.context->GSSetShader(gs.Get(),nullptr,0);
    check(ARS_OK,until(render),"render with host GS must isolate output");
    test::check(gpu.pixels()==baseline,"host GS cannot suppress SDK accepted pixels");
    ComPtr<ID3D11GeometryShader> after;gpu.context->GSGetShader(&after,nullptr,nullptr);
    test::check(after.Get()==gs.Get(),"host GS identity restored");gpu.context->GSSetShader(nullptr,nullptr,0);after.Reset();
    D3D11_QUERY_DESC desc{D3D11_QUERY_OCCLUSION_PREDICATE,0};ComPtr<ID3D11Predicate> predicate;
    test::check(SUCCEEDED(gpu.device->CreatePredicate(&desc,&predicate)),"host predicate sentinel");
    gpu.context->Begin(predicate.Get());gpu.context->End(predicate.Get());gpu.context->Flush();BOOL done=TRUE;HRESULT ready=S_FALSE;
    for(unsigned i=0;i<5000&&ready==S_FALSE;++i){ready=gpu.context->GetData(predicate.Get(),&done,sizeof(done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(ready==S_FALSE)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    test::check(ready==S_OK&&!done,"known false host predicate");
    for(const BOOL value:{FALSE,TRUE}){
        constexpr FLOAT sentinel[4]{0,.25f,.75f,1};
        gpu.context->SetPredication(nullptr,FALSE);gpu.context->ClearRenderTargetView(gpu.view.Get(),sentinel);
        test::check(gpu.pixels()!=baseline,"predicate presentation oracle starts from distinct destination pixels");
        gpu.context->SetPredication(predicate.Get(),value);check(ARS_OK,until(render),"predicated host still allows valid SDK query completion");
        ComPtr<ID3D11Predicate> actual;BOOL actual_value=FALSE;gpu.context->GetPredication(&actual,&actual_value);
        test::check(actual.Get()==predicate.Get()&&actual_value==value,"host predicate identity/value restored");
        gpu.context->SetPredication(nullptr,FALSE);test::check(gpu.pixels()==baseline,"host predicate cannot suppress SDK draw/copy/mips");
    }
    work.reset();close(f,host);std::puts("PASS hostile host GS/predicate exact pixels and state restoration");
}
}
int main(int argc,char **argv){if(argc<2||argc>3)return 2;
    const bool pipeline_only=argc==3&&(std::string_view(argv[2])=="--pipeline"||std::string_view(argv[2])=="--pipeline-hardware");
    const bool hardware=argc==3&&std::string_view(argv[2])!="--pipeline";
    const std::filesystem::path path=std::filesystem::path(argv[1])/"committed-euler.arschar";
    Gpu gpu(hardware);ComPtr<ID3D11InfoQueue> messages;test::check(SUCCEEDED(gpu.device.As(&messages)),"debug queue");
    if(pipeline_only){pipeline(path,gpu);return 0;}
    gpu.report("ARS_LIVE_BASELINE\n");messages->ClearStoredMessages();
    oom_and_loss(path,gpu,false);oom_and_loss(path,gpu,true);
    oom_and_loss(path,gpu,false,true);
    for(const unsigned scenario:{0u,1u,3u})races(path,gpu,scenario);
    for(unsigned iteration=0;iteration<100;++iteration)races(path,gpu,2);
    std::puts("PASS actual runtime100 H-close/R-publication races with exact CLOSED result and R drain");
    pipeline(path,gpu);
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size=0;messages->GetMessage(i,nullptr,&size);
        std::vector<unsigned char> bytes(size);auto *m=reinterpret_cast<D3D11_MESSAGE*>(bytes.data());
        test::check(SUCCEEDED(messages->GetMessage(i,m,&size)),"read debug message");
        if(m->Severity<=D3D11_MESSAGE_SEVERITY_WARNING)std::fprintf(stderr,"D3D %s\n",m->pDescription);
        test::check(m->Severity>D3D11_MESSAGE_SEVERITY_WARNING,"no debug warnings/errors");}
    gpu.report("ARS_LIVE_FINAL\n");
}
