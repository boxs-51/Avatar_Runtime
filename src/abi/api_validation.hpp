#pragma once
#include "ars/ars.h"
#include <cstring>

namespace ars::abi {
template<class T> bool prefix(const T *p) {
    if(!p) return false;
    uint32_t size=0,version=0;
    std::memcpy(&size,p,sizeof(size)); if(size<sizeof(T)) return false;
    const auto *bytes=static_cast<const unsigned char *>(static_cast<const void *>(p));
    std::memcpy(&version,bytes+sizeof(size),sizeof(version));
    return (version>>16)==ARS_ABI_MAJOR;
}
template<class T> void clear(T *p) { if(p) std::memset(p,0,sizeof(T)); }
inline bool valid(const ars_runtime_desc *p) { return prefix(p) && !p->reserved_flags && !p->reserved0; }
inline bool valid(const ars_asset_source *p) { return prefix(p) && !p->reserved_flags; }
inline bool valid(const ars_character_desc *p) { return prefix(p) && !p->reserved_flags; }
inline bool valid(const ars_layer_change *p) { return prefix(p) && !p->reserved_flags; }
inline bool valid(const ars_source_desc *p) { return prefix(p) && !p->reserved_flags && !p->reserved0; }
inline bool valid(const ars_motion_frame *p) { return prefix(p) && !p->reserved_flags; }
inline bool valid(const ars_render_target *p) { return prefix(p) && !p->reserved_flags && !p->reserved0; }
}
