#pragma once
#include "runtime_lifecycle.hpp"
#include "../assets/validated_cpu_model.hpp"
#include "../motion/cpu_snapshot.hpp"
namespace ars::core {
// Caller-owned immutable capture. Construct privately, capture once, then share
// as const. This object and its control block must not allocate from the model's
// runtime-owned Resource: runtime custody is released during its destruction.
class RenderFrameLease final {
    RuntimeLease runtime_; // Last to die: model/pose allocator remains alive.
    SnapshotLease pose_;
    std::shared_ptr<const assets::ValidatedCpuModel> model_;
    ars_runtime_h runtime_id_{};
    ars_character_h character_id_{};
    ars_model_h model_id_{};
    std::uint64_t configuration_=0;
    std::uint32_t mode_=0;
public:
    struct Layer {bool visible=true;float opacity=1;std::int32_t z=0;};
private:
    std::array<Layer,128> layers_{};
    std::size_t layer_count_=0;
    friend class RuntimeRouter;
public:
    RenderFrameLease()=default;
    RenderFrameLease(const RenderFrameLease&)=delete;
    RenderFrameLease &operator=(const RenderFrameLease&)=delete;
    bool valid() const noexcept{return bool(runtime_)&&model_&&pose_.get();}
    const auto &model() const noexcept{return model_;}
    const motion::Snapshot *pose() const noexcept{return pose_.get();}
    ars_runtime_h runtime_id() const noexcept{return runtime_id_;}
    ars_character_h character_id() const noexcept{return character_id_;}
    ars_model_h model_id() const noexcept{return model_id_;}
    std::uint64_t configuration() const noexcept{return configuration_;}
    std::uint64_t generation() const noexcept{return pose_.get()?pose_.get()->generation:0;}
    std::uint32_t mode() const noexcept{return mode_;}
    std::span<const Layer> layers() const noexcept{return {layers_.data(),layer_count_};}
};
struct RenderFrameSet {
    std::array<std::shared_ptr<const RenderFrameLease>,16> frames{};
    std::size_t count=0;
};
}
