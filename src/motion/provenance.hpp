#pragma once
#include "../security/authority_types.hpp"
namespace ars::motion {
inline constexpr std::uint32_t max_dependencies=16,max_nodes=512,max_cells=256,max_triggers=32,max_snapshots=64,max_candidates=8;
struct Key {
    ars_character_h character{};
    ars_source_admin_h source{};
    std::uint64_t epoch=0;
};
bool same(const Key &,const Key &) noexcept;
struct Provenance {
    std::array<Key,max_dependencies> keys{};
    std::uint32_t count=0;
    ars_status include(const Key &,std::uint16_t *) noexcept;
};
struct Value { float scalar=0; std::uint16_t dependencies=0; std::array<float,3> tail{}; };
Value add(Value,Value) noexcept;
Value scale(Value,float) noexcept;
}
