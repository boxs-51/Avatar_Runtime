#include "graphics/d3d11/host_device.hpp"
#include "graphics/d3d11/query_retirement.hpp"
#include <d3d11sdklayers.h>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
using ars::graphics::d3d11::HostDevice;
using ars::graphics::d3d11::QueryRetirement;

int main() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level{};
    HRESULT created=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_DEBUG,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
    const bool debug_layer=SUCCEEDED(created);
    if (!debug_layer) created=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,
        nullptr,0,D3D11_SDK_VERSION,&device,&level,&context);
    if (FAILED(created)) { std::puts("HOLD physical D3D11 unavailable"); return 2; }
    std::printf("debug_layer=%s\n",debug_layer?"available":"unavailable");
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=16; desc.Height=16; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    HostDevice backend(device.Get(),context.Get(),42);
    ComPtr<ID3D11Debug> debug;
    if (debug_layer && FAILED(device.As(&debug))) return 10;
    unsigned busy_count=0;
    for (unsigned cycle=0; cycle<100; ++cycle) {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11RenderTargetView> rtv;
        if (FAILED(device->CreateTexture2D(&desc,nullptr,&texture)) ||
            FAILED(device->CreateRenderTargetView(texture.Get(),nullptr,&rtv))) return 3;
        if (backend.submit(rtv.Get(), {42,desc.Width,16})!=Result::ok) return 4;
        auto pin=std::make_shared<unsigned>(cycle);
        std::weak_ptr<unsigned> witness=pin;
        QueryRetirement retirement;
        if (retirement.begin(device.Get(),context.Get(),rtv.Get(),42,pin)!=Result::ok) return 5;
        pin.reset(); rtv.Reset(); texture.Reset();
        if (witness.expired() || !retirement.holds_target()) return 6;
        if (retirement.poll(43)!=Result::invalid_argument || witness.expired()) return 7;
        Result off_thread=Result::ok;
        std::thread wrong([&] { off_thread=retirement.poll(42); });
        wrong.join();
        if (off_thread!=Result::wrong_thread) return 8;
        Result result=Result::busy;
        // Test harness waits outside graphics/runtime locks. Production caller
        // receives BUSY and schedules another bounded R-thread poll.
        for (unsigned attempt=0; attempt<200 && result==Result::busy; ++attempt) {
            result=retirement.poll(42);
            if (result==Result::busy) {
                ++busy_count;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        if (result!=Result::ok || !retirement.complete() || retirement.holds_target() || !witness.expired()) return 9;
        if (cycle==0 && debug) {
            context->ClearState(); context->Flush();
            OutputDebugStringA("ARS_LIVE_BASELINE\n");
            if (FAILED(debug->ReportLiveDeviceObjects(static_cast<D3D11_RLDO_FLAGS>(
                D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL)))) return 10;
        }
        desc.Width=(desc.Width==16 ? 17 : 16); // 100 target resize/recreate cycles
    }
    if (debug) {
        context->ClearState(); context->Flush();
        OutputDebugStringA("ARS_LIVE_FINAL\n");
        if (FAILED(debug->ReportLiveDeviceObjects(static_cast<D3D11_RLDO_FLAGS>(
            D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL)))) return 10;
    }
    if (debug_layer) {
        ComPtr<ID3D11InfoQueue> messages;
        if (FAILED(device.As(&messages))) return 10;
        const auto count=messages->GetNumStoredMessagesAllowedByRetrievalFilter();
        unsigned errors=0;
        for (UINT64 i=0;i<count;++i) {
            SIZE_T size=0;
            if (FAILED(messages->GetMessage(i,nullptr,&size))) return 10;
            auto message=std::make_unique<unsigned char[]>(size);
            if (FAILED(messages->GetMessage(i,reinterpret_cast<D3D11_MESSAGE *>(message.get()),&size))) return 10;
            const auto severity=reinterpret_cast<D3D11_MESSAGE *>(message.get())->Severity;
            if (severity==D3D11_MESSAGE_SEVERITY_ERROR || severity==D3D11_MESSAGE_SEVERITY_CORRUPTION) ++errors;
        }
        std::printf("debug_messages=%llu errors=%u\n",static_cast<unsigned long long>(count),errors);
        if (errors) return 11;
    }
    std::printf("PASS D2 event-query 100 cycles; busy=%u feature=%04x\n",busy_count,static_cast<unsigned>(level));
    return 0;
}
