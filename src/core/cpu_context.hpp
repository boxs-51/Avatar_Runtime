#pragma once
#include "../assets/validated_cpu_model.hpp"
#include "../security/authority_types.hpp"
#include "../motion/public_resolution.hpp"
#include "../motion/publish_gate.hpp"
#include "../assets/isolated_cpu_load.hpp"
#include "runtime_lifecycle.hpp"
#include <thread>
namespace ars::core {
inline constexpr std::size_t cpu_model_limit=64;
struct CpuContext {
    assets::Resource memory;
    struct Model {
        ars_model_h id{};
        bool live=false;
        std::shared_ptr<const assets::ValidatedCpuModel> value;
    };
    struct Layer { bool visible=true; float opacity=1; std::int32_t z=0; };
    struct Character {
        ars_character_h id{};
        bool live=false;
        std::uint32_t mode=ARS_MODE_AUTO;
        std::shared_ptr<const assets::ValidatedCpuModel> model;
        std::array<Layer,128> layers{};
    };
    std::array<Model,cpu_model_limit> models;
    std::array<Character,security::max_characters> characters;
    struct Source {
        ars_source_admin_h id{};ars_character_h character{};bool live=false;std::int32_t priority=0;
        motion::RetainedInput cache;
    };
    std::array<Source,security::max_source_records> sources;
    std::array<motion::PublicResolution,security::max_characters> resolutions;
    motion::PublicBatch batch;
    std::unique_ptr<assets::AdmissionCustody> quarantine;
    RuntimeLease quarantine_pin;
    std::thread::id simulation;
    std::uint64_t simulation_time=0;
    bool simulation_bound=false,has_time=false;
    CpuContext(ars_host_alloc_fn alloc,ars_host_free_fn free,void *user)
        : memory(alloc,free,user,256*assets::MiB) { memory.reserve(sizeof(CpuContext)); }
    ~CpuContext() { memory.release(sizeof(CpuContext)); }
};
} // namespace ars::core
