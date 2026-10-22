#pragma once
#include "validated_cpu_model.hpp"
namespace ars::assets::packet {
inline constexpr std::size_t header_bytes = 96, byte_cap = 16 * MiB;
using Digest = std::array<std::uint8_t, 32>;
struct Identity { std::uint64_t nonce = 0; Digest request{}; };
// Windows SHA256 provider; does not parse an encoded asset.
ars_status digest(std::span<const std::uint8_t>, Digest &) noexcept;
// On every failure, preserve the caller's output. Resource must outlive result.
ars_status encode(const ValidatedCpuModel &, Identity, Resource &,
                  std::pmr::vector<std::uint8_t> &) noexcept;
ars_status encode_full(const ValidatedCpuModel &, Identity, Resource &,
                       std::pmr::vector<std::uint8_t> &) noexcept;
ars_status rejection_full(ars_status, Identity, Resource &, std::pmr::vector<std::uint8_t> &) noexcept;
ars_status rejection(ars_status, Identity, Resource &, std::pmr::vector<std::uint8_t> &) noexcept;
ars_status decode(std::span<const std::uint8_t>, Identity, Resource &,
                  std::shared_ptr<const ValidatedCpuModel> &, const Limits & = {}) noexcept;
} // namespace ars::assets::packet
