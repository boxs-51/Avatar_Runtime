#pragma once
#include "../assets/validated_cpu_model.hpp"
#include <array>
#include <memory>
#include <vector>

namespace ars::rendering {
// Private, CPU-only M2-C snapshot. The source remains pinned because material
// images are immutable C2-owned values, not copies of untrusted GLB bytes.
struct Vertex3D {
    std::array<float, 3> position{}, normal{};
    std::array<float, 4> tangent{};
    std::array<float, 2> uv{};
};
struct Texture3D {
    std::uint32_t image = assets::glb_none;
    std::uint32_t mag_filter = 9729, min_filter = 9987;
    std::uint32_t wrap_s = 10497, wrap_t = 10497;
};
struct Material3D {
    std::array<float, 4> base_color{1, 1, 1, 1};
    float metallic = 1, roughness = 1, alpha_cutoff = 0.5f;
    Texture3D base_texture, metallic_roughness_texture;
    enum class Alpha { opaque, mask, blend } alpha = Alpha::opaque;
    bool double_sided = false;
};
struct Draw3D {
    std::uint32_t node = 0, primitive = 0;
    Material3D material;
    std::vector<Vertex3D> vertices;
    std::vector<std::uint32_t> indices;
};
struct Camera3D {
    std::array<double,16> clip_from_world{};
    bool front_counter_clockwise = false;
};
struct CameraNode3D {
    std::uint32_t node=0;
    bool perspective=true;
    double yfov=0,aspect=0,znear=0,zfar=0,xmag=0,ymag=0;
    bool infinite_far=false;
    std::array<double,16> world{};
};
struct PreparedScene3D {
    std::shared_ptr<const assets::GlbModel> source;
    std::vector<Draw3D> draws;
    std::vector<CameraNode3D> cameras;
    std::size_t estimated_gpu_bytes = 0;
    std::size_t gpu_budget = 0;
};
struct Scene3DLimits {
    std::size_t gpu_bytes = 256 * assets::MiB;
    std::size_t pose_bytes = 64 * assets::MiB;
};
// Explicit scene selection can override a missing/default glTF scene.
ars_status prepare_static_3d(std::shared_ptr<const assets::GlbModel> source,
    PreparedScene3D &out, std::uint32_t scene = assets::glb_none,
    Scene3DLimits limits = {}) noexcept;
ars_status select_gltf_camera(const PreparedScene3D &scene,std::uint32_t node,
    std::uint32_t width,std::uint32_t height,Camera3D &out) noexcept;
inline ars_status prepare_static_3d(const assets::ValidatedCpuModel &model,
    PreparedScene3D &out, std::uint32_t scene = assets::glb_none,
    Scene3DLimits limits = {}) noexcept {
    return prepare_static_3d(model.glb(), out, scene, limits);
}
} // namespace ars::rendering
