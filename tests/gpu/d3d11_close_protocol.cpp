#include "graphics/d3d11/close_coordinator.hpp"
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using ars::graphics::Result;
using ars::graphics::d3d11::CloseCoordinator;
using ars::graphics::d3d11::QueryOps;
using ars::graphics::d3d11::HostTeardownReceipt;
using ars::graphics::d3d11::PrivateTeardownHost;

namespace {
struct Script final : QueryOps {
    HRESULT status=S_OK;
    HRESULT reason=S_OK;
    BOOL done=TRUE;
    unsigned flushes=0;
    std::atomic<bool> entered{false}, release{false};
    bool block_once=false;
    HRESULT removed_reason(ID3D11Device *) override {
        if (block_once) {
            block_once=false;
            entered.store(true,std::memory_order_release);
            while (!release.load(std::memory_order_acquire)) std::this_thread::yield();
        }
        return reason;
    }
    HRESULT get_data(ID3D11DeviceContext *,ID3D11Query *,BOOL *out) override {
        *out=done;
        return status;
    }
    void flush(ID3D11DeviceContext *) override { ++flushes; }
};
struct Fixture {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> target;
    bool create() {
        if (FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,
            D3D11_SDK_VERSION,&device,nullptr,&context))) return false;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=16; desc.Height=16; desc.MipLevels=1; desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        return SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)) &&
               SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&target));
    }
};
#define CHECK(test, code) do { if (!(test)) { std::printf("FAIL case=%d\n",code); return code; } } while (false)
}
int main() {
    Fixture f;
    CHECK(f.create(),2);
    {
        Script ops;
        CloseCoordinator close(f.device.Get(),f.context.Get(),7,&ops);
        CHECK(close.begin_close()==Result::ok && close.begin_close()==Result::ok,3);
        CHECK(close.poll_close(7)==Result::ok && close.phase()==CloseCoordinator::Phase::drained,4);
        CHECK(close.submit(f.target.Get(),{7,16,16},nullptr)==Result::busy,5);
    }
    {
        Script ops;
        CloseCoordinator close(f.device.Get(),f.context.Get(),8,&ops);
        auto pin=std::make_shared<int>(1);
        std::weak_ptr<int> witness=pin;
        CHECK(close.submit(f.target.Get(),{8,16,16},pin)==Result::ok,6);
        pin.reset();
        CHECK(!witness.expired() && close.pending_count()==1,7);
        Result off_thread=Result::ok;
        std::thread wrong([&] { close.begin_close(); off_thread=close.poll_close(8); });
        wrong.join();
        CHECK(off_thread==Result::wrong_thread && close.close_requested(),8);
        CHECK(close.poll_close(9)==Result::invalid_argument,9);
        ops.status=S_FALSE;
        CHECK(close.poll_close(8)==Result::busy && !witness.expired() && ops.flushes==1,10);
        CHECK(close.poll_close(8)==Result::busy && ops.flushes==1,11);
        ops.status=S_OK; ops.done=TRUE;
        CHECK(close.poll_close(8)==Result::ok && close.phase()==CloseCoordinator::Phase::drained,12);
        CHECK(witness.expired() && close.pending_count()==0,13);
    }
    {
        Script ops;
        CloseCoordinator close(f.device.Get(),f.context.Get(),80,&ops);
        std::vector<std::weak_ptr<int>> witnesses;
        for (int i=0;i<64;++i) {
            auto pin=std::make_shared<int>(i);
            witnesses.push_back(pin);
            CHECK(close.submit(f.target.Get(),{80,16,16},pin)==Result::ok,35);
        }
        CHECK(close.pending_count()==witnesses.size(),36);
        close.begin_close();
        ops.status=S_FALSE;
        CHECK(close.poll_close(80)==Result::busy,37);
        for (const auto &pin : witnesses) CHECK(!pin.expired(),38);
        ops.status=S_OK;
        CHECK(close.poll_close(80)==Result::ok && close.pending_count()==0,39);
        for (const auto &pin : witnesses) CHECK(pin.expired(),40);
    }
    {
        Script ops;
        ops.block_once=true;
        CloseCoordinator close(f.device.Get(),f.context.Get(),81,&ops);
        std::thread closer([&] {
            while (!ops.entered.load(std::memory_order_acquire)) std::this_thread::yield();
            close.begin_close();
            ops.release.store(true,std::memory_order_release);
        });
        const auto admitted=close.submit(f.target.Get(),{81,16,16},nullptr);
        closer.join();
        CHECK(admitted==Result::ok && close.close_requested() && close.pending_count()==1,31);
        CHECK(close.submit(f.target.Get(),{81,16,16},nullptr)==Result::busy,32);
        CHECK(close.poll_close(81)==Result::ok,33);
    }
    {
        Script ops;
        PrivateTeardownHost host(f.device.Get(),f.context.Get(),f.target.Get(),9);
        CloseCoordinator close(f.device.Get(),f.context.Get(),9,&ops,host.authority());
        HostTeardownReceipt receipt;
        auto pin=std::make_shared<int>(2);
        std::weak_ptr<int> witness=pin;
        CHECK(close.submit(f.target.Get(),{9,16,16},pin)==Result::ok,14);
        pin.reset(); close.begin_close(); ops.status=E_FAIL;
        CHECK(close.poll_close(9)==Result::failed &&
              close.phase()==CloseCoordinator::Phase::lost_quarantined,15);
        CHECK(!witness.expired() && close.pending_count()==1,16);
        CHECK(host.issue(receipt)==Result::invalid_argument &&
              close.acknowledge_lost_teardown(9,receipt)==Result::invalid_argument,17);
        Result off_thread=Result::ok;
        std::thread wrong([&] { off_thread=close.acknowledge_lost_teardown(9,receipt); });
        wrong.join();
        CHECK(off_thread==Result::wrong_thread && !witness.expired(),18);
        CHECK(host.admit_user()==Result::ok && host.begin_close()==Result::ok &&
              host.issue(receipt)==Result::invalid_argument && !witness.expired() &&
              host.release_user()==Result::ok,41);
        Result wrong_issue=Result::ok;
        std::thread issuer([&] { wrong_issue=host.issue(receipt); });
        issuer.join();
        CHECK(wrong_issue==Result::wrong_thread && !witness.expired() &&
              host.issue(receipt)==Result::ok,44);
        CHECK(close.acknowledge_lost_teardown(8,receipt)==Result::invalid_argument &&
              !witness.expired(),42);
        PrivateTeardownHost other(f.device.Get(),f.context.Get(),f.target.Get(),9);
        HostTeardownReceipt other_receipt;
        CHECK(other.begin_close()==Result::ok && other.issue(other_receipt)==Result::ok &&
              close.acknowledge_lost_teardown(9,other_receipt)==Result::invalid_argument &&
              !witness.expired(),43);
        CHECK(close.acknowledge_lost_teardown(9,receipt)==Result::ok && witness.expired(),19);
        CHECK(close.phase()==CloseCoordinator::Phase::detached_lost &&
              close.acknowledge_lost_teardown(9,receipt)==Result::invalid_argument,20);
    }
    for (const HRESULT reason : {DXGI_ERROR_DEVICE_REMOVED,DXGI_ERROR_DEVICE_RESET}) {
        Script ops;
        PrivateTeardownHost host(f.device.Get(),f.context.Get(),f.target.Get(),10);
        CloseCoordinator close(f.device.Get(),f.context.Get(),10,&ops,host.authority());
        HostTeardownReceipt receipt;
        auto pin=std::make_shared<int>(3);
        std::weak_ptr<int> witness=pin;
        CHECK(close.submit(f.target.Get(),{10,16,16},pin)==Result::ok,21);
        pin.reset(); close.begin_close(); ops.reason=reason;
        CHECK(close.poll_close(10)==Result::device_lost && !witness.expired(),22);
        CHECK(close.submit(f.target.Get(),{10,16,16},nullptr)==Result::device_lost,34);
        CHECK(host.begin_close()==Result::ok && host.issue(receipt)==Result::ok &&
              close.acknowledge_lost_teardown(10,receipt)==Result::ok && witness.expired(),23);
    }
    {
        Script ops;
        PrivateTeardownHost host(f.device.Get(),f.context.Get(),f.target.Get(),11);
        CloseCoordinator close(f.device.Get(),f.context.Get(),11,&ops,host.authority());
        HostTeardownReceipt receipt;
        auto pin=std::make_shared<int>(4);
        std::weak_ptr<int> witness=pin;
        CHECK(close.submit(f.target.Get(),{11,16,16},pin)==Result::ok,24);
        pin.reset(); close.begin_close(); ops.status=S_OK; ops.done=FALSE;
        CHECK(close.poll_close(11)==Result::failed && !witness.expired(),25);
        CHECK(host.begin_close()==Result::ok && host.issue(receipt)==Result::ok &&
              close.acknowledge_lost_teardown(11,receipt)==Result::ok && witness.expired(),26);
    }
    {
        Fixture replacement;
        CHECK(replacement.create(),27);
        Script ops;
        CloseCoordinator close(replacement.device.Get(),replacement.context.Get(),12,&ops);
        CHECK(close.submit(f.target.Get(),{12,16,16},nullptr)==Result::invalid_argument,28);
        CHECK(close.submit(replacement.target.Get(),{12,16,16},nullptr)==Result::ok,29);
        close.begin_close();
        CHECK(close.poll_close(12)==Result::ok,30);
    }
    std::puts("PASS D07/D08 deterministic WARP close, BUSY, failed-query, removed-reason and recovery paths");
    return 0;
}
