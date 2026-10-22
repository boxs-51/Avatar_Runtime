#include "graphics/d3d11/context_ownership.hpp"
#include "graphics/d3d11/state_scope.hpp"
#include "graphics/d3d11/close_coordinator.hpp"
#include "moc3/packet_publication.hpp"
#include "moc3/lease_publication.hpp"
#include "../assets/asset_test_support.hpp"
#include <d3d11sdklayers.h>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
using namespace ars::graphics::d3d11;
using ars::graphics::Result;
using Host=ars::rendering::D3D11DualModeHost;
using Microsoft::WRL::ComPtr;
namespace {
unsigned assertions=0;
void check(bool value,const char *label){++assertions;if(!value){std::cerr<<"OWNERSHIP_FAIL "<<label<<'\n';std::exit(1);}}
template<class F> Result until(F run){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);Result r;
    do{r=run();if(r!=Result::busy)return r;std::this_thread::yield();}while(std::chrono::steady_clock::now()<end);return r;}
struct Ops:QueryOps {
    bool loss=false,warning=false;
    HRESULT create_query(ID3D11Device *d,const D3D11_QUERY_DESC *desc,ID3D11Query **q) override {
        if(warning){ComPtr<ID3D11InfoQueue> messages;check(SUCCEEDED(d->QueryInterface(IID_PPV_ARGS(&messages))),"adapter debug required");
            check(SUCCEEDED(messages->AddApplicationMessage(D3D11_MESSAGE_SEVERITY_WARNING,"alpha38 injected diagnostic")),"inject diagnostic");}
        return d->CreateQuery(desc,q);
    }
    std::function<void()> on_ready;
    HRESULT removed_reason(ID3D11Device *d) override{return loss?DXGI_ERROR_DEVICE_REMOVED:d->GetDeviceRemovedReason();}
    HRESULT get_data(ID3D11DeviceContext *c,ID3D11Query *q,BOOL *done) override{
        auto hr=c->GetData(q,done,sizeof(*done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(hr==S_OK&&*done&&on_ready){auto callback=std::move(on_ready);on_ready={};callback();}return hr;}
    void flush(ID3D11DeviceContext *c) override{c->Flush();}
};
auto snapshot(){auto s=std::make_shared<ars::moc3::Snapshot>();s->generation=2;
    ars::moc3::Drawable d;d.texture_index=0;d.double_sided=true;d.screen={0,0,0,1};
    d.vertices={{-1,-1,0,0},{1,-1,1,0},{0,1,.5f,1}};d.indices={0,1,2};s->drawables={d};return s;}
auto atlas(){auto a=std::make_shared<ars::moc3::AtlasSet>();a->images.push_back({1,1,{255,0,0,255}});return a;}
auto bytes(const std::ostringstream &out){auto s=out.str();return std::vector<std::uint8_t>(s.begin(),s.end());}
std::array<std::uint8_t,4> pixel(ContextOwnership &domain,Host &host){
    auto operation=domain.enter();check(operation.result()==Result::ok,"readback admitted");auto *texture=host.texture();check(texture!=nullptr,"leased target");
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);auto x=desc.Width/4,y=desc.Height/4;
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;
    check(SUCCEEDED(operation.device()->CreateTexture2D(&desc,nullptr,&staging)),"leased staging");operation.context()->CopyResource(staging.Get(),texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(operation.context()->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"leased map");
    const auto *p=static_cast<const std::uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4;std::array<std::uint8_t,4> result{p[0],p[1],p[2],p[3]};operation.context()->Unmap(staging.Get(),0);return result;
}
void dual_cases(ID3D11Device *device,ID3D11DeviceContext *context,const char *worker,const char *corpus){
    test::Host allocator;
    {
        ars::assets::Resource resource(test::Host::allocate,test::Host::release,&allocator);
        ars::validation::SchemaClient schema({std::filesystem::absolute(worker).string()});
        auto input=test::file(std::filesystem::path(corpus)/"positive-dual-full.arschar");std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
        check(ars::assets::validate_bundle(input,schema,resource,model)==ARS_OK&&model&&model->glb(),"actual validated dual bundle");
        auto scene2=std::make_shared<ars::rendering::PreparedScene2D>();check(ars::rendering::prepare_static_2d(ars::rendering::scene_input(*model),*scene2)==ARS_OK,"validated native2D");
        auto image=std::shared_ptr<const ars::assets::Image>(model,&model->image());
        auto scene3=std::allocate_shared<ars::rendering::PreparedScene3D>(std::pmr::polymorphic_allocator<ars::rendering::PreparedScene3D>(&resource),&resource);
        check(ars::rendering::prepare_static_3d(*model,*scene3)==ARS_OK,"validated GLB3D");
        ars::rendering::Camera3D camera{{1,0,0,0,0,-1,0,0,0,0,-1,0,-0.5,1,0.5,1}};
        ars::moc3::CoreConsumer core;const ars::moc3::ParameterInfo channel{"P",-1,0,1};check(core.open(std::span(&channel,1))==ARS_OK,"dual actual core");
        std::shared_ptr<const void> pin;check(core.retain_graphics(pin)==ARS_OK,"dual runtime pin");std::shared_ptr<ContextOwnership> domain;
        check(ContextOwnership::create(device,context,101,std::move(pin),domain)==Result::ok,"dual domain");
        std::unique_ptr<Host> host;check(Host::create_owned(domain,nullptr,host)==Result::ok,"dual owned host");
        check(host->request_2d(scene2,image,1,8,8)==Result::ok&&until([&]{return host->poll_commit();})==Result::ok,"owned 2D query");auto two=pixel(*domain,*host);
        check(host->request_3d(scene3,camera,2,8,8)==Result::ok&&until([&]{return host->poll_commit();})==Result::ok,"owned 3D query");auto three=pixel(*domain,*host);check(two!=three,"owned 2D/3D distinct pixels");
        for(std::uint64_t i=0;i<100;++i){auto edge=static_cast<std::uint32_t>(i%4<2?8:16);auto epoch=host->target_epoch();
            check((i%2==0?host->request_2d(scene2,image,i+3,edge,edge):host->request_3d(scene3,camera,i+3,edge,edge))==Result::ok,"owned switch/resize");
            check(until([&]{return host->poll_commit();})==Result::ok&&host->target_epoch()==epoch+1&&host->pose_generation()==i+3,"owned atomic epoch/pose");
            check(pixel(*domain,*host)==(i%2==0?two:three),"owned resized pixel parity");}
        auto epoch=host->target_epoch();check(host->request_2d(scene2,image,103,8,8,true)==Result::out_of_budget&&host->target_epoch()==epoch,"owned OOM retains epoch");
        check(pixel(*domain,*host)==three,"owned OOM retains pixel");
        check(core.close()==ARS_E_BUSY,"dual core cannot close before graphics");
        host->begin_close();check(until([&]{return host->poll_close();})==Result::ok,"dual close drain");HostTeardownReceipt receipt;
        check(domain->owners()==0&&domain->issue(receipt)==Result::ok&&host->acknowledge_teardown(receipt)==Result::ok,"dual ordered receipt");
        host.reset();domain.reset();check(core.close()==ARS_OK,"dual core closes after graphics");
    }
    check(allocator.live==0,"dual CPU allocator released");
}
void adapter_cases(bool hardware){
    using namespace ars::moc3;using namespace ars::moc3::admission;
    using Result=ars::graphics::Result;
    auto s=snapshot();auto a=atlas();PhysicsCheckpoint cp;cp.generation=1;
    cp.model=sha256({});cp.particles={{{0,0,0,0},{0,1,0,0}}};auto state=encode_checkpoint(cp);
    std::ostringstream encoded(std::ios::binary);encode_step(encoded,*s,*a,42,1,0,cp.model,sha256({}),state,131);auto packet=bytes(encoded);
    for(int scenario=0;scenario<5;++scenario){
        Ops ops;ops.warning=scenario==4;PacketPublication owner(hardware,&ops);check(owner.open()==ARS_OK,"packet open");
        check(owner.begin_step(1,42,0,0,2000,cp.model)==ARS_OK,"packet begin");
        check(owner.stage_step(packet)==Result::ok,"packet stage");
        if(scenario==0){check(until([&]{return owner.poll();})==Result::ok,"packet real query");
            auto pixels=owner.read_pixels();check(pixels.size()==512*512*4&&pixels[(256*512+256)*4]>200,"packet pixel");}
        if(scenario==1){ops.loss=true;check(owner.poll()==Result::device_lost,"packet injected loss");}
        if(scenario==2){ops.on_ready=[&]{check(owner.close()==Result::busy,"packet reentrant close deferred");};
            check(until([&]{return owner.poll();})==Result::failed,"packet late callback cancelled");}
        check(until([&]{return owner.close();})==(scenario==4?Result::failed:Result::ok),"packet ordered close");
        check(owner.close()==Result::ok&&owner.begin_step(1,42,0,0,2000,cp.model)==ARS_E_CLOSED,"packet closed admission");
        bool denied=false;try{owner.read_pixels();}catch(...){denied=true;}check(denied,"packet closed readback");
    }
    LeaseKey key{7,1,1,1,0,1,0,1,42,cp.model,sha256({})};auto leasecp=encode_lease_checkpoint(key,cp);
    auto leasepacket=encode_lease_candidate(key,*s,*a,leasecp,131);auto command=lease_key_bytes(key);command.insert(command.end(),{3,0,0,0,0,0,0,0});
    for(int scenario=0;scenario<5;++scenario){
        Ops ops;ops.warning=scenario==4;LeasePublication owner(hardware,&ops);check(owner.open(7)==ARS_OK,"lease open");
        check(owner.begin(command,2000)==ARS_OK&&owner.stage(leasepacket)==Result::ok,"lease stage");
        if(scenario==0){check(until([&]{return owner.poll();})==Result::ok,"lease real query");
            auto pixels=owner.read_pixels();check(pixels.size()==512*512*4&&pixels[(256*512+256)*4]>200,"lease pixel");}
        if(scenario==1){ops.loss=true;check(owner.poll()==Result::device_lost,"lease injected loss");}
        if(scenario==2){ops.on_ready=[&]{check(owner.close()==Result::busy,"lease reentrant close deferred");};
            check(until([&]{return owner.poll();})==Result::failed,"lease late callback cancelled");}
        check(until([&]{return owner.close();})==(scenario==4?Result::failed:Result::ok),"lease ordered close");
        check(owner.close()==Result::ok&&owner.begin(command,2000)==ARS_E_CLOSED,"lease closed admission");
        bool denied=false;try{owner.read_pixels();}catch(...){denied=true;}check(denied,"lease closed readback");
    }
}
void scope_identity_case(ID3D11Device *device,ID3D11DeviceContext *context){
    ComPtr<ID3D11Device> other;ComPtr<ID3D11DeviceContext> other_context;D3D_FEATURE_LEVEL level{};
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&other,&level,&other_context)),"scope other device");
    std::shared_ptr<ContextOwnership> a,b;std::unique_ptr<Host> ah,bh;
    check(ContextOwnership::create(device,context,201,std::make_shared<int>(1),a)==Result::ok&&
          ContextOwnership::create(other.Get(),other_context.Get(),202,std::make_shared<int>(2),b)==Result::ok,"two scope domains");
    check(Host::create_owned(a,nullptr,ah)==Result::ok&&Host::create_owned(b,nullptr,bh)==Result::ok,"two scope hosts");
    {
        auto operation=a->enter();StateScope invalid(other_context.Get(),a);
        check(invalid.result()==Result::invalid_argument&&b->users()==0,"cross-domain restore rejected before capture");
        auto b_operation=b->enter();StateScope matching(other_context.Get(),b);check(matching.result()==Result::ok,"matching B restore admitted");
        bh->begin_close();HostTeardownReceipt early;
        check(bh->poll_close()==Result::busy&&b->issue(early)==Result::busy,"matching B scope blocks close and receipt");
    }
    check(bh->poll_close()==Result::ok,"B close after restore");HostTeardownReceipt br;
    check(b->issue(br)==Result::ok&&bh->acknowledge_teardown(br)==Result::ok,"B receipt after scope");
    ah->begin_close();check(ah->poll_close()==Result::ok,"A close");HostTeardownReceipt ar;
    check(a->issue(ar)==Result::ok&&ah->acknowledge_teardown(ar)==Result::ok,"A receipt");
}
void domain_case(ID3D11Device *device,ID3D11DeviceContext *context,bool loss,std::uint64_t generation){
    ars_runtime_desc desc{};desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;ars_runtime_h runtime{};
    check(ars_runtime_create(&desc,&runtime)==ARS_OK,"public runtime create");
    auto runtime_pin=std::make_shared<ars::core::RuntimeLease>();
    check(ars::core::global_router().acquire(runtime,ars::core::LeaseKind::work,runtime_pin.get())==ARS_OK,"actual public runtime pin");
    std::weak_ptr<ars::core::RuntimeLease> runtime_witness=runtime_pin;
    std::shared_ptr<ContextOwnership> domain,duplicate;
    check(ContextOwnership::create(device,context,generation,runtime_pin,domain)==Result::ok,"canonical domain");runtime_pin.reset();
    check(ContextOwnership::create(device,context,generation,std::make_shared<int>(1),duplicate)==Result::busy,"duplicate domain rejected");
    Ops ops;std::unique_ptr<Host> host;check(Host::create_owned(domain,&ops,host)==Result::ok,"mandatory owned factory");
    {
        std::vector<ContextOwnership::Participant> participants;participants.reserve(ContextOwnership::capacity);
        for(std::size_t i=domain->owners();i<ContextOwnership::capacity;++i){ContextOwnership::Participant p;check(domain->retain(p)==Result::ok,"bounded owner admission");participants.push_back(std::move(p));}
        ContextOwnership::Participant overflow;check(domain->retain(overflow)==Result::out_of_budget,"owner capacity denied");
    }
    {
        auto operation=domain->enter();ComPtr<ID3D11DeviceContext> deferred;
        check(SUCCEEDED(device->CreateDeferredContext(0,&deferred)),"deferred context fixture");
        std::shared_ptr<ContextOwnership> invalid;
        check(ContextOwnership::create(device,deferred.Get(),generation,std::make_shared<int>(1),invalid)==Result::invalid_argument,"deferred domain rejected");
        HostDevice bad(device,deferred.Get(),generation,domain);check(bad.ownership_status()==Result::invalid_argument,"foreign context enrollment denied");
        StateScope wrong_scope(deferred.Get(),domain);check(wrong_scope.result()==Result::invalid_argument,"foreign restore context denied");
        HostDevice stale_backend(device,context,generation+1,domain);check(stale_backend.ownership_status()==Result::invalid_argument,"wrong generation enrollment denied");
        HostDevice backend(device,context,generation,domain);check(backend.ownership_status()==Result::ok,"valid backend");
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=8;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11RenderTargetView> target;
        check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&texture))&&SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&target)),"capacity target");
        QueryRetirement query(&ops,domain);
        check(query.prepare_split(device,deferred.Get(),target.Get(),generation,std::make_shared<int>(1),std::make_shared<int>(2))==Result::invalid_argument,"foreign query context denied");
        check(query.prepare_split(device,context,target.Get(),generation+1,std::make_shared<int>(1),std::make_shared<int>(2))==Result::invalid_argument,"stale query generation denied");
        // submit uses slot64; restore needs65 and must leave state unchanged.
        std::vector<ContextOwnership::Operation> operations;operations.reserve(ContextOwnership::capacity);
        for(std::size_t i=domain->users();i<ContextOwnership::capacity-1;++i){auto op=domain->enter();check(op.result()==Result::ok,"bounded operation admission");operations.push_back(std::move(op));}
        D3D11_PRIMITIVE_TOPOLOGY before{},after{};context->IAGetPrimitiveTopology(&before);
        check(backend.submit(target.Get(),{generation,8,8})==Result::out_of_budget,"restore capacity failure propagated");
        context->IAGetPrimitiveTopology(&after);check(after==before,"pipeline unchanged after denied restore");
        {auto last=domain->enter();check(last.result()==Result::ok&&domain->enter().result()==Result::out_of_budget,"operation capacity denied");}
        operations.clear();backend.release_owned_references();
    }
    HostTeardownReceipt receipt,forged;check(domain->issue(receipt)==Result::invalid_argument,"early receipt denied");
    check(host->acknowledge_teardown(forged)==Result::invalid_argument,"forged receipt denied");
    check(!host->texture(),"raw target outside lease denied");
    auto s=snapshot();auto a=atlas();std::weak_ptr<ars::moc3::Snapshot> cpu_witness=s;
    ars::moc3::AtlasResidency cache;
    auto pass=[&](auto& backend,auto* target,auto info,auto& query){return ars::moc3::render_offscreen_moc3(backend,target,info,s,a,{},query,nullptr,&cache);};
    check(host->request_moc(pass,1,32,32)==Result::ok,"owned MOC query");s.reset();a.reset();
    check(!cpu_witness.expired(),"query CPU retained");
    if(loss){ops.loss=true;check(host->poll_commit()==Result::device_lost,"bounded injected loss");}
    else {check(until([&]{return host->poll_commit();})==Result::ok,"healthy exact query");check(cpu_witness.expired(),"healthy CPU released");}
    auto ticket=domain->ticket();ContextOwnership::Participant external;ComPtr<ID3D11DeviceContext> external_context=context;
    check(domain->retain(external)==Result::ok,"registered external GPU owner");
    Result wrong=Result::ok;std::thread other([&]{wrong=domain->enter().result();});other.join();check(wrong==Result::wrong_thread,"wrong R denied");
    {
        auto operation=domain->enter();check(operation.result()==Result::ok,"readback operation admitted");
        StateScope restore(context,domain);check(restore.result()==Result::ok,"restore enrolled");
        std::thread closer([&]{host->begin_close();});closer.join();
        check(domain->enter().result()==Result::busy&&host->poll_close()==Result::busy,"close defers active restore/readback");
        check(domain->issue(receipt)==Result::busy,"active user denies receipt");
    }
    check(domain->enter(ticket).result()==Result::busy,"late callback ticket denied");
    check(host->detach_lost()==Result::invalid_argument,"legacy teardown bypass denied");
    auto close=until([&]{return host->poll_close();});check(close==(loss?Result::device_lost:Result::ok),"owned close state");
    if(loss){check(host->prepare_lost_teardown()==Result::ok,"GPU surrendered first");check(!cpu_witness.expired(),"unknown CPU still retained");}
    check(domain->issue(receipt)==Result::busy,"cache/external references block receipt");
    cache.state.reset();check(domain->issue(receipt)==Result::busy,"external owner independently blocks receipt");external_context.Reset();external.reset();
    check(ars_runtime_begin_close(runtime)==ARS_OK&&ars_runtime_finish_close(runtime)==ARS_E_BUSY,"public finish blocked by graphics pin");
    check(domain->owners()==0&&domain->users()==0&&domain->issue(receipt)==Result::ok,"all tracked GPU owners surrendered");
    check(!runtime_witness.expired()&&ars_runtime_finish_close(runtime)==ARS_E_BUSY,"issued receipt still pins runtime");
    {
        // Deliberately misuse the diagnostic consumer with an authority it did
        // not mint. It must not consume a domain receipt or release CPU custody.
        Ops diagnostic_ops;diagnostic_ops.loss=true;
        CloseCoordinator diagnostic(device,context,generation,&diagnostic_ops,domain->authority());
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=8;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11RenderTargetView> target;
        check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&texture))&&SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&target)),"diagnostic consumer fixture");
        check(diagnostic.submit(target.Get(),{generation,8,8},{})==Result::device_lost,"diagnostic loss phase");diagnostic.begin_close();
        check(diagnostic.acknowledge_lost_teardown(generation,receipt)==Result::invalid_argument&&!runtime_witness.expired(),"legacy consumer cannot consume domain receipt");
    }
    std::thread consumer([&]{wrong=host->acknowledge_teardown(receipt);});consumer.join();check(wrong==Result::wrong_thread,"wrong-thread receipt denied");
    // A receipt from the next domain/generation cannot acknowledge this host.
    std::shared_ptr<ContextOwnership> other_domain;ComPtr<ID3D11Device> other_device;ComPtr<ID3D11DeviceContext> other_context;D3D_FEATURE_LEVEL level{};
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&other_device,&level,&other_context)),"other device");
    check(ContextOwnership::create(other_device.Get(),other_context.Get(),generation+1,std::make_shared<int>(2),other_domain)==Result::ok,"other authority");
    std::unique_ptr<Host> other_host;check(Host::create_owned(other_domain,nullptr,other_host)==Result::ok,"other host");other_host->begin_close();check(other_host->poll_close()==Result::ok,"other close");
    HostTeardownReceipt stale;check(other_domain->issue(stale)==Result::ok,"other receipt");
    check(host->acknowledge_teardown(stale)==Result::invalid_argument&&other_host->acknowledge_teardown(receipt)==Result::invalid_argument,"wrong domain/device/generation denied");
    check(other_host->acknowledge_teardown(stale)==Result::ok,"other receipt remains consumable");
    check(host->acknowledge_teardown(receipt)==Result::ok,"opaque receipt consumed");
    check(cpu_witness.expired()&&runtime_witness.expired(),"CPU/runtime released only after receipt");
    check(host->acknowledge_teardown(receipt)==Result::invalid_argument&&domain->issue(receipt)==Result::invalid_argument,"replay denied");
    check(ars_runtime_finish_close(runtime)==ARS_OK,"public finish after receipt");
}
}
int main(int argc,char **argv){
    const bool hardware=argc==4&&std::string_view(argv[3])=="--hardware";
    if(argc!=3&&!hardware)return 2;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    check(SUCCEEDED(D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"debug layer required");
    ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC description{};
    check(SUCCEEDED(device.As(&dxgi))&&SUCCEEDED(dxgi->GetAdapter(&adapter))&&SUCCEEDED(adapter->GetDesc(&description)),"adapter identity");
    if(hardware)check(description.VendorId!=0x1414,"named hardware required");
    std::wcout<<L"adapter="<<description.Description<<L" vendor="<<description.VendorId<<L" device="<<description.DeviceId<<L'\n';
    ComPtr<ID3D11Debug> debug;ComPtr<ID3D11InfoQueue> messages;check(SUCCEEDED(device.As(&debug))&&SUCCEEDED(device.As(&messages)),"debug interfaces");
    auto report=[&](const char *marker){context->ClearState();context->Flush();OutputDebugStringA(marker);
        check(SUCCEEDED(debug->ReportLiveDeviceObjects(static_cast<D3D11_RLDO_FLAGS>(D3D11_RLDO_DETAIL|D3D11_RLDO_IGNORE_INTERNAL))),"live report");};
    report("ARS_LIVE_BASELINE\n");messages->ClearStoredMessages();
    scope_identity_case(device.Get(),context.Get());
    for(unsigned i=0;i<4;++i)domain_case(device.Get(),context.Get(),i%2!=0,i+1);
    dual_cases(device.Get(),context.Get(),argv[1],argv[2]);
    adapter_cases(hardware);
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T n=0;messages->GetMessage(i,nullptr,&n);std::vector<unsigned char> data(n);auto *m=reinterpret_cast<D3D11_MESSAGE*>(data.data());
        if(SUCCEEDED(messages->GetMessage(i,m,&n))&&m->Severity<=D3D11_MESSAGE_SEVERITY_WARNING)std::cerr<<"D3D message="<<m->pDescription<<'\n';
        check(SUCCEEDED(messages->GetMessage(i,m,&n))&&m->Severity>D3D11_MESSAGE_SEVERITY_WARNING,"no debug warnings/errors");}
    report("ARS_LIVE_FINAL\n");
    std::cout<<"OWNED_CONTEXT_PASS assertions="<<assertions<<" injected_only=1 physical_loss=HOLD\n";
}
