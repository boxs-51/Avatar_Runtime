#pragma once
#ifdef _WIN32
#include "../backend.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <thread>

namespace ars::graphics::d3d11 {
class HostDevice {
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_;
    std::thread::id owner_;
    std::uint64_t generation_;
    Result init();
public:
    HostDevice(ID3D11Device *device, ID3D11DeviceContext *context, std::uint64_t generation);
    ~HostDevice();
    HostDevice(const HostDevice &)=delete;
    HostDevice &operator=(const HostDevice &)=delete;
    Result validate(ID3D11RenderTargetView *target, TargetInfo info) const;
    Result submit(ID3D11RenderTargetView *target, TargetInfo info);
    Result release_owned_references();
    ID3D11DeviceContext *context() const noexcept { return context_.Get(); }
    ID3D11Device *device() const noexcept { return device_.Get(); }
    std::uint64_t generation() const noexcept { return generation_; }
    bool on_owner_thread() const noexcept { return std::this_thread::get_id()==owner_; }
};
}
#endif
