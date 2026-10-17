#include "graphics/d3d11/close_coordinator.hpp"
#include <d3d11sdklayers.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
using ars::graphics::d3d11::CloseCoordinator;

int main() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context))) {
        std::puts("HOLD physical debug-layer D3D11 unavailable"); return 2;
    }
    ComPtr<ID3D11Debug> debug;
    if (FAILED(device.As(&debug))) return 3;
    context->ClearState(); context->Flush();
    OutputDebugStringA("ARS_LIVE_BASELINE\n");
    if (FAILED(debug->ReportLiveDeviceObjects(static_cast<D3D11_RLDO_FLAGS>(
        D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL)))) return 4;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=16; desc.Height=16; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    unsigned busy=0, stale_rejected=0, concurrent_admitted=0, concurrent_busy=0;
    ComPtr<ID3D11RenderTargetView> old_target;
    UINT old_width=0;
    for (std::uint64_t cycle=0;cycle<100;++cycle) {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11RenderTargetView> target;
        if (FAILED(device->CreateTexture2D(&desc,nullptr,&texture)) ||
            FAILED(device->CreateRenderTargetView(texture.Get(),nullptr,&target))) return 5;
        auto pin=std::make_shared<std::uint64_t>(cycle);
        std::weak_ptr<std::uint64_t> witness=pin;
        {
            CloseCoordinator close(device.Get(),context.Get(),cycle+1);
            if (old_target) {
                if (close.submit(old_target.Get(),{cycle,old_width,16},nullptr)!=Result::invalid_argument ||
                    close.submit(old_target.Get(),{cycle+1,desc.Width,16},nullptr)!=Result::unsupported ||
                    close.pending_count()!=0) return 15;
                stale_rejected+=2;
                old_target.Reset();
            }
            if (close.submit(target.Get(),{cycle+1,desc.Width,16},pin)!=Result::ok) return 6;
            pin.reset();
            if (witness.expired() || close.pending_count()!=1) return 7;
            std::atomic<bool> start_close{false};
            std::thread closer([&] {
                while (!start_close.load(std::memory_order_acquire)) std::this_thread::yield();
                close.begin_close();
            });
            start_close.store(true,std::memory_order_release);
            // Exercise both sides of admission: preclosed on even cycles,
            // potentially admitted on odd cycles while close intent races.
            if (cycle%2==0)
                while (!close.close_requested()) std::this_thread::yield();
            const auto raced=close.submit(target.Get(),{cycle+1,desc.Width,16},nullptr);
            closer.join();
            if (raced==Result::ok) ++concurrent_admitted;
            else if (raced==Result::busy) ++concurrent_busy;
            else return 16;
            if (!close.close_requested() ||
                close.pending_count()!=(raced==Result::ok ? 2u : 1u) ||
                close.submit(target.Get(),{cycle+1,desc.Width,16},nullptr)!=Result::busy)
                return 17;
            old_width=desc.Width;
            old_target=target;
            target.Reset(); texture.Reset();
            Result result=Result::busy;
            for (unsigned attempt=0;attempt<200 && result==Result::busy;++attempt) {
                result=close.poll_close(cycle+1);
                if (result==Result::busy) {
                    ++busy;
                    if (witness.expired()) return 8;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            if (result!=Result::ok || close.phase()!=CloseCoordinator::Phase::drained ||
                !witness.expired() || close.pending_count()!=0) return 9;
        }
        desc.Width=desc.Width==16 ? 17 : 16;
    }
    if (concurrent_busy<50) return 18;
    old_target.Reset();
    // The test host owns the immediate context and performs its own teardown.
    context->ClearState(); context->Flush();
    OutputDebugStringA("ARS_LIVE_FINAL\n");
    if (FAILED(debug->ReportLiveDeviceObjects(static_cast<D3D11_RLDO_FLAGS>(
        D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL)))) return 10;
    ComPtr<ID3D11InfoQueue> messages;
    if (FAILED(device.As(&messages))) return 11;
    const auto count=messages->GetNumStoredMessagesAllowedByRetrievalFilter();
    unsigned errors=0;
    for (UINT64 i=0;i<count;++i) {
        SIZE_T size=0;
        if (FAILED(messages->GetMessage(i,nullptr,&size))) return 12;
        auto bytes=std::make_unique<unsigned char[]>(size);
        if (FAILED(messages->GetMessage(i,reinterpret_cast<D3D11_MESSAGE *>(bytes.get()),&size))) return 13;
        const auto severity=reinterpret_cast<D3D11_MESSAGE *>(bytes.get())->Severity;
        if (severity==D3D11_MESSAGE_SEVERITY_ERROR || severity==D3D11_MESSAGE_SEVERITY_CORRUPTION) ++errors;
    }
    std::printf("debug_messages=%llu errors=%u\n",static_cast<unsigned long long>(count),errors);
    if (errors) return 14;
    std::printf("PASS D02/D03/D07 physical 100 close/recreate cycles busy=%u stale_rejected=%u concurrent_admitted=%u concurrent_busy=%u feature=%04x\n",
                busy,stale_rejected,concurrent_admitted,concurrent_busy,static_cast<unsigned>(level));
    return 0;
}
