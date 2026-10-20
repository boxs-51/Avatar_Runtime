#pragma once
#ifdef _WIN32
#include "lease_protocol.hpp"
#include "core_consumer.hpp"
#include "d3d11_renderer.hpp"
#include "../rendering/d3d11_dual_mode_host.hpp"
namespace ars::moc3 {
class LeasePublication {
    struct Fence;
    CoreConsumer core_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    std::unique_ptr<rendering::D3D11DualModeHost> host_;
    std::shared_ptr<Fence> fence_;
    std::shared_ptr<const admission::LeaseState> accepted_;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
    graphics::d3d11::QueryOps* ops_;
    std::thread::id owner_=std::this_thread::get_id();
    std::uint64_t session_=0,generation_=0,ingress_=0,minimum_epoch_=1;
    bool hardware_,opened_=false,closed_=false,closing_=false,pending_=false,terminal_=false;
    View view_{};GpuProbe probe_;
    graphics::Result reject() noexcept;
public:
    struct State {std::uint64_t generation=0,host_generation=0,epoch=0,package=0,clock=0,device_calls=0,creations=0,ingress=0;bool pending=false,terminal=false;ars_status core_status=ARS_E_INVALID_ARGUMENT;};
    explicit LeasePublication(bool hardware=false,graphics::d3d11::QueryOps* ops=nullptr):ops_(ops),hardware_(hardware){}
    ~LeasePublication();
    ars_status open(std::uint64_t session);
    ars_status begin(std::span<const std::uint8_t> command,std::uint32_t lease_ms);
    graphics::Result stage(std::span<const std::uint8_t>,bool fail_target=false);
    graphics::Result poll();
    void cancel() noexcept;
    ars_status revoke();
    State state();
    const std::vector<std::uint8_t>& checkpoint() const;
    std::vector<std::uint8_t> read_pixels();
    std::vector<std::uint8_t> core_bytes();
    graphics::Result close();
};
}
#endif
