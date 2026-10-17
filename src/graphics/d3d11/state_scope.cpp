#include "state_scope.hpp"
#ifdef _WIN32
namespace ars::graphics::d3d11 {
StateScope::StateScope(ID3D11DeviceContext *context) : context_(context) {
    context_->IAGetPrimitiveTopology(&topology_);
    context_->IAGetInputLayout(input_layout_.GetAddressOf());
    context_->IAGetVertexBuffers(0,1,vertex_buffer_.GetAddressOf(),&vertex_stride_,&vertex_offset_);
    context_->IAGetIndexBuffer(index_buffer_.GetAddressOf(),&index_format_,&index_offset_);
    context_->VSGetShader(vs_.GetAddressOf(), vs_classes_.data(), &vs_count_);
    context_->PSGetShader(ps_.GetAddressOf(), ps_classes_.data(), &ps_count_);
    context_->VSGetConstantBuffers(0,1,vs_constant_.GetAddressOf());
    context_->PSGetShaderResources(0,1,ps_resource_.GetAddressOf());
    context_->PSGetSamplers(0,1,ps_sampler_.GetAddressOf());
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
    context_->IASetInputLayout(input_layout_.Get());
    ID3D11Buffer *vertex=vertex_buffer_.Get();
    context_->IASetVertexBuffers(0,1,&vertex,&vertex_stride_,&vertex_offset_);
    context_->IASetIndexBuffer(index_buffer_.Get(),index_format_,index_offset_);
    context_->VSSetShader(vs_.Get(), vs_classes_.data(), vs_count_);
    context_->PSSetShader(ps_.Get(), ps_classes_.data(), ps_count_);
    ID3D11Buffer *constant=vs_constant_.Get();
    ID3D11ShaderResourceView *resource=ps_resource_.Get();
    ID3D11SamplerState *sampler=ps_sampler_.Get();
    context_->VSSetConstantBuffers(0,1,&constant);
    context_->PSSetShaderResources(0,1,&resource);
    context_->PSSetSamplers(0,1,&sampler);
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
