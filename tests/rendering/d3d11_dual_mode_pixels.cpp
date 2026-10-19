#include "rendering/d3d11_dual_mode_host.hpp"
#include "../assets/asset_test_support.hpp"
#include <d3d11sdklayers.h>
#include <dxgi.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
using ars::rendering::D3D11DualModeHost;
namespace {
struct InjectedQuery final : ars::graphics::d3d11::QueryOps {
    HRESULT reason=S_OK;
    HRESULT status=S_OK;
    HRESULT removed_reason(ID3D11Device *) override { return reason; }
    HRESULT get_data(ID3D11DeviceContext *,ID3D11Query *,BOOL *done) override {
        *done=TRUE;return status;
    }
    void flush(ID3D11DeviceContext *) override {}
};
struct BlockingNativeQuery final : ars::graphics::d3d11::QueryOps {
    std::atomic<bool> entered{false},release{false};
    bool first=true;
    HRESULT removed_reason(ID3D11Device *device) override {
        if (first) {
            first=false;entered.store(true,std::memory_order_release);
            while (!release.load(std::memory_order_acquire)) std::this_thread::yield();
        }
        return device->GetDeviceRemovedReason();
    }
    HRESULT get_data(ID3D11DeviceContext *context,ID3D11Query *query,BOOL *done) override {
        return context->GetData(query,done,sizeof(*done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    }
    void flush(ID3D11DeviceContext *context) override { context->Flush(); }
};
std::array<unsigned char,4> pixel(ID3D11Device *device,ID3D11DeviceContext *context,
                                  ID3D11Texture2D *texture,unsigned x,unsigned y) {
    test::check(texture!=nullptr,"published target exists");
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    test::check(x<desc.Width && y<desc.Height,"readback coordinates");
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    test::check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"staging target");
    context->CopyResource(staging.Get(),texture);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    test::check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"staging map");
    const auto *p=static_cast<const unsigned char *>(mapped.pData)+y*mapped.RowPitch+x*4;
    const std::array<unsigned char,4> result{p[0],p[1],p[2],p[3]};
    context->Unmap(staging.Get(),0);
    return result;
}
void commit(D3D11DualModeHost &host) {
    test::check(host.pending(),"candidate pending before exact query completion");
    Result result=Result::busy;
    for (int i=0;i<100000 && result==Result::busy;++i) {
        result=host.poll_commit();
        if (result==Result::busy) std::this_thread::yield();
    }
    test::check(result==Result::ok && !host.pending(),"exact query commits candidate");
}
}
int main(int argc,char **argv) {
    test::check(argc==3 || (argc==4 && (std::string_view(argv[3])=="--hardware" ||
                                         std::string_view(argv[3])=="--live-report" ||
                                         std::string_view(argv[3])=="--live-hardware")),
                "schema worker, corpus and optional hardware");
    const bool hardware=argc==4 && (std::string_view(argv[3])=="--hardware" ||
                                     std::string_view(argv[3])=="--live-hardware");
    const bool live_report=argc==4 && (std::string_view(argv[3])=="--live-report" ||
                                        std::string_view(argv[3])=="--live-hardware");
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature{};
    auto hr=D3D11CreateDevice(nullptr,hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,
                              nullptr,D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,
                              &device,&feature,&context);
    const bool debug=SUCCEEDED(hr);
    if (FAILED(hr)) hr=D3D11CreateDevice(nullptr,
        hardware?D3D_DRIVER_TYPE_HARDWARE:D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
        D3D11_SDK_VERSION,&device,&feature,&context);
    test::check(SUCCEEDED(hr),"D3D11 device");
    ComPtr<IDXGIDevice> dxgi;
    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapter_info{};
    test::check(SUCCEEDED(device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(&adapter)) &&
                SUCCEEDED(adapter->GetDesc(&adapter_info)),"adapter identity");
    if (hardware) test::check(adapter_info.VendorId!=0x1414,"physical adapter required");
    std::printf("M2D_ADAPTER=%s vendor=%04x device=%04x feature=%04x debug=%u\n",
        hardware?"HARDWARE":"WARP",adapter_info.VendorId,adapter_info.DeviceId,
        unsigned(feature),unsigned(debug));
    ComPtr<ID3D11Debug> debug_device;
    if (live_report) {
        test::check(debug && SUCCEEDED(device.As(&debug_device)),"debug live-object interface");
        context->ClearState();context->Flush();
        OutputDebugStringA("ARS_LIVE_BASELINE\n");
        debug_device->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
    test::Host allocator;
    {
        ars::assets::Resource resource(test::Host::allocate,test::Host::release,&allocator);
        ars::validation::SchemaClient schema({std::filesystem::absolute(argv[1]).string()});
        const auto bytes=test::file(std::filesystem::path(argv[2])/"positive-dual-full.arschar");
        std::shared_ptr<const ars::assets::ValidatedCpuModel> model;
        test::check(ars::assets::validate_bundle(bytes,schema,resource,model)==ARS_OK &&
                    model && model->glb(),"validated dual CPU model");
        auto scene2=std::make_shared<ars::rendering::PreparedScene2D>();
        test::check(ars::rendering::prepare_static_2d(ars::rendering::scene_input(*model),*scene2)==
                    ARS_OK,"prepared dual 2D");
        auto atlas=std::shared_ptr<const ars::assets::Image>(model,&model->image());
        auto scene3=std::allocate_shared<ars::rendering::PreparedScene3D>(
            std::pmr::polymorphic_allocator<ars::rendering::PreparedScene3D>(&resource),&resource);
        test::check(ars::rendering::prepare_static_3d(*model,*scene3)==ARS_OK,
                    "host-allocated dual 3D pose");
        ars::rendering::Camera3D camera{{1,0,0,0,0,-1,0,0,0,0,-1,0,-0.5,1,0.5,1}};
        D3D11DualModeHost host(device.Get(),context.Get(),51);
        test::check(host.mode()==D3D11DualModeHost::Mode::none && !host.pending(),
                    "empty host starts unpublished");
        test::check(host.request_2d(scene2,atlas,1,8,8)==Result::ok &&
                    host.mode()==D3D11DualModeHost::Mode::none,"2D candidate not prematurely visible");
        commit(host);
        test::check(host.mode()==D3D11DualModeHost::Mode::two_d && host.pose_generation()==1,
                    "2D pose and mode commit together");
        const auto old_pixel=pixel(device.Get(),context.Get(),host.texture(),2,2);
        auto old_view=ComPtr<ID3D11RenderTargetView>(host.target());
        const auto old_epoch=host.target_epoch();
        test::check(host.accepts(old_view.Get(),old_epoch),"current target accepted");
        test::check(host.request_3d({},camera,2,8,8)==Result::unsupported &&
                    host.request_3d(scene3,camera,2,8,8,nullptr,true)==Result::out_of_budget &&
                    host.mode()==D3D11DualModeHost::Mode::two_d &&
                    host.pose_generation()==1 && host.target_epoch()==old_epoch &&
                    pixel(device.Get(),context.Get(),host.texture(),2,2)==old_pixel,
                    "missing mode and target OOM preserve old frame and pose");
        auto invalid_camera=camera;
        invalid_camera.clip_from_world[0]=std::numeric_limits<double>::quiet_NaN();
        test::check(host.request_3d(scene3,invalid_camera,2,8,8)==Result::invalid_argument &&
                    host.mode()==D3D11DualModeHost::Mode::two_d &&
                    pixel(device.Get(),context.Get(),host.texture(),2,2)==old_pixel,
                    "invalid camera rejects before old frame mutation");
        ars::rendering::GpuCreateProbe gpu_oom{1};
        test::check(host.request_3d(scene3,camera,2,8,8,&gpu_oom)==Result::out_of_budget &&
                    gpu_oom.attempts==1 && !host.pending() &&
                    host.pose_generation()==1 &&
                    pixel(device.Get(),context.Get(),host.texture(),2,2)==old_pixel,
                    "GPU creation OOM preserves old frame and pose");
        test::check(host.request_3d(scene3,camera,2,8,8)==Result::ok &&
                    host.mode()==D3D11DualModeHost::Mode::two_d,"3D pending keeps old mode");
        test::check(host.request_2d(scene2,atlas,3,8,8)==Result::busy,
                    "no second candidate while query pending");
        commit(host);
        test::check(host.mode()==D3D11DualModeHost::Mode::three_d && host.pose_generation()==2 &&
                    !host.accepts(old_view.Get(),old_epoch),"3D commit rejects stale RTV and epoch");
        const auto three_pixel=pixel(device.Get(),context.Get(),host.texture(),2,2);
        std::printf("M2D_2D_PIXEL=%u,%u,%u,%u M2D_3D_PIXEL=%u,%u,%u,%u\n",
            unsigned(old_pixel[0]),unsigned(old_pixel[1]),unsigned(old_pixel[2]),unsigned(old_pixel[3]),
            unsigned(three_pixel[0]),unsigned(three_pixel[1]),unsigned(three_pixel[2]),unsigned(three_pixel[3]));
        test::check(three_pixel!=old_pixel,"2D and 3D pixels differ");
        old_view.Reset();
        for (std::uint64_t i=0;i<100;++i) {
            const auto pose=i+3;
            const auto requested=i%2==0?
                host.request_2d(scene2,atlas,pose,8,8):
                host.request_3d(scene3,camera,pose,8,8);
            test::check(requested==Result::ok && host.pose_generation()==pose-1,
                        "switch accepted without early pose publish");
            commit(host);
            test::check(host.pose_generation()==pose &&
                        host.mode()==(i%2==0?D3D11DualModeHost::Mode::two_d:
                                       D3D11DualModeHost::Mode::three_d),
                        "100 switches publish matching mode and pose");
        }
        for (std::uint64_t i=0;i<100;++i) {
            const auto epoch=host.target_epoch();
            auto prior=ComPtr<ID3D11RenderTargetView>(host.target());
            const auto edge=std::uint32_t(i%2==0?16:8);
            test::check(host.request_2d(scene2,atlas,i+103,edge,edge)==Result::ok,
                        "host resize candidate");
            commit(host);
            test::check(host.target_epoch()==epoch+1 && !host.accepts(prior.Get(),epoch) &&
                        host.accepts(host.target(),host.target_epoch()),
                        "100 host resizes reject stale target");
        }
        const auto before_resize=host.target_epoch();
        auto stale_view=ComPtr<ID3D11RenderTargetView>(host.target());
        test::check(host.request_2d(scene2,atlas,203,16,16)==Result::ok,"resized candidate");
        commit(host);
        test::check(host.target_epoch()==before_resize+1 &&
                    !host.accepts(stale_view.Get(),before_resize) &&
                    host.accepts(host.target(),host.target_epoch()),
                    "host resize rejects old RTV before mutation");
        stale_view.Reset();
        auto faint=std::make_shared<ars::rendering::PreparedScene2D>(*scene2);
        faint->draws.resize(1);
        faint->draws[0].opacity=0.5f;
        test::check(host.request_2d(faint,atlas,204,16,16)==Result::ok,"opacity candidate");
        commit(host);
        const auto faint_pixel=pixel(device.Get(),context.Get(),host.texture(),4,4);
        test::check(faint_pixel[0]>100 && faint_pixel[0]<150 &&
                    faint_pixel[3]>100 && faint_pixel[3]<150,
                    "dynamic opacity changes host 2D pixels");
        auto hidden=std::make_shared<ars::rendering::PreparedScene2D>(*scene2);
        hidden->draws.clear();
        test::check(host.request_2d(hidden,atlas,205,16,16)==Result::ok,"hidden layer candidate");
        commit(host);
        test::check(pixel(device.Get(),context.Get(),host.texture(),4,4)==
                    std::array<unsigned char,4>{0,0,0,0},"dynamic hide clears 2D frame");
        Result off_thread=Result::ok;
        std::thread worker([&] { off_thread=host.request_2d(scene2,atlas,206,16,16);host.begin_close(); });
        worker.join();
        test::check(off_thread==Result::wrong_thread &&
                    host.request_3d(scene3,camera,206,16,16)==Result::busy,
                    "R-thread admission and off-thread close intent");
        test::check(host.poll_close()==Result::ok && !host.target(),"R-thread host close");
        D3D11DualModeHost closing(device.Get(),context.Get(),52);
        test::check(closing.request_2d(scene2,atlas,1,8,8)==Result::ok && closing.pending(),
                    "pending frame before close intent");
        std::thread closer([&] { closing.begin_close(); });
        closer.join();
        test::check(closing.request_3d(scene3,camera,2,8,8)==Result::busy,
                    "no post-close admission");
        Result close_result=Result::busy;
        for (int i=0;i<100000 && close_result==Result::busy;++i) {
            close_result=closing.poll_close();
            if (close_result==Result::busy) std::this_thread::yield();
        }
        test::check(close_result==Result::ok && !closing.pending() && !closing.target(),
                    "pending frame drains on R before close");
        for (int i=0;i<100;++i) {
            BlockingNativeQuery ops;
            D3D11DualModeHost raced(device.Get(),context.Get(),1000+i,&ops);
            std::thread intent([&] {
                while (!ops.entered.load(std::memory_order_acquire)) std::this_thread::yield();
                raced.begin_close();
                ops.release.store(true,std::memory_order_release);
            });
            const auto admitted=raced.request_2d(scene2,atlas,1,8,8);
            intent.join();
            test::check(admitted==Result::ok && raced.pending() &&
                        raced.request_2d(scene2,atlas,2,8,8)==Result::busy,
                        "100 host close intents preserve admitted query and reject later work");
            Result drained=Result::busy;
            for (int spin=0;spin<100000 && drained==Result::busy;++spin) {
                drained=raced.poll_close();
                if (drained==Result::busy) std::this_thread::yield();
            }
            test::check(drained==Result::ok && !raced.pending() && !raced.target(),
                        "100 host close intents drain exact query on R");
        }
        InjectedQuery injected;
        D3D11DualModeHost lost(device.Get(),context.Get(),53,&injected);
        auto isolated=std::make_shared<ars::rendering::PreparedScene3D>(*scene3);
        std::weak_ptr<const ars::rendering::PreparedScene3D> held=isolated;
        test::check(lost.request_3d(isolated,camera,1,8,8)==Result::ok &&
                    lost.detach_lost()==Result::invalid_argument,
                    "cannot acknowledge teardown before loss and close intent");
        isolated.reset();
        injected.reason=DXGI_ERROR_DEVICE_REMOVED;
        test::check(lost.poll_commit()==Result::device_lost && lost.lost() &&
                    lost.pending() && !held.expired() &&
                    lost.request_2d(scene2,atlas,2,8,8)==Result::device_lost,
                    "injected loss quarantines exact pending frame and CPU pin");
        Result wrong_detach=Result::ok;
        std::thread wrong([&] { wrong_detach=lost.detach_lost(); });
        wrong.join();
        test::check(wrong_detach==Result::wrong_thread && !held.expired(),
                    "wrong-thread teardown cannot release pin");
        lost.begin_close();
        test::check(lost.detach_lost()==Result::ok && held.expired() &&
                    lost.detach_lost()==Result::invalid_argument,
                    "isolated host teardown releases pin once after barrier");
        if (debug) {
            ComPtr<ID3D11InfoQueue> queue;
            test::check(SUCCEEDED(device.As(&queue)),"debug info queue");
            for (UINT64 i=0;i<queue->GetNumStoredMessages();++i) {
                SIZE_T size=0;queue->GetMessage(i,nullptr,&size);
                std::vector<unsigned char> buffer(size);
                test::check(SUCCEEDED(queue->GetMessage(i,
                    reinterpret_cast<D3D11_MESSAGE *>(buffer.data()),&size)),"debug message");
                const auto *message=reinterpret_cast<const D3D11_MESSAGE *>(buffer.data());
                test::check(message->Severity!=D3D11_MESSAGE_SEVERITY_ERROR &&
                            message->Severity!=D3D11_MESSAGE_SEVERITY_CORRUPTION,
                            "zero D3D error/corruption messages");
            }
        }
        std::printf("PASS M2-D %s 100 switches, 100 resizes, 100 close races, OOM rollback and injected loss\n",
                    hardware?"HARDWARE":"WARP");
    }
    test::check(allocator.live==0,"validated model and snapshots released");
    if (live_report) {
        context->ClearState();context->Flush();
        OutputDebugStringA("ARS_LIVE_FINAL\n");
        debug_device->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
}
