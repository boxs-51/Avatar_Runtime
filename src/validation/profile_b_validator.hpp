#pragma once
#include "manifest_validator.hpp"
namespace ars::validation {
void validate_profile_b(Value manifest, Value rig, std::uint32_t width, std::uint32_t height,
                        const assets::Limits &, assets::Work &);
}
