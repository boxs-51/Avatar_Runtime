#include "validated_cpu_model.hpp"
#include "../validation/profile_b_validator.hpp"
#include "zip_reader.hpp"
#include <algorithm>
namespace ars::assets {
using validation::Kind;
using validation::Value;
namespace {
std::string_view text(const Member &m) {
    return {reinterpret_cast<const char *>(m.bytes.data()), m.bytes.size()};
}
float f(Value value) { return static_cast<float>(value.number()); }
Pair pair(Value value) { return {f(value.at(0)), f(value.at(1))}; }
} // namespace
class ModelBuilder {
  public:
    static std::shared_ptr<const ValidatedCpuModel> build(Value manifest, Value rig, Image image, Resource &r,
                                                          Work &work) {
        auto *memory = r.allocate(sizeof(ValidatedCpuModel), alignof(ValidatedCpuModel));
        auto *raw = new (memory) ValidatedCpuModel(r);
        std::shared_ptr<ValidatedCpuModel> model(
            raw,
            [&r](auto *p) {
                p->~ValidatedCpuModel();
                r.deallocate(p, sizeof(ValidatedCpuModel), alignof(ValidatedCpuModel));
            },
            std::pmr::polymorphic_allocator<std::byte>(&r));
        model->image_ = std::move(image);
        const auto layers = manifest.at("visual_layers"), parts = rig.at("parts"),
                   params = rig.at("parameters"), sem = manifest.at("rig_semantics");
        for (std::size_t i = 0; i < sem.size(); ++i) {
            auto source = sem.at(i);
            model->semantics_.emplace_back(&r);
            auto &s = model->semantics_.back();
            s.id = source.at("id").string();
            s.type = source.at("type").string();
            if (source.has("units"))
                s.units = source.at("units").string();
            if (source.has("default")) {
                auto def = source.at("default");
                if (def.kind() == Kind::number)
                    s.initial[0] = def.number();
                else if (def.kind() == Kind::boolean)
                    s.initial[0] = def.boolean() ? 1 : 0;
                else
                    for (std::size_t j = 0; j < def.size(); ++j)
                        s.initial[j] = def.at(j).number();
            }
        }
        for (std::size_t i = 0; i < layers.size(); ++i) {
            auto source = layers.at(i);
            model->layers_.emplace_back(&r);
            auto &l = model->layers_.back();
            l.id = source.at("id").string();
            l.z = static_cast<std::int32_t>(source.at("z_index").number());
            l.visible = source.at("visible").boolean();
            l.opacity = f(source.at("opacity"));
            std::size_t current = i;
            for (std::size_t depth = 0; layers.at(current).has("parent"); ++depth) {
                require(depth < 128);
                current = validation::id_index(layers, layers.at(current).at("parent").string(), work);
                auto parent = layers.at(current);
                l.visible &= parent.at("visible").boolean();
                l.opacity *= f(parent.at("opacity"));
            }
        }
        for (std::size_t i = 0; i < params.size(); ++i) {
            auto p = params.at(i);
            model->parameters_.emplace_back(&r);
            auto &out = model->parameters_.back();
            out.id = p.at("id").string();
            out.minimum = f(p.at("min"));
            out.maximum = f(p.at("max"));
            out.initial = f(p.at("default"));
        }
        const auto bindings = manifest.at("representations").at("2d").at("layer_bindings");
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const auto part = parts.at(i);
            model->parts_.emplace_back(&r);
            auto &p = model->parts_.back();
            p.id = part.at("id").string();
            p.z = static_cast<std::int32_t>(part.at("z_index").number());
            for (std::size_t k = 0; k < 4; ++k)
                p.rect[k] = part.at("atlas_rect_px").at(k).u32();
            if (part.has("pivot_normalized"))
                p.pivot = pair(part.at("pivot_normalized"));
            for (std::size_t b = 0; b < bindings.size(); ++b) {
                auto binding = bindings.at(b), ids = binding.at("part_ids");
                for (std::size_t k = 0; k < ids.size(); ++k)
                    if (ids.at(k).string() == p.id)
                        p.layer = static_cast<std::uint32_t>(
                            validation::id_index(layers, binding.at("layer_id").string(), work));
            }
            for (std::size_t j = 0; j < part.at("vertices_px").size(); ++j) {
                p.vertices.push_back(pair(part.at("vertices_px").at(j)));
                p.uv.push_back(pair(part.at("uv").at(j)));
            }
            for (std::size_t j = 0; j < part.at("triangles").size(); ++j) {
                auto t = part.at("triangles").at(j);
                p.triangles.push_back({t.at(0).u32(), t.at(1).u32(), t.at(2).u32()});
            }
            if (part.has("deform_targets"))
                for (std::size_t j = 0; j < part.at("deform_targets").size(); ++j) {
                    auto d = part.at("deform_targets").at(j);
                    p.deforms.emplace_back(&r);
                    auto &target = p.deforms.back();
                    target.parameter = static_cast<std::uint32_t>(
                        validation::id_index(params, d.at("parameter").string(), work));
                    for (std::size_t k = 0; k < d.at("vertex_deltas_px").size(); ++k)
                        target.deltas.push_back(pair(d.at("vertex_deltas_px").at(k)));
                }
        }
        return model;
    }
};
ars_status validate_bundle(std::span<const std::uint8_t> input, validation::SchemaClient &schema,
                           Resource &resource, std::shared_ptr<const ValidatedCpuModel> &out,
                           const Limits &limits) noexcept {
    out.reset();
    if (!resource.begin_job())
        return ARS_E_BUSY;
    struct JobGuard {
        Resource &resource;
        ~JobGuard() { resource.end_job(); }
    } job_guard{resource};
    try {
        require(limits.valid(), ARS_E_INVALID_ARGUMENT);
        Work work{limits.work};
        OwnedBundle bundle(resource);
        read_zip(input, bundle, resource, limits, work);
        BoundedMemory json_memory(&resource, 64 * MiB);
        // JSON nodes/keys share bounded blocks; faults occur at block boundaries
        // rather than thousands of host calls for tiny transient allocations.
        std::pmr::monotonic_buffer_resource json_arena(4096, &json_memory);
        validation::Document manifest(&json_arena), rig(&json_arena);
        const auto &source = bundle.find("character.json");
        validation::parse_json(text(source), manifest, limits, work);
        auto status = schema.validate(validation::Schema::manifest, text(source), resource);
        require(status == ARS_OK, status);
        const auto m = manifest.root();
        bool disabled = validation::validate_manifest(m, work);
        const auto reps = m.at("representations");
        // References must exist even for a decoder/capability disabled in C1.
        // Missing cross-file data is malformed, not a feature negotiation result.
        for (auto mode : {"2d", "3d"}) {
            if (reps.has(mode)) {
                (void)bundle.find(reps.at(mode).at("asset").string());
                if (std::string_view(mode) == "2d")
                    (void)bundle.find(reps.at(mode).at("atlas").string());
            }
        }
        if (!reps.has("2d"))
            fail(ARS_E_UNSUPPORTED);
        const auto rep = reps.at("2d");
        const auto &rig_source = bundle.find(rep.at("asset").string());
        validation::parse_json(text(rig_source), rig, limits, work);
        status = schema.validate(validation::Schema::rig, text(rig_source), resource);
        require(status == ARS_OK, status);
        Image image(&resource);
        decode_png(bundle.find(rep.at("atlas").string()).bytes, image, resource, limits, work);
        validation::validate_profile_b(m, rig.root(), image.width, image.height, limits, work);
        // C1 supplies no GLB decoder. Any required 3D representation keeps this
        // component UNSUPPORTED; C2 owns actual GLB corruption/semantic oracles.
        if (reps.has("3d")) {
            (void)bundle.find(reps.at("3d").at("asset").string());
            disabled = true;
        }
        require(!disabled, ARS_E_UNSUPPORTED);
        out = ModelBuilder::build(m, rig.root(), std::move(image), resource, work);
        return ARS_OK;
    } catch (const Failure &error) {
        return error.status;
    } catch (const std::bad_alloc &) {
        return ARS_E_OUT_OF_BUDGET;
    } catch (...) {
        return ARS_E_INTERNAL;
    }
}
} // namespace ars::assets
