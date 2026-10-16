#pragma once
#include "strict_json.hpp"
namespace ars::validation {
bool validate_manifest(Value, assets::Work &);
std::size_t id_index(Value array, std::string_view id, assets::Work &);
} // namespace ars::validation
