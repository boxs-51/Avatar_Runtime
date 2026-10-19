#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
namespace ars::moc3::admission {
using Digest=std::array<std::uint8_t,32>;
inline constexpr std::size_t checkpoint_cap=128*1024;
struct PhysicsCheckpoint {
    std::uint64_t generation=0,clock=0,remainder=0;
    Digest model{};
    std::vector<std::vector<std::array<double,4>>> particles;
};
Digest sha256(std::span<const std::uint8_t>);
Digest package_identity(std::span<const std::uint8_t>,std::span<const std::uint8_t> entry);
bool validate_checkpoint(std::span<const std::uint8_t>,const Digest&,std::uint64_t,std::uint64_t) noexcept;
bool decode_checkpoint(std::span<const std::uint8_t>,const Digest&,std::uint64_t,std::uint64_t,PhysicsCheckpoint&) noexcept;
std::vector<std::uint8_t> encode_checkpoint(const PhysicsCheckpoint&);
}
