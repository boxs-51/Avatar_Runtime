#include "independent_snapshot.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <malloc.h>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>

#include "PurismCore.h"

namespace ars::moc3 {
namespace {

bool add_bytes(std::size_t& total, std::size_t count, std::size_t stride,
               std::size_t limit) noexcept {
    if (stride && count > (limit - total) / stride) return false;
    total += count * stride;
    return true;
}

bool finite_color(const csmVector4& value) noexcept {
    return std::isfinite(value.X) && std::isfinite(value.Y) &&
           std::isfinite(value.Z) && std::isfinite(value.W);
}

bool bounded_id(const char* value, std::size_t& size) noexcept {
    if (!value) return false;
    for (size = 0; size < 256; ++size) {
        if (value[size] == '\0') return size != 0;
    }
    return false;
}

bool inside_root(const std::filesystem::path& root,
                 const std::filesystem::path& child) {
    const auto relative = child.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto& part : relative) {
        if (part == "..") return false;
    }
    return true;
}

} // namespace

struct Session::Impl {
    void* moc_memory = nullptr;
    void* model_memory = nullptr;
    csmModel* model = nullptr;
    std::thread::id owner = std::this_thread::get_id();
    bool valid = true;

    ~Impl() {
        _aligned_free(model_memory);
        _aligned_free(moc_memory);
    }
};

Session::~Session() = default;

Result Session::open(const std::filesystem::path& local_root,
                     const std::filesystem::path& relative_moc,
                     Budget budget, std::unique_ptr<Session>& out) noexcept {
    try {
        if (relative_moc.empty() || relative_moc.is_absolute() ||
            relative_moc.extension() != ".moc3" ||
            budget.max_moc_bytes == 0 || budget.max_model_bytes == 0 ||
            budget.max_snapshot_bytes < sizeof(Snapshot)) return Result::invalid_path;
        std::error_code error;
        const auto root = std::filesystem::canonical(local_root, error);
        if (error || !std::filesystem::is_directory(root)) return Result::invalid_path;
        const auto child = std::filesystem::canonical(root / relative_moc, error);
        if (error || !inside_root(root, child) || !std::filesystem::is_regular_file(child))
            return Result::invalid_path;
        const auto size = std::filesystem::file_size(child, error);
        if (error || size < 4) return Result::invalid_model;
        if (size > budget.max_moc_bytes ||
            size > std::numeric_limits<unsigned int>::max()) return Result::out_of_budget;

        auto candidate = std::unique_ptr<Session>(new Session);
        candidate->budget_ = budget;
        candidate->impl_ = std::make_unique<Impl>();
        candidate->impl_->moc_memory = _aligned_malloc(static_cast<std::size_t>(size), csmAlignofMoc);
        if (!candidate->impl_->moc_memory) return Result::allocation_failed;
        std::ifstream input(child, std::ios::binary);
        if (!input.read(static_cast<char*>(candidate->impl_->moc_memory),
                        static_cast<std::streamsize>(size))) return Result::invalid_model;
        auto* moc_bytes = candidate->impl_->moc_memory;
        const auto moc_size = static_cast<unsigned int>(size);
        if (std::memcmp(moc_bytes, "MOC3", 4) != 0 ||
            csmGetMocVersion(moc_bytes, moc_size) == csmMocVersion_Unknown ||
            !csmHasMocConsistency(moc_bytes, moc_size)) return Result::invalid_model;
        auto* moc = csmReviveMocInPlace(moc_bytes, moc_size);
        if (!moc) return Result::invalid_model;
        const auto model_size = csmGetSizeofModel(moc);
        if (!model_size || model_size > budget.max_model_bytes) return Result::out_of_budget;
        candidate->impl_->model_memory = _aligned_malloc(model_size, csmAlignofModel);
        if (!candidate->impl_->model_memory) return Result::allocation_failed;
        candidate->impl_->model = csmInitializeModelInPlace(moc, candidate->impl_->model_memory,
                                                            model_size);
        if (!candidate->impl_->model) return Result::invalid_model;
        const auto status = candidate->update({});
        if (status != Result::ok) return status;
        out = std::move(candidate);
        return Result::ok;
    } catch (const std::bad_alloc&) {
        return Result::allocation_failed;
    } catch (const std::length_error&) {
        return Result::out_of_budget;
    } catch (const std::filesystem::filesystem_error&) {
        return Result::invalid_path;
    }
}

Result Session::update(const std::vector<Parameter>& parameters) noexcept {
    if (!impl_ || !impl_->valid || !impl_->model) return Result::invalid_model;
    if (std::this_thread::get_id() != impl_->owner) return Result::wrong_thread;
    std::vector<float> previous;
    bool mutated = false;
    auto rollback = [&] {
        if (!mutated) return;
        auto* values = csmGetParameterValues(impl_->model);
        for (std::size_t i = 0; i < previous.size(); ++i) values[i] = previous[i];
        csmUpdateModel(impl_->model);
        if (csmGetLastError(impl_->model) != csmError_NoError) impl_->valid = false;
    };
    try {
        auto* model = impl_->model;
        const auto parameter_count = csmGetParameterCount(model);
        if (parameter_count < 0 || parameter_count > 4096) return Result::out_of_budget;
        auto* values = csmGetParameterValues(model);
        const auto* ids = csmGetParameterIds(model);
        const auto* minimum = csmGetParameterMinimumValues(model);
        const auto* maximum = csmGetParameterMaximumValues(model);
        if (parameter_count && (!values || !ids || !minimum || !maximum))
            return Result::invalid_model;
        if (parameters.size() > static_cast<std::size_t>(parameter_count))
            return Result::invalid_parameter;
        if (parameter_count) previous.assign(values, values + parameter_count);
        std::vector<std::pair<int, float>> changes;
        changes.reserve(parameters.size());
        for (const auto& setting : parameters) {
            if (!std::isfinite(setting.value)) return Result::invalid_parameter;
            int found = -1;
            for (int i = 0; i < parameter_count; ++i) {
                if (ids[i] && setting.id == ids[i]) { found = i; break; }
            }
            if (found < 0 || setting.value < minimum[found] || setting.value > maximum[found])
                return Result::invalid_parameter;
            for (const auto& change : changes) {
                if (change.first == found) return Result::invalid_parameter;
            }
            changes.emplace_back(found, setting.value);
        }
        for (const auto& [index, value] : changes) values[index] = value;
        mutated = true;
        csmUpdateModel(model);
        auto restore = rollback;
        if (csmGetLastError(model) != csmError_NoError) {
            restore();
            return Result::invalid_model;
        }
        const int count = csmGetDrawableCount(model);
        if (count <= 0 || static_cast<std::size_t>(count) > budget_.max_drawables) {
            restore(); return Result::out_of_budget;
        }
        if (csmGetOffscreenCount(model) != 0) {
            restore(); return Result::unsupported_feature;
        }
        const auto* drawable_ids = csmGetDrawableIds(model);
        const auto* vertex_counts = csmGetDrawableVertexCounts(model);
        const auto* index_counts = csmGetDrawableIndexCounts(model);
        const auto* positions = csmGetDrawableVertexPositions(model);
        const auto* uvs = csmGetDrawableVertexUvs(model);
        const auto* indices = csmGetDrawableIndices(model);
        const auto* mask_counts = csmGetDrawableMaskCounts(model);
        const auto* masks = csmGetDrawableMasks(model);
        const auto* textures = csmGetDrawableTextureIndices(model);
        const auto* draw_orders = csmGetDrawableDrawOrders(model);
        const auto* orders = csmGetRenderOrders(model);
        const auto* blends = csmGetDrawableBlendModes(model);
        const auto* flags = csmGetDrawableConstantFlags(model);
        const auto* dynamic_flags = csmGetDrawableDynamicFlags(model);
        const auto* opacities = csmGetDrawableOpacities(model);
        const auto* multiply = csmGetDrawableMultiplyColors(model);
        const auto* screen = csmGetDrawableScreenColors(model);
        if (!drawable_ids || !vertex_counts || !index_counts || !positions || !uvs ||
            !indices || !mask_counts || !masks || !textures || !draw_orders ||
            !orders || !blends || !flags || !dynamic_flags || !opacities ||
            !multiply || !screen) {
            restore(); return Result::invalid_model;
        }
        std::size_t total = sizeof(Snapshot);
        if (!add_bytes(total, count, sizeof(Drawable), budget_.max_snapshot_bytes)) {
            restore(); return Result::out_of_budget;
        }
        std::size_t vertices_total = 0, indices_total = 0, masks_total = 0;
        auto snapshot = std::make_shared<Snapshot>();
        snapshot->generation = generation_ + 1;
        snapshot->drawables.reserve(count);
        std::unordered_set<std::string_view> seen_ids;
        seen_ids.reserve(count);
        for (int i = 0; i < count; ++i) {
            if (fail_after_ == 0) { restore(); return Result::allocation_failed; }
            if (fail_after_ > 0) --fail_after_;
            std::size_t id_size = 0;
            if (!bounded_id(drawable_ids[i], id_size) || vertex_counts[i] < 0 ||
                index_counts[i] < 0 || index_counts[i] % 3 || mask_counts[i] < 0 ||
                textures[i] < 0 ||
                !std::isfinite(opacities[i]) || opacities[i] < 0 || opacities[i] > 1 ||
                !finite_color(multiply[i]) || !finite_color(screen[i])) {
                restore(); return Result::invalid_model;
            }
            if (!seen_ids.emplace(drawable_ids[i], id_size).second) {
                restore(); return Result::invalid_model;
            }
            if (blends[i] != csmColorBlendType_Normal &&
                blends[i] != csmColorBlendType_AddCompatible &&
                blends[i] != csmColorBlendType_MultiplyCompatible) {
                restore(); return Result::unsupported_feature;
            }
            if (flags[i] & ~(csmBlendAdditive | csmBlendMultiplicative |
                             csmIsDoubleSided | csmIsInvertedMask)) {
                restore(); return Result::unsupported_feature;
            }
            if (static_cast<std::size_t>(textures[i]) >= budget_.max_atlases) {
                restore(); return Result::out_of_budget;
            }
            const auto vc = static_cast<std::size_t>(vertex_counts[i]);
            const auto ic = static_cast<std::size_t>(index_counts[i]);
            const auto mc = static_cast<std::size_t>(mask_counts[i]);
            if (vc > budget_.max_vertices - vertices_total ||
                ic > budget_.max_indices - indices_total ||
                mc > budget_.max_masks - masks_total ||
                !add_bytes(total, id_size + 1, 1, budget_.max_snapshot_bytes) ||
                !add_bytes(total, vc, sizeof(Vertex), budget_.max_snapshot_bytes) ||
                !add_bytes(total, ic, sizeof(std::uint16_t), budget_.max_snapshot_bytes) ||
                !add_bytes(total, mc, sizeof(std::uint32_t), budget_.max_snapshot_bytes) ||
                (vc && (!positions[i] || !uvs[i])) ||
                (ic && !indices[i]) || (mc && !masks[i])) {
                restore(); return Result::out_of_budget;
            }
            vertices_total += vc; indices_total += ic; masks_total += mc;
            Drawable drawable;
            drawable.id.assign(drawable_ids[i], id_size);
            drawable.texture_index = textures[i];
            drawable.draw_order = draw_orders[i];
            drawable.render_order = orders[i];
            drawable.blend_mode = blends[i];
            drawable.opacity = opacities[i];
            drawable.inverted_mask = (flags[i] & csmIsInvertedMask) != 0;
            drawable.double_sided = (flags[i] & csmIsDoubleSided) != 0;
            drawable.visible = (dynamic_flags[i] & csmIsVisible) != 0;
            drawable.multiply = {multiply[i].X, multiply[i].Y, multiply[i].Z, multiply[i].W};
            drawable.screen = {screen[i].X, screen[i].Y, screen[i].Z, screen[i].W};
            drawable.vertices.reserve(vc);
            for (std::size_t j = 0; j < vc; ++j) {
                const auto& p = positions[i][j]; const auto& uv = uvs[i][j];
                if (!std::isfinite(p.X) || !std::isfinite(p.Y) ||
                    !std::isfinite(uv.X) || !std::isfinite(uv.Y)) {
                    restore(); return Result::invalid_model;
                }
                drawable.vertices.push_back({p.X, p.Y, uv.X, uv.Y});
            }
            drawable.indices.reserve(ic);
            for (std::size_t j = 0; j < ic; ++j) {
                if (indices[i][j] >= vc) { restore(); return Result::invalid_model; }
                drawable.indices.push_back(indices[i][j]);
            }
            drawable.masks.reserve(mc);
            for (std::size_t j = 0; j < mc; ++j) {
                const int mask = masks[i][j];
                if (mask < 0 || mask >= count) { restore(); return Result::invalid_model; }
                drawable.masks.push_back(static_cast<std::uint32_t>(mask));
            }
            snapshot->drawables.push_back(std::move(drawable));
        }
        generation_ = snapshot->generation;
        current_.store(std::move(snapshot));
        return Result::ok;
    } catch (const std::bad_alloc&) {
        rollback();
        return Result::allocation_failed;
    } catch (const std::length_error&) {
        rollback();
        return Result::out_of_budget;
    }
}

} // namespace ars::moc3
