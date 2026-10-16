#include "manifest_validator.hpp"
#include <array>
#include <cmath>
namespace ars::validation {
using assets::fail;
using assets::require;
std::size_t id_index(Value array, std::string_view id, assets::Work &work) {
    for (std::size_t i = 0; i < array.size(); ++i) {
        work.take();
        if (array.at(i).at("id").string() == id)
            return i;
    }
    fail();
}
namespace {
void unique(Value array, assets::Work &work) {
    for (std::size_t i = 0; i < array.size(); ++i) {
        auto id = array.at(i).at("id").string();
        require(!id.empty() && id.size() <= 128 && id.find('\0') == std::string_view::npos);
        for (std::size_t j = 0; j < i; ++j) {
            work.take();
            require(array.at(j).at("id").string() != id);
        }
    }
}
void canonical(std::string_view id) {
    require(!id.empty() && id.size() <= 128 && id.front() >= 'a' && id.front() <= 'z');
    for (char c : id)
        require((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-');
}
void walk(Value layers, std::size_t i, std::array<unsigned char, 128> &color, assets::Work &work) {
    work.take();
    require(color[i] != 1);
    if (color[i] == 2)
        return;
    color[i] = 1;
    for (auto key : {"parent", "mask"})
        if (layers.at(i).has(key)) {
            auto j = id_index(layers, layers.at(i).at(key).string(), work);
            require(j != i);
            walk(layers, j, color, work);
        }
    color[i] = 2;
}
} // namespace
bool validate_manifest(Value m, assets::Work &work) {
    const auto semantics = m.at("rig_semantics"), layers = m.at("visual_layers"),
               reps = m.at("representations");
    require(semantics.size() <= 512 && layers.size() <= 128);
    unique(semantics, work);
    unique(layers, work);
    require(reps.has(m.at("default_mode").string()));
    bool disabled = false;
    for (std::size_t i = 0; i < semantics.size(); ++i) {
        auto value = semantics.at(i);
        canonical(value.at("id").string());
        const auto type = value.at("type").string();
        for (auto key : {"min", "max"})
            if (value.has(key))
                require(std::abs(value.at(key).number()) <= 3.402823466e38);
        if (value.has("min") && value.has("max"))
            require(value.at("min").number() <= value.at("max").number());
        if (type == "trigger") {
            require(!value.has("default"));
            continue;
        }
        require(value.has("default"));
        const auto def = value.at("default");
        if (type == "bool") {
            (void)def.boolean();
            require(!value.has("min") && !value.has("max"));
            continue;
        }
        const auto count = type == "float"  ? 1u
                           : type == "vec2" ? 2u
                           : type == "vec3" ? 3u
                           : type == "quat" ? 4u
                                            : 0u;
        require(count != 0);
        if (count > 1)
            require(def.kind() == Kind::array && def.size() == count);
        double norm = 0;
        for (std::size_t j = 0; j < count; ++j) {
            const auto n = count == 1 ? def.number() : def.at(j).number();
            require(std::abs(n) <= 3.402823466e38);
            if (value.has("min"))
                require(n >= value.at("min").number());
            if (value.has("max"))
                require(n <= value.at("max").number());
            norm += n * n;
        }
        if (type == "quat")
            require(std::abs(norm - 1) <= 0.001);
    }
    std::array<unsigned char, 128> color{};
    for (std::size_t i = 0; i < layers.size(); ++i) {
        walk(layers, i, color, work);
        const auto layer = layers.at(i);
        disabled |= layer.has("mask") || layer.at("mode_scope").string() != "2d" ||
                    (layer.has("blend_mode") && layer.at("blend_mode").string() != "normal");
    }
    for (auto mode : {"2d", "3d"})
        if (reps.has(mode)) {
            const auto mappings = reps.at(mode).at("mappings");
            for (std::size_t i = 0; i < mappings.size(); ++i) {
                work.take();
                const auto mapping = mappings.at(i);
                (void)id_index(semantics, mapping.at("semantic").string(), work);
                require(!mapping.at("target").string().empty());
                for (std::size_t j = 0; j < i; ++j)
                    require(mappings.at(j).at("semantic").string() != mapping.at("semantic").string());
            }
        }
    if (reps.has("3d") && reps.at("3d").at("format").string() == "vrm1")
        disabled = true;
    return disabled;
}
} // namespace ars::validation
