#include "validated_cpu_model.hpp"
#include "../validation/glb_validator.hpp"
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
                                                          Work &work, std::shared_ptr<const GlbModel> glb) {
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
        model->glb_ = std::move(glb);
        model->default_mode_ = manifest.at("default_mode").string() == "3d" ? 1 : 0;
        model->complete_hierarchy_ = true;
        const auto layers = manifest.at("visual_layers"), sem = manifest.at("rig_semantics");
        for (std::size_t i = 0; i < sem.size(); ++i) {
            auto source = sem.at(i);
            model->semantics_.emplace_back(&r);
            auto &s = model->semantics_.back();
            s.id = source.at("id").string();
            s.type = source.at("type").string();
            if (source.has("units"))
                s.units = source.at("units").string();
            s.has_minimum = source.has("min");
            if (s.has_minimum)
                s.minimum = source.at("min").number();
            s.has_maximum = source.has("max");
            if (s.has_maximum)
                s.maximum = source.at("max").number();
            for (auto mode : {"2d", "3d"})
                if (manifest.at("representations").has(mode)) {
                    const auto maps = manifest.at("representations").at(mode).at("mappings");
                    for (std::size_t j = 0; j < maps.size(); ++j) {
                        work.take();
                        if (maps.at(j).at("semantic").string() == s.id) {
                            auto target = maps.at(j).at("target").string();
                            if (std::string_view(mode) == "2d")
                                s.target2d = target;
                            else
                                s.target3d = target;
                        }
                    }
                }
            s.has_default = source.has("default");
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
        std::sort(model->semantics_.begin(), model->semantics_.end(),
                  [](const auto &a, const auto &b) { return a.id < b.id; });
        for (std::size_t i = 0; i < layers.size(); ++i) {
            auto source = layers.at(i);
            model->layers_.emplace_back(&r);
            auto &l = model->layers_.back();
            l.id = source.at("id").string();
            l.z = static_cast<std::int32_t>(source.at("z_index").number());
            l.visible = source.at("visible").boolean();
            l.opacity = f(source.at("opacity"));
            l.local_visible = l.visible; l.local_opacity = l.opacity;
            if (source.has("parent")) l.parent = static_cast<std::uint32_t>(validation::id_index(layers, source.at("parent").string(), work));
            std::size_t current = i;
            for (std::size_t depth = 0; layers.at(current).has("parent"); ++depth) {
                require(depth < 128);
                current = validation::id_index(layers, layers.at(current).at("parent").string(), work);
                auto parent = layers.at(current);
                l.visible &= parent.at("visible").boolean();
                l.opacity *= f(parent.at("opacity"));
            }
        }
        if (manifest.at("representations").has("2d")) {
            model->canvas_ = {rig.at("canvas").at("width").u32(), rig.at("canvas").at("height").u32()};
            const auto parts = rig.at("parts"), params = rig.at("parameters");
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
        }
        return model;
    }
};
ars_status validate_bundle(std::span<const std::uint8_t> input, validation::SchemaClient &schema,
                           Resource &resource, std::shared_ptr<const ValidatedCpuModel> &out,
                           const Limits &limits) noexcept {
    return validate_bundle_checked(input,
        [](void *context, validation::Schema kind, std::string_view text, Resource &r) noexcept {
            return static_cast<validation::SchemaClient *>(context)->validate(kind, text, r);
        }, &schema, resource, out, limits);
}
ars_status validate_bundle_checked(std::span<const std::uint8_t> input, SchemaCheck schema, void *context,
                                   Resource &resource, std::shared_ptr<const ValidatedCpuModel> &out,
                                   const Limits &limits) noexcept {
    out.reset();
    if (!schema)
        return ARS_E_INVALID_ARGUMENT;
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
        auto json_arena = make_glb_arena(resource);
        validation::Document manifest(json_arena->memory()), rig(json_arena->memory());
        const auto &source = bundle.find("character.json");
        validation::parse_json(text(source), manifest, limits, work);
        auto status = schema(context, validation::Schema::manifest, text(source), resource);
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
        Image image(&resource);
        if (reps.has("2d")) {
            const auto rep = reps.at("2d");
            const auto &rig_source = bundle.find(rep.at("asset").string());
            validation::parse_json(text(rig_source), rig, limits, work);
            status = schema(context, validation::Schema::rig, text(rig_source), resource);
            require(status == ARS_OK, status);
            decode_png(bundle.find(rep.at("atlas").string()).bytes, image, resource, limits, work);
            validation::validate_profile_b(m, rig.root(), image.width, image.height, limits, work);
        }
        std::shared_ptr<const GlbModel> glb;
        if (reps.has("3d")) {
            auto remaining = limits;
            remaining.pixels -= image.rgba.size();
            glb = prepare_glb(bundle.find(reps.at("3d").at("asset").string()).bytes, resource, remaining,
                              work, json_arena);
            validation::validate_glb_mappings(m, *glb, work);
            disabled |= glb->requires_disabled();
        }
        require(!disabled, ARS_E_UNSUPPORTED);
        out = ModelBuilder::build(m, reps.has("2d") ? rig.root() : m, std::move(image), resource, work,
                                  std::move(glb));
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
