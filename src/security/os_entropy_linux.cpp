#include "os_entropy.hpp"
#include <cerrno>
#include <sys/random.h>

namespace ars::security {
std::ptrdiff_t native_entropy_chunk(void *,std::span<std::byte> bytes,int *error) noexcept {
    const auto count=getrandom(bytes.data(),bytes.size(),GRND_NONBLOCK);
    *error=count<0?errno:0; return count;
}
ars_status OsEntropy::fill(std::span<std::byte> bytes) noexcept {
    std::size_t offset=0;
    for(unsigned attempt=0;attempt<8 && offset<bytes.size();++attempt) {
        int error=0;
        const auto count=read_(user_,bytes.subspan(offset),&error);
        if(count>0 && static_cast<std::size_t>(count)<=bytes.size()-offset) offset+=static_cast<std::size_t>(count);
        else if(count<0 && error==EINTR) continue;
        else { wipe(bytes); return ARS_E_INTERNAL; }
    }
    if(offset!=bytes.size()) { wipe(bytes); return ARS_E_INTERNAL; }
    return ARS_OK;
}
ars_status native_entropy(void *,std::span<std::byte> bytes) noexcept { OsEntropy entropy; return entropy.fill(bytes); }
}
