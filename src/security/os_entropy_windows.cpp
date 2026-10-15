#include "os_entropy.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <limits>

namespace ars::security {
std::ptrdiff_t native_entropy_chunk(void *,std::span<std::byte> bytes,int *error) noexcept {
    *error=0;
    if(bytes.size()>std::numeric_limits<ULONG>::max()) return -1;
    if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),
                       BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) return -1;
    return static_cast<std::ptrdiff_t>(bytes.size());
}
ars_status OsEntropy::fill(std::span<std::byte> bytes) noexcept {
    int error=0;
    if(read_(user_,bytes,&error)!=static_cast<std::ptrdiff_t>(bytes.size())) { wipe(bytes); return ARS_E_INTERNAL; }
    return ARS_OK;
}
ars_status native_entropy(void *,std::span<std::byte> bytes) noexcept { OsEntropy entropy; return entropy.fill(bytes); }
}
