#include "state_scope.hpp"
#ifdef _WIN32
namespace ars::graphics::d3d11 {
StateScope::StateScope(ID3D11DeviceContext *context) : context_(context) {
    context_->IAGetPrimitiveTopology(&topology_);
    context_->VSGetShader(vs_.GetAddressOf(), vs_classes_.data(), &vs_count_);
    context_->PSGetShader(ps_.GetAddressOf(), ps_classes_.data(), &ps_count_);
    context_->RSGetState(rasterizer_.GetAddressOf());
    context_->RSGetViewports(&viewport_count_, viewports_.data());
    ID3D11RenderTargetView *views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    context_->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, views, dsv_.GetAddressOf());
    for (std::size_t i=0; i<rtvs_.size(); ++i) rtvs_[i].Attach(views[i]);
    context_->OMGetBlendState(blend_.GetAddressOf(), blend_factor_, &sample_mask_);
    context_->OMGetDepthStencilState(depth_.GetAddressOf(), &stencil_ref_);
}
StateScope::~StateScope() {
    context_->IASetPrimitiveTopology(topology_);
    context_->VSSetShader(vs_.Get(), vs_classes_.data(), vs_count_);
    context_->PSSetShader(ps_.Get(), ps_classes_.data(), ps_count_);
    context_->RSSetState(rasterizer_.Get());
    context_->RSSetViewports(viewport_count_, viewports_.data());
    ID3D11RenderTargetView *views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    for (std::size_t i=0; i<rtvs_.size(); ++i) views[i]=rtvs_[i].Get();
    context_->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, views, dsv_.Get());
    context_->OMSetBlendState(blend_.Get(), blend_factor_, sample_mask_);
    context_->OMSetDepthStencilState(depth_.Get(), stencil_ref_);
    for (UINT i=0; i<vs_count_; ++i) if (vs_classes_[i]) vs_classes_[i]->Release();
    for (UINT i=0; i<ps_count_; ++i) if (ps_classes_[i]) ps_classes_[i]->Release();
}
}
#endif
