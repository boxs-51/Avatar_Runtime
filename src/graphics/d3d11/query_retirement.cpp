#include "query_retirement.hpp"
#ifdef _WIN32
#include <exception>
namespace ars::graphics::d3d11 {
QueryRetirement::~QueryRetirement() {
    // This private spike has no host device-teardown protocol yet. Fail closed
    // instead of silently releasing a pending/quarantined GPU resource.
    if (std::this_thread::get_id()!=owner_ || (started_ && !complete_)) std::terminate();
}
Result QueryRetirement::begin(ID3D11Device *device, ID3D11DeviceContext *context,
                              ID3D11RenderTargetView *target, std::uint64_t generation,
                              std::shared_ptr<const void> snapshot_pin) {
    if (std::this_thread::get_id()!=owner_) return Result::wrong_thread;
    if (started_) return Result::busy;
    if (!device || !context || !target || !generation) return Result::invalid_argument;
    if (FAILED(device->GetDeviceRemovedReason())) return Result::device_lost;
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
    if (FAILED(device->CreateQuery(&desc, &query))) return Result::failed;
    device_=device; context_=context; target_=target; query_=std::move(query);
    snapshot_pin_=std::move(snapshot_pin); generation_=generation; started_=true;
    context_->End(query_.Get());
    return Result::ok;
}
Result QueryRetirement::poll(std::uint64_t generation) {
    if (std::this_thread::get_id()!=owner_) return Result::wrong_thread;
    if (!started_ || !generation || generation!=generation_) return Result::invalid_argument;
    if (complete_) return Result::ok;
    if (quarantined_ || FAILED(device_->GetDeviceRemovedReason())) {
        quarantined_=true;
        return Result::device_lost;
    }
    BOOL done=FALSE;
    const HRESULT hr=context_->GetData(query_.Get(), &done, sizeof(done), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (hr==S_OK && done) {
        // Exact query and generation completed. Release owned pins on the R thread.
        snapshot_pin_.reset(); target_.Reset(); query_.Reset();
        complete_=true;
        return Result::ok;
    }
    if (hr==S_FALSE) {
        if (!flushed_) { context_->Flush(); flushed_=true; }
        return Result::busy;
    }
    quarantined_=true;
    return FAILED(device_->GetDeviceRemovedReason()) ? Result::device_lost : Result::failed;
}
}
#endif
