#include "query_retirement.hpp"
#include "creation_status.hpp"
#ifdef _WIN32
#include <exception>
namespace ars::graphics::d3d11 {
namespace {
class NativeQueryOps final : public QueryOps {
public:
    HRESULT removed_reason(ID3D11Device *device) override { return device->GetDeviceRemovedReason(); }
    HRESULT get_data(ID3D11DeviceContext *context, ID3D11Query *query, BOOL *done) override {
        return context->GetData(query,done,sizeof(*done),D3D11_ASYNC_GETDATA_DONOTFLUSH);
    }
    void flush(ID3D11DeviceContext *context) override { context->Flush(); }
};
NativeQueryOps native_ops;
}
QueryRetirement::QueryRetirement(QueryOps *ops,std::shared_ptr<ContextOwnership> ownership)
    :ownership_(std::move(ownership)),ops_(ops ? ops : &native_ops) {}
QueryRetirement::~QueryRetirement() {
    if (std::this_thread::get_id()!=owner_ || (started_ && !complete_ && !detached_lost_)) std::terminate();
}
Result QueryRetirement::begin(ID3D11Device *device, ID3D11DeviceContext *context,
                              ID3D11RenderTargetView *target, std::uint64_t generation,
                              std::shared_ptr<const void> snapshot_pin) {
    const auto ready=prepare(device,context,target,generation,std::move(snapshot_pin));
    return ready==Result::ok ? end() : ready;
}
Result QueryRetirement::prepare(ID3D11Device *device, ID3D11DeviceContext *context,
                                ID3D11RenderTargetView *target, std::uint64_t generation,
                                std::shared_ptr<const void> snapshot_pin) {
    return prepare_split(device,context,target,generation,std::move(snapshot_pin),{});
}
Result QueryRetirement::prepare_split(ID3D11Device *device,ID3D11DeviceContext *context,
    ID3D11RenderTargetView *target,std::uint64_t generation,std::shared_ptr<const void> snapshot_pin,
    std::shared_ptr<const void> gpu_pin) {
    if (std::this_thread::get_id()!=owner_) return Result::wrong_thread;
    if (started_) return Result::busy;
    if(ownership_) {
        if(!ownership_->owns(device,context,generation) || !snapshot_pin || !gpu_pin)return Result::invalid_argument;
    }
    if (!device || !context || !target || !generation) return Result::invalid_argument;
    if (FAILED(ops_->removed_reason(device))) return Result::device_lost;
    Microsoft::WRL::ComPtr<ID3D11Device> context_device, target_device;
    context->GetDevice(&context_device);
    target->GetDevice(&target_device);
    Microsoft::WRL::ComPtr<IUnknown> expected_id, context_id, target_id;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&expected_id))) ||
        FAILED(context_device.As(&context_id)) || FAILED(target_device.As(&target_id)) ||
        expected_id.Get()!=context_id.Get() || expected_id.Get()!=target_id.Get())
        return Result::invalid_argument;
    D3D11_QUERY_DESC desc{D3D11_QUERY_EVENT, 0};
    Microsoft::WRL::ComPtr<ID3D11Query> query;
    const auto created=ops_->create_query(device,&desc,&query);
    if (FAILED(created)) return creation_status(created,ops_->removed_reason(device));
    if(ownership_){const auto status=ownership_->retain(gpu_owner_);if(status!=Result::ok)return status;}
    device_=device; context_=context; target_=target; query_=std::move(query);
    snapshot_pin_=std::move(snapshot_pin);gpu_pin_=std::move(gpu_pin); generation_=generation; started_=true;
    return Result::ok;
}
Result QueryRetirement::end() {
    if (std::this_thread::get_id()!=owner_) return Result::wrong_thread;
    if(ownership_&&!ownership_->admitted())return Result::busy;
    if (!started_ || ended_ || quarantined_) return Result::invalid_argument;
    context_->End(query_.Get());
    ended_=true;
    return Result::ok;
}
Result QueryRetirement::cancel_unsubmitted() {
    if (std::this_thread::get_id()!=owner_) return Result::wrong_thread;
    if(ownership_&&!ownership_->admitted())return Result::busy;
    if (!started_ || ended_ || quarantined_) return Result::invalid_argument;
    query_.Reset(); target_.Reset(); context_.Reset(); device_.Reset();gpu_pin_.reset();gpu_owner_.reset();snapshot_pin_.reset();
    started_=false;
    return Result::ok;
}
Result QueryRetirement::mark_lost() {
    if (std::this_thread::get_id()!=owner_) return Result::wrong_thread;
    if(ownership_&&!ownership_->admitted())return Result::busy;
    if (!started_ || complete_) return Result::invalid_argument;
    quarantined_=true;
    return Result::device_lost;
}
Result QueryRetirement::poll(std::uint64_t generation) {
    if (std::this_thread::get_id()!=owner_) return Result::wrong_thread;
    if(ownership_&&!ownership_->admitted())return Result::busy;
    if (!started_ || !generation || generation!=generation_) return Result::invalid_argument;
    if (complete_) return Result::ok;
    if (quarantined_ || FAILED(ops_->removed_reason(device_.Get()))) {
        quarantined_=true;
        return Result::device_lost;
    }
    if (!ended_) return Result::invalid_argument;
    BOOL done=FALSE;
    const HRESULT hr=ops_->get_data(context_.Get(),query_.Get(),&done);
    if (hr==S_OK && done) {
        // Exact query and generation completed. Release owned pins on the R thread.
        gpu_pin_.reset();snapshot_pin_.reset(); target_.Reset(); query_.Reset();
        context_.Reset();device_.Reset();gpu_owner_.reset();
        complete_=true;
        return Result::ok;
    }
    if (hr==S_FALSE) {
        if (!flushed_) { ops_->flush(context_.Get()); flushed_=true; }
        return Result::busy;
    }
    quarantined_=true;
    return FAILED(ops_->removed_reason(device_.Get())) ? Result::device_lost : Result::failed;
}
Result QueryRetirement::surrender_lost(std::uint64_t generation) {
    if(std::this_thread::get_id()!=owner_)return Result::wrong_thread;
    if(!ownership_ || !ownership_->admitted() || !ownership_->closing() || !started_ ||
       generation!=generation_ || !quarantined_ || complete_ || surrendered_)return Result::invalid_argument;
    query_.Reset();target_.Reset();context_.Reset();device_.Reset();gpu_pin_.reset();gpu_owner_.reset();
    surrendered_=true;return Result::ok;
}
Result QueryRetirement::detach_lost(std::uint64_t generation) {
    if(ownership_)return Result::invalid_argument;
    return detach_lost_impl(generation);
}
Result QueryRetirement::detach_lost_impl(std::uint64_t generation) {
    if (std::this_thread::get_id()!=owner_) return Result::wrong_thread;
    if (!started_ || !generation || generation!=generation_) return Result::invalid_argument;
    if (!quarantined_ || complete_ || detached_lost_ || (ownership_ && !surrendered_)) return Result::invalid_argument;
    // No completion is asserted. Host has separately stopped use of this device.
    query_.Reset(); target_.Reset(); context_.Reset(); device_.Reset();gpu_pin_.reset();gpu_owner_.reset();snapshot_pin_.reset();
    detached_lost_=true;
    return Result::ok;
}
}
#endif
