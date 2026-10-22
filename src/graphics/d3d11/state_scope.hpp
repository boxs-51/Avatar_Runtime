#pragma once
#ifdef _WIN32
#include "context_ownership.hpp"
#include <array>
#include <d3d11.h>
#include <wrl/client.h>

namespace ars::graphics::d3d11 {
// Captures SDK-touched slots and isolates interfering host shaders/predication.
// Keep the scope through uploads, draws, copies and query End. Get* references
// remain owned through restoration and are released before admission ends.
class StateScope {
    ContextOwnership::Operation operation_;
    Result result_=Result::ok;
    ID3D11DeviceContext *context_;
    D3D11_PRIMITIVE_TOPOLOGY topology_{};
    Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer_, index_buffer_;
    UINT vertex_stride_=0, vertex_offset_=0, index_offset_=0;
    DXGI_FORMAT index_format_=DXGI_FORMAT_UNKNOWN;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> gs_;
    Microsoft::WRL::ComPtr<ID3D11HullShader> hs_;
    Microsoft::WRL::ComPtr<ID3D11DomainShader> ds_;
    Microsoft::WRL::ComPtr<ID3D11Predicate> predicate_;
    BOOL predicate_value_=FALSE;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vs_constant_;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>,2> ps_resources_;
    std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>,2> ps_samplers_;
    std::array<ID3D11ClassInstance *, 256> vs_classes_{};
    std::array<ID3D11ClassInstance *, 256> ps_classes_{};
    std::array<ID3D11ClassInstance *, 256> gs_classes_{},hs_classes_{},ds_classes_{};
    UINT vs_count_=256, ps_count_=256;
    UINT gs_count_=256,hs_count_=256,ds_count_=256;
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
    explicit StateScope(ID3D11DeviceContext *context,std::shared_ptr<ContextOwnership> ownership={});
    Result result() const noexcept {return result_!=Result::ok?result_:operation_.result();}
    ~StateScope();
    StateScope(const StateScope &)=delete;
    StateScope &operator=(const StateScope &)=delete;
};
}
#endif
