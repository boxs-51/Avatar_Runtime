#pragma once
#include "asset_limits.hpp"
#include <span>
#include <string>
#include <vector>
namespace ars::assets {
struct Member {
    std::pmr::string name, alias;
    std::pmr::vector<std::uint8_t> bytes;
    std::uint32_t offset = 0, packed = 0, size = 0, crc = 0;
    std::uint16_t method = 0, flags = 0;
    bool directory = false;
    explicit Member(std::pmr::memory_resource *r) : name(r), alias(r), bytes(r) {}
    Member(Member &&) = default;
    Member &operator=(Member &&) = default;
    Member(const Member &) = delete;
};
struct OwnedBundle {
    std::pmr::vector<std::uint8_t> input;
    std::pmr::vector<Member> members;
    explicit OwnedBundle(Resource &r) : input(&r), members(&r) {}
    const Member &find(std::string_view path) const;
};
std::pmr::string canonical_path(std::string_view, std::pmr::memory_resource *, const Limits &);
} // namespace ars::assets
