#pragma once

// Optional private MOC3 evaluation boundary. No public ARS ABI or asset admission.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ars::moc3 {

enum class Result {
    ok,
    invalid_path,
    invalid_model,
    invalid_parameter,
    unsupported_feature,
    out_of_budget,
    allocation_failed,
    wrong_thread,
};

struct Budget {
    std::size_t max_moc_bytes = 16u * 1024u * 1024u;
    std::size_t max_model_bytes = 64u * 1024u * 1024u;
    std::size_t max_snapshot_bytes = 128u * 1024u * 1024u;
    std::size_t max_drawables = 100000;
    std::size_t max_vertices = 2000000;
    std::size_t max_indices = 12000000;
    std::size_t max_masks = 2000000;
    std::size_t max_atlases = 32;
};

struct Vertex {
    float x = 0, y = 0, u = 0, v = 0;
    bool operator==(const Vertex&) const = default;
};

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    bool operator==(const Color&) const = default;
};

struct Drawable {
    std::string id;
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<std::uint32_t> masks;
    std::int32_t texture_index = -1;
    std::int32_t draw_order = 0;
    std::int32_t render_order = 0;
    std::int32_t blend_mode = 0;
    float opacity = 1;
    bool inverted_mask = false;
    bool double_sided = false;
    bool visible = true;
    Color multiply;
    Color screen;
    bool operator==(const Drawable&) const = default;
};

struct Snapshot {
    std::uint64_t generation = 0;
    std::vector<Drawable> drawables;
};

struct Parameter {
    std::string id;
    float value = 0;
};
struct ParameterInfo {
    std::string id;
    float minimum=0, default_value=0, maximum=0;
};

class Session {
public:
    static Result open(const std::filesystem::path& local_root,
                       const std::filesystem::path& relative_moc,
                       Budget budget, std::unique_ptr<Session>& out) noexcept;
    // Copies exactly the admitted blob. No filesystem access. The atlas count
    // is the actual immutable package size, not merely a resource ceiling.
    static Result open_blob(std::span<const std::uint8_t> moc,
                            std::size_t texture_count, Budget budget,
                            std::unique_ptr<Session>& out,
                            int fail_checkpoint = -1) noexcept;
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Result update(const std::vector<Parameter>& parameters) noexcept;
    // Complete pose: reset unspecified parameters to model defaults in the
    // same transactional update as the supplied finite overrides.
    Result update_complete(const std::vector<Parameter>& overrides) noexcept;
    Result parameter_info(std::vector<ParameterInfo>& out) const noexcept;
    std::shared_ptr<const Snapshot> current() const noexcept { return current_.load(); }
    // Experimental diagnostic only; no public ABI or model data exposure.
    const char* last_failure_stage() const noexcept { return last_failure_stage_; }

    // Deterministic private fault injection; -1 disables it.
    void fail_snapshot_checkpoint_after(int count) noexcept { fail_after_ = count; }

private:
    Session() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Budget budget_;
    std::atomic<std::shared_ptr<const Snapshot>> current_;
    std::uint64_t generation_ = 0;
    int fail_after_ = -1;
    const char* last_failure_stage_ = "none";
};

} // namespace ars::moc3
