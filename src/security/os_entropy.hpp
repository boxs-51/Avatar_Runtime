#pragma once
#include "authority_types.hpp"
#include <cstddef>

namespace ars::security {
using EntropyRead=ars_status(*)(void *,std::span<std::byte>) noexcept;
inline void wipe(std::span<std::byte> bytes) noexcept {
    volatile std::byte *ptr=bytes.data();
    for(std::size_t i=0;i<bytes.size();++i) ptr[i]=std::byte{0};
}
ars_status native_entropy(void *,std::span<std::byte>) noexcept;
std::ptrdiff_t native_entropy_chunk(void *,std::span<std::byte>,int *) noexcept;
class OsEntropy {
    using ReadChunk=std::ptrdiff_t(*)(void *,std::span<std::byte>,int *) noexcept;
    ReadChunk read_=native_entropy_chunk;
    void *user_=nullptr;
    friend struct ars::core::ComponentFixture;
public:
    ars_status fill(std::span<std::byte>) noexcept;
};
}
