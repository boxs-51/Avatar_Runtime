#pragma once
#ifdef _WIN32
#include "independent_snapshot.hpp"
#include "../graphics/d3d11/host_device.hpp"
#include "../graphics/d3d11/query_retirement.hpp"

namespace ars::moc3 {
struct Atlas {
    std::uint32_t width=0, height=0;
    std::vector<std::uint8_t> rgba;
};
struct AtlasSet { std::vector<Atlas> images; };
struct View { float sx=1, sy=1, tx=0, ty=0; };
// Render-owner cache for one immutable package/device generation. In-flight
// frames keep their own references even when this cache is replaced/destroyed.
struct AtlasResidency {
    struct State;
    std::shared_ptr<State> state;
};
struct GpuProbe {
    int fail_after=-1;
    std::size_t creations=0, resident_bytes=0, staging_bytes=0;
    bool next() noexcept {
        if (fail_after==0) return false;
        if (fail_after>0) --fail_after;
        ++creations;
        return true;
    }
};

// Private host pass. Snapshot/atlas/pipeline resources are retained by the
// exact query. The caller must retire or detach loss on the render owner.
graphics::Result render_offscreen_moc3(graphics::d3d11::HostDevice&,
    ID3D11RenderTargetView*, graphics::TargetInfo,
    std::shared_ptr<const Snapshot>, std::shared_ptr<const AtlasSet>, View,
    graphics::d3d11::QueryRetirement&, GpuProbe* = nullptr, AtlasResidency* = nullptr);
}
#endif
