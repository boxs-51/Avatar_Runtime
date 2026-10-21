#pragma once
#include "../assets/glb_reader.hpp"
namespace ars::validation {
// Complete bounded core checks precede recognized disabled capability dispatch.
bool validate_glb(const Document &, std::span<const std::uint8_t>, assets::GlbData &, assets::Resource &,
                  const assets::Limits &, assets::Work &);
// Typed-only build: numeric values are decoded raw components (sparse overlays
// already flattened), images are RGBA. No encoded container/image parser runs.
bool validate_typed_glb(const Document &, const assets::GlbData &, assets::GlbData &,
                        assets::Resource &, const assets::Limits &, assets::Work &);
void validate_typed_glb_mappings(Value, const assets::GlbModel &, assets::Work &);
void validate_glb_mappings(Value manifest, const assets::GlbModel &, assets::Work &);
} // namespace ars::validation
