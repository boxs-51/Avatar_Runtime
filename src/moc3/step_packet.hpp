#pragma once
#include "drawable_packet.hpp"
#include "physics_checkpoint.hpp"
namespace ars::moc3::admission {
class VerifiedStep {
    VerifiedScene scene_;
    std::shared_ptr<const PhysicsCheckpoint> state_;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
    friend bool decode_step(std::span<const std::uint8_t>,std::uint64_t,std::uint64_t,std::uint64_t,const Digest&,const Digest&,VerifiedStep&) noexcept;
public:
    const auto& scene() const noexcept {return scene_;}
    auto checkpoint() const noexcept {return state_;}
    auto checkpoint_bytes() const noexcept {return bytes_;}
};
bool decode_step(std::span<const std::uint8_t>,std::uint64_t nonce,std::uint64_t sequence,std::uint64_t clock,const Digest& model,const Digest& predecessor,VerifiedStep&) noexcept;
void encode_step(std::ostream&,const Snapshot&,const AtlasSet&,std::uint64_t,std::uint64_t,std::uint64_t,const Digest&,const Digest&,std::span<const std::uint8_t>,std::uint32_t);
}
