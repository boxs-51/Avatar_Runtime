#pragma once
#include "step_packet.hpp"
#include <istream>
#include <ostream>
namespace ars::moc3::admission {
struct LeaseKey {
    std::uint64_t session=0,package=0,epoch=0,generation=0,clock=0,local_sequence=0,local_clock=0,revision=0,nonce=0;
    Digest model{},predecessor{};
    bool operator==(const LeaseKey&) const = default;
};
struct LeaseState {LeaseKey key;PhysicsCheckpoint physics;};
struct LeaseMessage {std::uint32_t kind=0;std::vector<std::uint8_t> body;};
struct LeaseCommand {LeaseKey key;std::uint32_t control=0;std::vector<std::pair<std::uint32_t,float>> bindings;};
std::vector<std::uint8_t> lease_key_bytes(const LeaseKey&);
LeaseKey lease_read_key(std::span<const std::uint8_t>);
bool valid_lease_key(const LeaseKey&) noexcept;
bool lease_successor(const LeaseState*,const LeaseKey&,const Digest&,std::uint64_t base_generation=0) noexcept;
std::vector<std::uint8_t> encode_lease_checkpoint(const LeaseKey&,const PhysicsCheckpoint&);
bool decode_lease_checkpoint(std::span<const std::uint8_t>,LeaseState&) noexcept;
LeaseCommand decode_lease_command(std::span<const std::uint8_t>);
LeaseMessage read_lease_message(std::istream&,std::size_t max_body=packet_cap-24);
void write_lease_message(std::ostream&,std::uint32_t,std::span<const std::uint8_t>);
std::vector<std::uint8_t> encode_lease_candidate(const LeaseKey&,const Snapshot&,const AtlasSet&,std::span<const std::uint8_t>,std::uint32_t);
class VerifiedLease {
    VerifiedScene scene_;
    std::shared_ptr<const LeaseState> state_;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
    friend bool decode_lease_candidate(std::span<const std::uint8_t>,const LeaseKey&,VerifiedLease&) noexcept;
public:
    const auto& scene() const noexcept {return scene_;}
    auto state() const noexcept {return state_;}
    auto checkpoint_bytes() const noexcept {return bytes_;}
};
bool decode_lease_candidate(std::span<const std::uint8_t>,const LeaseKey&,VerifiedLease&) noexcept;
}
