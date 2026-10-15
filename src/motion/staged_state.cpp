#include "staged_state.hpp"
namespace ars::motion {
void StagedState::purge(std::uint16_t invalid) noexcept {
    for(auto &cell:cells) if(cell.dependencies&invalid) cell={};
    std::uint32_t kept=0;
    for(std::uint32_t i=0;i<trigger_count;++i) if(!(triggers[i].value.dependencies&invalid)) triggers[kept++]=triggers[i];
    trigger_count=kept; compact();
}
void StagedState::compact() noexcept {
    std::uint16_t used=0;
    for(auto cell:cells) used=static_cast<std::uint16_t>(used|cell.dependencies);
    for(std::uint32_t i=0;i<trigger_count;++i) used=static_cast<std::uint16_t>(used|triggers[i].value.dependencies);
    Provenance next; std::array<std::uint16_t,max_dependencies> remap{};
    for(std::uint32_t i=0;i<provenance.count;++i) if(used&(1U<<i)) (void)next.include(provenance.keys[i],&remap[i]);
    auto convert=[&](std::uint16_t old) { std::uint16_t mask=0; for(std::uint32_t i=0;i<provenance.count;++i) if(old&(1U<<i)) mask=static_cast<std::uint16_t>(mask|remap[i]); return mask; };
    for(auto &cell:cells) cell.dependencies=convert(cell.dependencies);
    for(std::uint32_t i=0;i<trigger_count;++i) triggers[i].value.dependencies=convert(triggers[i].value.dependencies);
    provenance=next;
}
}
