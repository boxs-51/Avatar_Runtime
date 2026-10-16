#pragma once
#include "../assets/glb_reader.hpp"
namespace ars::validation {
// Complete bounded core checks precede recognized disabled capability dispatch.
bool validate_glb(const Document &, std::span<const std::uint8_t>, assets::GlbData &, assets::Resource &,
                  const assets::Limits &, assets::Work &);
void validate_glb_mappings(Value manifest, const assets::GlbModel &, assets::Work &);
} // namespace ars::validation
