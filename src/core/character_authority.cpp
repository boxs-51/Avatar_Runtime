#include "character_authority.hpp"
#include <cmath>
#include <algorithm>
#include <utility>

namespace ars::security {
bool canonical_id(ars_utf8_view id) noexcept {
    if(!id.ptr || !id.byte_count || id.byte_count>128 || id.ptr[0]<'a' || id.ptr[0]>'z') return false;
    for(std::uint32_t i=1;i<id.byte_count;++i) {
        const auto c=id.ptr[i];
        if(!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='.' || c=='-')) return false;
    }
    return true;
}
bool valid_utf8(ars_utf8_view text,std::uint32_t limit) noexcept {
    if(text.byte_count>limit || (text.byte_count && !text.ptr)) return false;
    for(std::uint32_t i=0;i<text.byte_count;) {
        const auto c=static_cast<unsigned char>(text.ptr[i++]);
        if(!c) return false;
        if(c<128) continue;
        unsigned n=0; std::uint32_t value=0,minimum=0;
        if(c>=0xc2 && c<=0xdf) { n=1; value=c&31U; minimum=128; }
        else if(c>=0xe0 && c<=0xef) { n=2; value=c&15U; minimum=2048; }
        else if(c>=0xf0 && c<=0xf4) { n=3; value=c&7U; minimum=65536; }
        else return false;
        if(n>text.byte_count-i) return false;
        for(unsigned j=0;j<n;++j) {
            const auto tail=static_cast<unsigned char>(text.ptr[i++]);
            if((tail&0xc0)!=0x80) return false;
            value=(value<<6)|(tail&63U);
        }
        if(value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    }
    return true;
}
bool valid_value(std::uint32_t kind,std::span<const float,4> value) noexcept {
    if(kind<ARS_VALUE_SCALAR || kind>ARS_VALUE_TRIGGER) return false;
    for(float component:value) if(!std::isfinite(component)) return false;
    if((kind==ARS_VALUE_BOOL || kind==ARS_VALUE_TRIGGER) && value[0]!=0 && value[0]!=1) return false;
    if(kind==ARS_VALUE_QUAT_XYZW) {
        double norm=0; for(float v:value) norm+=static_cast<double>(v)*v;
        if(std::abs(norm-1)>0.001) return false;
    }
    return true;
}
}
namespace ars::core {
ObjectLease::ObjectLease(ObjectLease &&other) noexcept { *this=std::move(other); }
ObjectLease &ObjectLease::operator=(ObjectLease &&other) noexcept {
    if(this!=&other) { reset(); registry_=std::move(other.registry_); users_=std::exchange(other.users_,nullptr); }
    return *this;
}
ObjectLease::~ObjectLease() { reset(); }
void ObjectLease::reset() noexcept {
    registry_.reset();
    if(auto *users=std::exchange(users_,nullptr)) users->fetch_sub(1,std::memory_order_acq_rel);
}
ars_status validate_catalog(std::span<const security::SemanticChannel> catalog) noexcept {
    if(catalog.empty() || catalog.size()>security::max_channels) return ARS_E_INVALID_ARGUMENT;
    for(std::size_t i=0;i<catalog.size();++i) {
        const auto &channel=catalog[i];
        const auto end=std::find(channel.id.begin(),channel.id.end(),'\0');
        if(end==channel.id.end()) return ARS_E_INVALID_ARGUMENT;
        if(!security::canonical_id({channel.id.data(),static_cast<std::uint32_t>(end-channel.id.begin())}) ||
           !security::valid_value(channel.kind,channel.neutral)) return ARS_E_INVALID_ARGUMENT;
        for(std::size_t j=0;j<i;++j) if(std::strcmp(channel.id.data(),catalog[j].id.data())==0) return ARS_E_INVALID_ARGUMENT;
    }
    return ARS_OK;
}
}
