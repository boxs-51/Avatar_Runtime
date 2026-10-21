#pragma once
#include "glb_reader.hpp"
namespace ars::assets::packet {
// Internal full-packet suffix. No digest boundary of its own; the enclosing
// CPU packet authenticates length/identity/hash before reaching this reader.
void write_glb(const GlbModel &, std::pmr::vector<std::uint8_t> &, Work &);
std::shared_ptr<const GlbModel> read_glb(std::span<const std::uint8_t>, std::size_t &,
                                      Resource &, const Limits &, Work &);
} // namespace ars::assets::packet
