#pragma once
#ifdef _WIN32
#include "scene3d.hpp"
#include "../graphics/d3d11/host_device.hpp"
#include "../graphics/d3d11/query_retirement.hpp"

namespace ars::rendering {
// Camera3D is a column-major, right-handed world-to-D3D-clip matrix. The caller
// can select a validated glTF camera or provide a private host camera.
graphics::Result render_offscreen_3d(graphics::d3d11::HostDevice &backend,
    ID3D11RenderTargetView *target, graphics::TargetInfo info,
    const PreparedScene3D &scene, const Camera3D &camera,
    graphics::d3d11::QueryRetirement &retirement,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> *depth_capture = nullptr);
} // namespace ars::rendering
#endif
