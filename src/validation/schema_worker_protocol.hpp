#pragma once
#include "ars/ars.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
namespace ars::validation {
inline constexpr std::uint32_t schema_magic = 0x31435341;
inline constexpr std::uint32_t schema_protocol = 1;
inline constexpr std::size_t worker_memory = 128 * 1024 * 1024;
inline constexpr std::size_t max_request = 1024 * 1024;
inline constexpr std::uint32_t max_deadline_ms = 5000;
enum class Schema : std::uint32_t { manifest = 1, rig = 2 };
using Message = std::array<std::uint8_t, 32>;
inline void put(Message &m, std::size_t i, std::uint32_t v) {
    for (std::size_t j = 0; j < 4; ++j)
        m[i + j] = static_cast<std::uint8_t>(v >> (j * 8));
}
inline std::uint32_t get(const Message &m, std::size_t i) {
    std::uint32_t v = 0;
    for (std::size_t j = 0; j < 4; ++j)
        v |= std::uint32_t(m[i + j]) << (j * 8);
    return v;
}
// Correlation/checksum only, not an authentication or cryptographic digest.
inline std::uint32_t checksum(std::string_view s) {
    std::uint32_t h = 2166136261u;
    for (unsigned char c : s)
        h = (h ^ c) * 16777619u;
    return h;
}
inline bool valid_header(const Message &m) {
    return get(m, 0) == schema_magic && get(m, 4) == schema_protocol && get(m, 28) == 0;
}
inline bool valid_reply(const Message &reply, const Message &request) {
    const auto status = get(reply, 8);
    return valid_header(reply) && get(reply, 12) == get(request, 12) && get(reply, 16) == get(request, 16) &&
           get(reply, 20) == get(request, 20) && get(reply, 24) == 0 &&
           (status == ARS_OK || status == ARS_E_BAD_ASSET || status == ARS_E_OUT_OF_BUDGET ||
            status == ARS_E_INTERNAL);
}
} // namespace ars::validation
