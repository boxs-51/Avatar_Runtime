#include "profile_b_validator.hpp"
#include <array>
#include <cmath>
namespace ars::validation {
using assets::require;
void validate_profile_b(Value m, Value rig, std::uint32_t width, std::uint32_t height,
                        const assets::Limits &limits, assets::Work &work) {
    const auto params = rig.at("parameters"), parts = rig.at("parts"), layers = m.at("visual_layers"),
               rep = m.at("representations").at("2d"), sem = m.at("rig_semantics");
    for (auto array : {params, parts})
        for (std::size_t i = 0; i < array.size(); ++i) {
            require(array.at(i).at("id").string().find('\0') == std::string_view::npos);
            for (std::size_t j = 0; j < i; ++j) {
                work.take();
                require(array.at(i).at("id").string() != array.at(j).at("id").string());
            }
        }
    std::array<bool, 128> seen_layers{}, seen_parts{};
    const auto bindings = rep.at("layer_bindings");
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        const auto binding = bindings.at(i);
        const auto l = id_index(layers, binding.at("layer_id").string(), work);
        require(!seen_layers[l]);
        seen_layers[l] = true;
        const auto mode = layers.at(l).at("mode_scope").string();
        require(mode == "2d" || mode == "both");
        const auto ids = binding.at("part_ids");
        for (std::size_t j = 0; j < ids.size(); ++j) {
            const auto p = id_index(parts, ids.at(j).string(), work);
            require(!seen_parts[p]);
            seen_parts[p] = true;
        }
    }
    for (std::size_t i = 0; i < parts.size(); ++i)
        require(seen_parts[i]);
    for (std::size_t i = 0; i < layers.size(); ++i)
        require(seen_layers[i] == (layers.at(i).at("mode_scope").string() != "3d"));
    for (std::size_t i = 0; i < params.size(); ++i) {
        auto p = params.at(i);
        for (auto key : {"min", "max", "default"})
            require(std::abs(p.at(key).number()) <= 3.402823466e38);
        require(p.at("min").number() <= p.at("default").number() &&
                p.at("default").number() <= p.at("max").number());
    }
    const auto maps = rep.at("mappings");
    for (std::size_t i = 0; i < maps.size(); ++i) {
        const auto map = maps.at(i), s = sem.at(id_index(sem, map.at("semantic").string(), work)),
                   p = params.at(id_index(params, map.at("target").string(), work));
        require(s.at("type").string() == "float");
        require(s.at("default").number() == p.at("default").number());
        if (s.has("min"))
            require(s.at("min").number() == p.at("min").number());
        if (s.has("max"))
            require(s.at("max").number() == p.at("max").number());
    }
    std::size_t vertices = 0;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto part = parts.at(i), rect = part.at("atlas_rect_px"), v = part.at("vertices_px"),
                   uv = part.at("uv"), tri = part.at("triangles");
        const auto x = rect.at(0).u32(), y = rect.at(1).u32(), w = rect.at(2).u32(), h = rect.at(3).u32();
        require(w && h && x <= width && y <= height && w <= width - x && h <= height - y);
        require(v.size() <= limits.vertices - vertices, ARS_E_OUT_OF_BUDGET);
        vertices += v.size();
        require(uv.size() == v.size());
        for (auto array : {v, uv})
            for (std::size_t j = 0; j < array.size(); ++j)
                for (std::size_t k = 0; k < 2; ++k) {
                    work.take();
                    require(std::abs(array.at(j).at(k).number()) <= 3.402823466e38);
                }
        if (part.has("pivot_normalized"))
            for (std::size_t k = 0; k < 2; ++k) {
                const auto n = part.at("pivot_normalized").at(k).number();
                require(n >= 0 && n <= 1);
            }
        for (std::size_t j = 0; j < tri.size(); ++j)
            for (std::size_t k = 0; k < 3; ++k) {
                work.take();
                require(tri.at(j).at(k).u32() < v.size());
            }
        if (part.has("deform_targets")) {
            const auto targets = part.at("deform_targets");
            for (std::size_t j = 0; j < targets.size(); ++j) {
                const auto target = targets.at(j);
                (void)id_index(params, target.at("parameter").string(), work);
                for (std::size_t prior = 0; prior < j; ++prior)
                    require(targets.at(prior).at("parameter").string() != target.at("parameter").string());
                const auto deltas = target.at("vertex_deltas_px");
                require(deltas.size() == v.size());
                for (std::size_t d = 0; d < deltas.size(); ++d)
                    for (std::size_t k = 0; k < 2; ++k) {
                        work.take();
                        require(std::abs(deltas.at(d).at(k).number()) <= 3.402823466e38);
                    }
            }
        }
    }
}
} // namespace ars::validation
