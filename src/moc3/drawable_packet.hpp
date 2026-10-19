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
    friend bool decode_frame(std::span<const std::uint8_t>,std::uint64_t,std::uint64_t,std::uint32_t,VerifiedScene&) noexcept;
public:
    explicit operator bool() const noexcept {return bool(snapshot_);}
    auto snapshot() const noexcept {return snapshot_;}
    auto atlases() const noexcept {return atlases_;}
    auto flags() const noexcept {return flags_;}
};
bool decode(std::span<const std::uint8_t>,std::uint64_t expected_nonce,VerifiedScene&) noexcept;
bool validate_frame(std::span<const std::uint8_t>,std::uint64_t,std::uint64_t,std::uint32_t) noexcept;
bool decode_frame(std::span<const std::uint8_t>,std::uint64_t,std::uint64_t,std::uint32_t,VerifiedScene&) noexcept;
class VerifiedSequence {
    std::vector<VerifiedScene> frames_;
    std::vector<std::uint64_t> clocks_;
    friend bool decode_sequence(std::span<const std::uint8_t>,std::uint64_t,VerifiedSequence&) noexcept;
public:
    std::size_t size() const noexcept {return frames_.size();}
    const VerifiedScene& frame(std::size_t i) const {return frames_.at(i);}
    std::uint64_t timestamp(std::size_t i) const {return clocks_.at(i);}
};
bool decode_sequence(std::span<const std::uint8_t>,std::uint64_t,VerifiedSequence&) noexcept;
std::uint64_t encoded_size(const Snapshot&,const AtlasSet&);
void encode_frame(std::ostream&,const Snapshot&,const AtlasSet&,std::uint64_t,std::uint32_t,std::uint32_t version);
// Worker-only encoder; host must still independently decode its output.
void encode(std::ostream&,const Snapshot&,const AtlasSet&,std::uint64_t nonce,std::uint32_t flags);
}
