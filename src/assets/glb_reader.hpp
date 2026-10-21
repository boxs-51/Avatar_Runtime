#pragma once
#include "../validation/strict_json.hpp"
#include "asset_limits.hpp"
#include "png_decoder.hpp"
#include <array>
#include <memory>
#include <span>
namespace ars::assets {
inline constexpr std::uint32_t glb_none = UINT32_MAX;
struct GlbView {
    std::size_t offset = 0, length = 0, stride = 0;
    std::uint32_t target = 0;
};
struct GlbAccessor {
    std::uint32_t count = 0, component = 0, components = 0, rows = 0, columns = 0, view = glb_none;
    std::size_t offset = 0;
    bool normalized = false;
    std::pmr::vector<double> values;
    explicit GlbAccessor(std::pmr::memory_resource *r) : values(r) {}
};
struct GlbMorph {
    std::uint32_t position = glb_none, normal = glb_none, tangent = glb_none;
};
struct GlbPrimitive {
    std::uint32_t position = glb_none, normal = glb_none, tangent = glb_none, uv = glb_none;
    std::uint32_t color = glb_none, joints = glb_none, weights = glb_none, material = glb_none, mode = 4;
    std::pmr::vector<std::uint32_t> indices;
    std::pmr::vector<GlbMorph> morphs;
    explicit GlbPrimitive(std::pmr::memory_resource *r) : indices(r), morphs(r) {}
};
struct GlbMesh {
    std::pmr::string name;
    std::pmr::vector<GlbPrimitive> primitives;
    std::pmr::vector<float> weights;
    std::pmr::vector<std::pmr::string> target_names;
    explicit GlbMesh(std::pmr::memory_resource *r) : name(r), primitives(r), weights(r), target_names(r) {}
};
struct GlbNode {
    std::pmr::string name;
    std::pmr::vector<std::uint32_t> children;
    std::pmr::vector<float> weights;
    std::uint32_t mesh = glb_none, skin = glb_none;
    bool has_matrix = false;
    std::array<double, 16> matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::array<double, 3> translation{}, scale{1, 1, 1};
    std::array<double, 4> rotation{0, 0, 0, 1};
    explicit GlbNode(std::pmr::memory_resource *r) : name(r), children(r), weights(r) {}
};
struct GlbSkin {
    std::pmr::string name;
    std::pmr::vector<std::uint32_t> joints;
    std::uint32_t inverse_bind = glb_none, skeleton = glb_none;
    explicit GlbSkin(std::pmr::memory_resource *r) : name(r), joints(r) {}
};
struct GlbAnimationSampler {
    std::uint32_t input = 0, output = 0, interpolation = 0;
};
struct GlbAnimationChannel {
    std::uint32_t sampler = 0, node = 0, path = 0;
};
struct GlbAnimation {
    std::pmr::string name;
    std::pmr::vector<GlbAnimationSampler> samplers;
    std::pmr::vector<GlbAnimationChannel> channels;
    explicit GlbAnimation(std::pmr::memory_resource *r) : name(r), samplers(r), channels(r) {}
};
struct GlbScene {
    std::pmr::vector<std::uint32_t> roots;
    explicit GlbScene(std::pmr::memory_resource *r) : roots(r) {}
};
struct GlbData {
    std::pmr::vector<GlbView> views;
    std::pmr::vector<GlbAccessor> accessors;
    std::pmr::vector<GlbMesh> meshes;
    std::pmr::vector<GlbNode> nodes;
    std::pmr::vector<GlbSkin> skins;
    std::pmr::vector<GlbAnimation> animations;
    std::pmr::vector<GlbScene> scenes;
    std::pmr::vector<Image> images;
    std::uint32_t scene = glb_none;
    explicit GlbData(std::pmr::memory_resource *r)
        : views(r), accessors(r), meshes(r), nodes(r), skins(r), animations(r), scenes(r), images(r) {}
};
// One shared, host-accounted 64 MiB JSON arena across manifest, rig and GLB.
// A 3D result retains it so no published metadata borrows transaction storage.
class GlbParseArena {
    Resource *owner_;
    BoundedMemory bounded_;
    std::pmr::monotonic_buffer_resource arena_;

  public:
    explicit GlbParseArena(Resource &r) : owner_(&r), bounded_(&r, 64 * MiB), arena_(4096, &bounded_) {}
    auto *memory() { return &arena_; }
    auto *owner() const { return owner_; }
};
std::shared_ptr<GlbParseArena> make_glb_arena(Resource &);
class GlbBuilder;
class GlbModel {
    friend class GlbBuilder;
    friend class GlbPacketBuilder;
    std::shared_ptr<GlbParseArena> arena_;
    validation::Document json_;
    GlbData data_;
    bool disabled_ = false;
    GlbModel(Resource &r, std::shared_ptr<GlbParseArena> arena)
        : arena_(std::move(arena)), json_(arena_->memory()), data_(&r) {}

  public:
    GlbModel(const GlbModel &) = delete;
    GlbModel &operator=(const GlbModel &) = delete;
    const GlbData &data() const { return data_; }
    validation::Value metadata() const { return json_.root(); }
    const validation::Document &metadata_document() const { return json_; }
    bool requires_disabled() const { return disabled_; }
};
// Transaction-local core validation; caller must finish cross-file validation
// and reject requires_disabled() before publishing an enclosing CPU model.
std::shared_ptr<const GlbModel> prepare_glb(std::span<const std::uint8_t>, Resource &, const Limits &, Work &,
                                            std::shared_ptr<GlbParseArena> = {});
// Private CPU-only synchronous ingestion. Result owns every published value.
ars_status ingest_glb(std::span<const std::uint8_t>, Resource &, std::shared_ptr<const GlbModel> &,
                      const Limits & = {}) noexcept;
} // namespace ars::assets
