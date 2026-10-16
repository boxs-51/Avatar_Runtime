#pragma once
#include "owned_bundle.hpp"
namespace ars::assets {
struct Image {
    std::uint32_t width = 0, height = 0;
    std::pmr::vector<std::uint8_t> rgba;
    explicit Image(std::pmr::memory_resource *r) : rgba(r) {}
    Image(Image &&) = default;
    Image &operator=(Image &&) = default;
    Image(const Image &) = delete;
};
void decode_png(std::span<const std::uint8_t>, Image &, Resource &, const Limits &, Work &);
} // namespace ars::assets
