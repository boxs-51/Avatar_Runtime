#include "provenance.hpp"
namespace ars::motion {
bool same(const Key &a,const Key &b) noexcept {
    return security::same(a.character.id,b.character.id) && security::same(a.source.id,b.source.id) && a.epoch==b.epoch;
}
ars_status Provenance::include(const Key &key,std::uint16_t *mask) noexcept {
    for(std::uint32_t i=0;i<count;++i) if(same(keys[i],key)) { *mask=static_cast<std::uint16_t>(1U<<i); return ARS_OK; }
    if(count==max_dependencies) return ARS_E_OUT_OF_BUDGET;
    keys[count]=key; *mask=static_cast<std::uint16_t>(1U<<count++); return ARS_OK;
}
Value add(Value a,Value b) noexcept { return {a.scalar+b.scalar,static_cast<std::uint16_t>(a.dependencies|b.dependencies)}; }
Value scale(Value a,float weight) noexcept { return {a.scalar*weight,a.dependencies}; }
}
