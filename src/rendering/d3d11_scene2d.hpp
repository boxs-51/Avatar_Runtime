#pragma once
#ifdef _WIN32
#include "scene2d.hpp"
#include "../graphics/d3d11/host_device.hpp"
#include "../graphics/d3d11/query_retirement.hpp"

namespace ars::rendering {
struct GpuCreateProbe;
// Private offscreen pass. Caller owns R-thread admission/close and polls the
// retirement query to completion before destroying it or the host device.
graphics::Result render_offscreen_2d(graphics::d3d11::HostDevice &backend,
    ID3D11RenderTargetView *target, graphics::TargetInfo info,
    const PreparedScene2D &scene, const assets::Image &atlas,
    graphics::d3d11::QueryRetirement &retirement,
    std::shared_ptr<const void> snapshot_owner = {},bool clear_target=true,GpuCreateProbe *probe=nullptr);
} // namespace ars::rendering
#endif
