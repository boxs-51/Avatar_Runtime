#pragma once
#ifdef _WIN32
#include "../backend.hpp"
#include "context_ownership.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <thread>

namespace ars::graphics::d3d11 {
class HostDevice {
    std::shared_ptr<ContextOwnership> ownership_;
    ContextOwnership::Participant gpu_owner_;
    Result ownership_status_=Result::ok;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_;
    std::thread::id owner_;
    std::uint64_t generation_;
    bool lost_ = false;
    Result init();
public:
    HostDevice(ID3D11Device *device, ID3D11DeviceContext *context, std::uint64_t generation,std::shared_ptr<ContextOwnership> ownership={});
    ~HostDevice();
    HostDevice(const HostDevice &)=delete;
    HostDevice &operator=(const HostDevice &)=delete;
    Result validate(ID3D11RenderTargetView *target, TargetInfo info) const;
    Result submit(ID3D11RenderTargetView *target, TargetInfo info);
    Result release_owned_references();
    Result mark_lost();
    ID3D11DeviceContext *context() const noexcept { return !ownership_||ownership_->admitted()?context_.Get():nullptr; }
    ID3D11Device *device() const noexcept { return !ownership_||ownership_->admitted()?device_.Get():nullptr; }
    const std::shared_ptr<ContextOwnership> &ownership() const noexcept{return ownership_;}
    ContextOwnership::Operation enter_context(bool drain=false) const {return ownership_?ownership_->enter(drain):ContextOwnership::Operation{};}
    Result ownership_status() const noexcept{return ownership_status_;}
    std::uint64_t generation() const noexcept { return generation_; }
    bool on_owner_thread() const noexcept { return std::this_thread::get_id()==owner_; }
};
}
#endif
