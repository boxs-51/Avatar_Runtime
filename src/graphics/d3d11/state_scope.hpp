#pragma once
#ifdef _WIN32
#include <array>
#include <d3d11.h>
#include <wrl/client.h>

namespace ars::graphics::d3d11 {
// Exactly the pipeline slots changed by HostDevice::submit. Get* calls retain
// references; the scope owns them through restoration and then releases them.
class StateScope {
    ID3D11DeviceContext *context_;
    D3D11_PRIMITIVE_TOPOLOGY topology_{};
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
    std::array<ID3D11ClassInstance *, 256> vs_classes_{};
    std::array<ID3D11ClassInstance *, 256> ps_classes_{};
    UINT vs_count_=256, ps_count_=256;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports_{};
    UINT viewport_count_=static_cast<UINT>(viewports_.size());
    std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs_{};
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;
    FLOAT blend_factor_[4]{};
    UINT sample_mask_{};
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_;
    UINT stencil_ref_{};
public:
    explicit StateScope(ID3D11DeviceContext *context);
    ~StateScope();
    StateScope(const StateScope &)=delete;
    StateScope &operator=(const StateScope &)=delete;
};
}
#endif
