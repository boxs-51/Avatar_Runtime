#pragma once
#include <cstdint>
#include <vector>
namespace ars::moc3 {
struct Atlas {
    std::uint32_t width=0, height=0;
    std::vector<std::uint8_t> rgba;
};
struct AtlasSet { std::vector<Atlas> images; };
}
