#pragma once
#include "../validation/schema_worker_client.hpp"
#include "glb_reader.hpp"
#include "png_decoder.hpp"
#include <array>
#include <memory>
namespace ars::assets {
struct Pair {
    float x = 0, y = 0;
};
struct Parameter {
    std::pmr::string id;
    float minimum = 0, maximum = 0, initial = 0;
    explicit Parameter(std::pmr::memory_resource *r) : id(r) {}
};
struct Deform {
    std::uint32_t parameter = 0;
    std::pmr::vector<Pair> deltas;
    explicit Deform(std::pmr::memory_resource *r) : deltas(r) {}
};
struct Part {
    std::pmr::string id;
    std::array<std::uint32_t, 4> rect{};
    Pair pivot{0.5f, 0.5f};
    std::int32_t z = 0;
    std::uint32_t layer = 0;
    std::pmr::vector<Pair> vertices, uv;
    std::pmr::vector<std::array<std::uint32_t, 3>> triangles;
    std::pmr::vector<Deform> deforms;
    explicit Part(std::pmr::memory_resource *r) : id(r), vertices(r), uv(r), triangles(r), deforms(r) {}
};
struct Layer {
    std::pmr::string id;
    std::int32_t z = 0;
    bool visible = true;
    float opacity = 1;
    explicit Layer(std::pmr::memory_resource *r) : id(r) {}
};
struct Semantic {
    std::pmr::string id, type, units, target2d, target3d;
    std::array<double, 4> initial{};
    double minimum = 0, maximum = 0;
    bool has_minimum = false, has_maximum = false, has_default = false;
    explicit Semantic(std::pmr::memory_resource *r) : id(r), type(r), units(r), target2d(r), target3d(r) {}
};
class ModelBuilder;
class ValidatedCpuModel {
    friend class ModelBuilder;
    std::pmr::vector<Part> parts_;
    std::pmr::vector<Parameter> parameters_;
    std::pmr::vector<Layer> layers_;
    std::pmr::vector<Semantic> semantics_;
    Image image_;
    std::array<std::uint32_t, 2> canvas_{};
    std::shared_ptr<const GlbModel> glb_;
    explicit ValidatedCpuModel(Resource &r)
        : parts_(&r), parameters_(&r), layers_(&r), semantics_(&r), image_(&r) {}

  public:
    const auto &parts() const { return parts_; }
    const auto &parameters() const { return parameters_; }
    const auto &layers() const { return layers_; }
    const auto &semantics() const { return semantics_; }
    const Image &image() const { return image_; }
    const auto &canvas() const { return canvas_; }
    const auto &glb() const { return glb_; }
};
// Private CPU ingestion only. No registry handle, authority grant or GPU effect.
ars_status validate_bundle(std::span<const std::uint8_t>, validation::SchemaClient &, Resource &,
                           std::shared_ptr<const ValidatedCpuModel> &, const Limits &limits = {}) noexcept;
} // namespace ars::assets
