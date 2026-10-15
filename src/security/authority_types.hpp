#pragma once
#include "ars/ars.h"
#include <array>
#include <cstdint>
#include <span>

namespace ars::security {
struct SemanticChannel {
    std::array<char,129> id{};
    std::uint32_t kind=ARS_VALUE_SCALAR;
    std::array<float,4> neutral{};
};
}
