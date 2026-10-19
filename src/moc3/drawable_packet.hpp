#pragma once
#include "atlas.hpp"
#include "independent_snapshot.hpp"
#include <iosfwd>
#include <span>
namespace ars::moc3::admission {
inline constexpr std::size_t packet_cap=192ull*1024*1024;
inline constexpr std::uint32_t physics_validated=3;
// Only decode() can mint a nonempty verified scene. No mutable aliases escape.
class VerifiedScene {
    std::shared_ptr<const Snapshot> snapshot_;
    std::shared_ptr<const AtlasSet> atlases_;
    std::uint32_t flags_=0;
    friend bool decode(std::span<const std::uint8_t>,std::uint64_t,VerifiedScene&) noexcept;
public:
    explicit operator bool() const noexcept {return bool(snapshot_);}
    auto snapshot() const noexcept {return snapshot_;}
    auto atlases() const noexcept {return atlases_;}
    auto flags() const noexcept {return flags_;}
};
bool decode(std::span<const std::uint8_t>,std::uint64_t expected_nonce,VerifiedScene&) noexcept;
// Worker-only encoder; host must still independently decode its output.
void encode(std::ostream&,const Snapshot&,const AtlasSet&,std::uint64_t nonce,std::uint32_t flags);
}
