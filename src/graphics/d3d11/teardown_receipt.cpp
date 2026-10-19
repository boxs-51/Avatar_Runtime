#include "teardown_receipt.hpp"
#ifdef _WIN32
namespace ars::graphics::d3d11 {
PrivateTeardownHost::PrivateTeardownHost(ID3D11Device *device, ID3D11DeviceContext *context,
                                         ID3D11RenderTargetView *target, std::uint64_t generation)
    : state_(std::make_shared<HostTeardownReceipt::State>(generation,device)),
      device_(device), context_(context), target_(target) {}

Result PrivateTeardownHost::admit_user() {
    if (std::this_thread::get_id()!=state_->owner) return Result::wrong_thread;
    if (state_->closing.load(std::memory_order_acquire) || state_->issued) return Result::busy;
    ++state_->active_users;
    return Result::ok;
}
Result PrivateTeardownHost::release_user() {
    if (std::this_thread::get_id()!=state_->owner) return Result::wrong_thread;
    if (!state_->active_users) return Result::invalid_argument;
    --state_->active_users;
    return Result::ok;
}
Result PrivateTeardownHost::begin_close() noexcept {
    state_->closing.store(true,std::memory_order_release);
    return Result::ok;
}
Result PrivateTeardownHost::issue(HostTeardownReceipt &out) {
    if (std::this_thread::get_id()!=state_->owner) return Result::wrong_thread;
    if (!state_->generation || !device_ || !context_ || !target_ ||
        !state_->closing.load(std::memory_order_acquire) ||
        state_->active_users || state_->issued) return Result::invalid_argument;
    Microsoft::WRL::ComPtr<ID3D11Device> context_device, target_device;
    Microsoft::WRL::ComPtr<IUnknown> expected_id, context_id, target_id;
    context_->GetDevice(&context_device);
    target_->GetDevice(&target_device);
    if (!context_device || !target_device ||
        FAILED(device_.As(&expected_id)) || FAILED(context_device.As(&context_id)) ||
        FAILED(target_device.As(&target_id)) || expected_id.Get()!=context_id.Get() ||
        expected_id.Get()!=target_id.Get()) return Result::invalid_argument;
    context_->ClearState();
    target_.Reset(); context_.Reset(); device_.Reset();
    state_->issued=true;
    out=HostTeardownReceipt(state_);
    return Result::ok;
}
} // namespace ars::graphics::d3d11
#endif
