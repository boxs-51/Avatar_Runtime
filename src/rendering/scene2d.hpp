#pragma once
#include "../assets/validated_cpu_model.hpp"
#include <span>
#include <vector>

namespace ars::rendering {
// Private M2-A static-geometry snapshot. No GPU resource or public capability.
struct Scene2DInput {
    std::span<const assets::Layer> layers;
    std::span<const assets::Part> parts;
    std::array<std::uint32_t, 2> canvas{};
    const assets::Image *atlas=nullptr;
};
inline Scene2DInput scene_input(const assets::ValidatedCpuModel &model) noexcept {
    return {model.layers(), model.parts(), model.canvas(), &model.image()};
}
struct Scene2DLimits {
    std::size_t parts=128, vertices=2'000'000, triangles=2'000'000;
};
struct DrawPart2D {
    std::uint32_t layer_index=0, part_index=0;
    float opacity=1;
    std::array<std::uint32_t, 4> atlas_rect{};
    std::vector<assets::Pair> vertices, uv;
    std::vector<std::array<std::uint32_t, 3>> triangles;
};
struct PreparedScene2D {
    std::array<std::uint32_t, 2> canvas{};
    std::uint32_t atlas_width=0, atlas_height=0;
    std::vector<DrawPart2D> draws;
};
// Copies the default pose geometry. Caller must retain the separately validated
// atlas pixels for the later GPU upload; this snapshot never invokes D3D11.
ars_status prepare_static_2d(Scene2DInput input, PreparedScene2D &out,
                             Scene2DLimits limits={}) noexcept;
} // namespace ars::rendering
