#include "state_scope.hpp"
#ifdef _WIN32
namespace ars::graphics::d3d11 {
StateScope::StateScope(ID3D11DeviceContext *context,std::shared_ptr<ContextOwnership> ownership)
    : operation_(ownership?ownership->nested():ContextOwnership::Operation{}),context_(context) {
    if(operation_.result()!=Result::ok) {context_=nullptr;return;}
    if(!context_ || (ownership && context_!=operation_.context())) {
        result_=Result::invalid_argument;context_=nullptr;return;
    }
    context_->IAGetPrimitiveTopology(&topology_);
    context_->IAGetInputLayout(input_layout_.GetAddressOf());
    context_->IAGetVertexBuffers(0,1,vertex_buffer_.GetAddressOf(),&vertex_stride_,&vertex_offset_);
    context_->IAGetIndexBuffer(index_buffer_.GetAddressOf(),&index_format_,&index_offset_);
    context_->VSGetShader(vs_.GetAddressOf(), vs_classes_.data(), &vs_count_);
    context_->PSGetShader(ps_.GetAddressOf(), ps_classes_.data(), &ps_count_);
    context_->GSGetShader(gs_.GetAddressOf(),gs_classes_.data(),&gs_count_);
    context_->HSGetShader(hs_.GetAddressOf(),hs_classes_.data(),&hs_count_);
    context_->DSGetShader(ds_.GetAddressOf(),ds_classes_.data(),&ds_count_);
    context_->GetPredication(predicate_.GetAddressOf(),&predicate_value_);
    context_->VSGetConstantBuffers(0,1,vs_constant_.GetAddressOf());
    ID3D11ShaderResourceView *resources[2]{};
    ID3D11SamplerState *samplers[2]{};
    context_->PSGetShaderResources(0,2,resources);
    context_->PSGetSamplers(0,2,samplers);
    for (std::size_t i=0;i<2;++i) { ps_resources_[i].Attach(resources[i]); ps_samplers_[i].Attach(samplers[i]); }
    context_->RSGetState(rasterizer_.GetAddressOf());
    context_->RSGetViewports(&viewport_count_, viewports_.data());
    ID3D11RenderTargetView *views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    context_->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, views, dsv_.GetAddressOf());
    for (std::size_t i=0; i<rtvs_.size(); ++i) rtvs_[i].Attach(views[i]);
    context_->OMGetBlendState(blend_.GetAddressOf(), blend_factor_, &sample_mask_);
    context_->OMGetDepthStencilState(depth_.GetAddressOf(), &stencil_ref_);
    // Host stages and predicates must not suppress SDK draw/upload/copy/query
    // commands. The caller keeps this scope through its query End.
    context_->GSSetShader(nullptr,nullptr,0);context_->HSSetShader(nullptr,nullptr,0);context_->DSSetShader(nullptr,nullptr,0);
    context_->SetPredication(nullptr,FALSE);
}
StateScope::~StateScope() {
    if(!context_)return;
    context_->IASetPrimitiveTopology(topology_);
    context_->IASetInputLayout(input_layout_.Get());
    ID3D11Buffer *vertex=vertex_buffer_.Get();
    context_->IASetVertexBuffers(0,1,&vertex,&vertex_stride_,&vertex_offset_);
    context_->IASetIndexBuffer(index_buffer_.Get(),index_format_,index_offset_);
    context_->VSSetShader(vs_.Get(), vs_classes_.data(), vs_count_);
    context_->PSSetShader(ps_.Get(), ps_classes_.data(), ps_count_);
    context_->GSSetShader(gs_.Get(),gs_classes_.data(),gs_count_);
    context_->HSSetShader(hs_.Get(),hs_classes_.data(),hs_count_);
    context_->DSSetShader(ds_.Get(),ds_classes_.data(),ds_count_);
    ID3D11Buffer *constant=vs_constant_.Get();
    ID3D11ShaderResourceView *resources[]{ps_resources_[0].Get(),ps_resources_[1].Get()};
    ID3D11SamplerState *samplers[]{ps_samplers_[0].Get(),ps_samplers_[1].Get()};
    context_->VSSetConstantBuffers(0,1,&constant);
    context_->PSSetShaderResources(0,2,resources);
    context_->PSSetSamplers(0,2,samplers);
    context_->RSSetState(rasterizer_.Get());
    context_->RSSetViewports(viewport_count_, viewports_.data());
    ID3D11RenderTargetView *views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    for (std::size_t i=0; i<rtvs_.size(); ++i) views[i]=rtvs_[i].Get();
    context_->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, views, dsv_.Get());
    context_->OMSetBlendState(blend_.Get(), blend_factor_, sample_mask_);
    context_->OMSetDepthStencilState(depth_.Get(), stencil_ref_);
    context_->SetPredication(predicate_.Get(),predicate_value_);
    for (UINT i=0; i<vs_count_; ++i) if (vs_classes_[i]) vs_classes_[i]->Release();
    for (UINT i=0; i<ps_count_; ++i) if (ps_classes_[i]) ps_classes_[i]->Release();
    for(UINT i=0;i<gs_count_;++i)if(gs_classes_[i])gs_classes_[i]->Release();
    for(UINT i=0;i<hs_count_;++i)if(hs_classes_[i])hs_classes_[i]->Release();
    for(UINT i=0;i<ds_count_;++i)if(ds_classes_[i])ds_classes_[i]->Release();
}
}
#endif
