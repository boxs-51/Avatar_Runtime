#pragma once
#include "owned_bundle.hpp"
namespace ars::assets {
void read_zip(std::span<const std::uint8_t>, OwnedBundle &, Resource &, const Limits &, Work &);
}
