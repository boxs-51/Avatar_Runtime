#include "scene2d.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <tuple>

namespace ars::rendering {
ars_status prepare_static_2d(Scene2DInput input, PreparedScene2D &out,
                             Scene2DLimits limits) noexcept {
    if (!input.canvas[0] || !input.canvas[1] || !input.atlas ||
        !input.atlas->width || !input.atlas->height || input.parts.empty())
        return ARS_E_UNSUPPORTED;
    if (input.atlas->width>assets::Limits{}.edge ||
        input.atlas->height>assets::Limits{}.edge ||
        std::uint64_t(input.atlas->width)*input.atlas->height*4 != input.atlas->rgba.size())
        return ARS_E_INVALID_ARGUMENT;
    if (input.layers.size()>limits.parts || input.parts.size()>limits.parts)
        return ARS_E_OUT_OF_BUDGET;
    try {
        std::vector<std::size_t> order(input.parts.size());
        std::iota(order.begin(),order.end(),0);
        std::size_t visible_vertices=0, visible_triangles=0, visible_parts=0;
        for (const auto &layer : input.layers)
            if (!std::isfinite(layer.opacity) || layer.opacity<0 || layer.opacity>1)
                return ARS_E_INVALID_ARGUMENT;
        for (const auto &part : input.parts) {
            if (part.layer>=input.layers.size() || part.uv.size()!=part.vertices.size())
                return ARS_E_INVALID_ARGUMENT;
            const auto &layer=input.layers[part.layer];
            if (!layer.visible || layer.opacity==0) continue;
            const auto &rect=part.rect;
            if (!rect[2] || !rect[3] || rect[0]>input.atlas->width ||
                rect[1]>input.atlas->height || rect[2]>input.atlas->width-rect[0] ||
                rect[3]>input.atlas->height-rect[1]) return ARS_E_INVALID_ARGUMENT;
            if (part.vertices.size()>limits.vertices-visible_vertices ||
                part.triangles.size()>limits.triangles-visible_triangles ||
                visible_parts==limits.parts) return ARS_E_OUT_OF_BUDGET;
            visible_vertices+=part.vertices.size();
            visible_triangles+=part.triangles.size();
            ++visible_parts;
            for (const auto &v : part.vertices)
                if (!std::isfinite(v.x) || !std::isfinite(v.y)) return ARS_E_INVALID_ARGUMENT;
            for (const auto &uv : part.uv)
                if (!std::isfinite(uv.x) || !std::isfinite(uv.y)) return ARS_E_INVALID_ARGUMENT;
            for (const auto &tri : part.triangles)
                if (tri[0]>=part.vertices.size() || tri[1]>=part.vertices.size() ||
                    tri[2]>=part.vertices.size()) return ARS_E_INVALID_ARGUMENT;
        }
        std::sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b) {
            const auto &pa=input.parts[a], &pb=input.parts[b];
            return std::tuple{input.layers[pa.layer].z,pa.layer,pa.z,a} <
                   std::tuple{input.layers[pb.layer].z,pb.layer,pb.z,b};
        });
        PreparedScene2D next;
        next.canvas=input.canvas;
        next.atlas_width=input.atlas->width;
        next.atlas_height=input.atlas->height;
        next.draws.reserve(visible_parts);
        for (auto index : order) {
            const auto &part=input.parts[index];
            const auto &layer=input.layers[part.layer];
            if (!layer.visible || layer.opacity==0) continue;
            auto &draw=next.draws.emplace_back();
            draw.layer_index=part.layer;
            draw.part_index=static_cast<std::uint32_t>(index);
            draw.opacity=layer.opacity;
            draw.atlas_rect=part.rect;
            draw.vertices.assign(part.vertices.begin(),part.vertices.end());
            draw.uv.assign(part.uv.begin(),part.uv.end());
            draw.triangles.assign(part.triangles.begin(),part.triangles.end());
        }
        out.draws.swap(next.draws);
        out.canvas=next.canvas;
        out.atlas_width=next.atlas_width;
        out.atlas_height=next.atlas_height;
        return ARS_OK;
    } catch (const std::bad_alloc &) {
        return ARS_E_OUT_OF_BUDGET;
    } catch (...) {
        return ARS_E_INTERNAL;
    }
}
} // namespace ars::rendering
