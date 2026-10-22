#pragma once
#ifdef _WIN32
#include "../backend.hpp"
#include <dxgi.h>
namespace ars::graphics::d3d11 {
// Native and injected failures share this classification. Removed-device
// evidence takes priority over an allocation or generic failure.
inline Result creation_status(HRESULT hr, HRESULT removed_reason) noexcept {
    if (SUCCEEDED(hr)) return Result::ok;
    if (FAILED(removed_reason) || hr==DXGI_ERROR_DEVICE_REMOVED ||
        hr==DXGI_ERROR_DEVICE_RESET || hr==DXGI_ERROR_DEVICE_HUNG ||
        hr==DXGI_ERROR_DRIVER_INTERNAL_ERROR) return Result::device_lost;
    return hr==E_OUTOFMEMORY?Result::out_of_budget:Result::failed;
}
}
#endif
