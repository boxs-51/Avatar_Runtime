#pragma once
#include "scene2d.hpp"
#include "scene3d.hpp"
#include "../motion/provenance.hpp"

namespace ars::rendering {
struct LayerControl { bool visible=true;float opacity=1;std::int32_t z=0; };
// CPU projection only. The caller retains the immutable model and originating
// snapshot together for every derived scene and GPU submission.
ars_status prepare_committed_2d(const assets::ValidatedCpuModel &,
    std::span<const motion::Value>,std::span<const LayerControl>,PreparedScene2D &) noexcept;
ars_status prepare_committed_3d(const assets::ValidatedCpuModel &,
    std::span<const motion::Value>,PreparedScene3D &) noexcept;
}
