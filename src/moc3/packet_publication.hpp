#pragma once
#ifdef _WIN32
#include "drawable_packet.hpp"
#include "core_consumer.hpp"
#include "d3d11_renderer.hpp"
#include "../rendering/d3d11_dual_mode_host.hpp"
#include <chrono>
namespace ars::moc3 {
class PacketPublication {
    using Host=rendering::D3D11DualModeHost;
    struct Fence;
    CoreConsumer core_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    std::unique_ptr<Host> host_;
    std::shared_ptr<Fence> fence_;
    graphics::d3d11::QueryOps* ops_;
    std::thread::id owner_=std::this_thread::get_id();
    View view_{};
    std::uint64_t sequence_=0,nonce_=0,clock_=0,accepted_cursor_=0;
    std::uint32_t index_=0;
    ID3D11Texture2D* accepted_texture_=nullptr;
    bool hardware_,opened_=false,prepared_=false,terminal_=false,closed_=false;
    GpuProbe probe_;
    std::uint64_t device_calls_=0;
    graphics::Result fail() noexcept;
public:
    struct State {
        std::uint64_t core_generation=0,host_generation=0,epoch=0,physics_cursor=0,device_calls=0,gpu_creations=0;
        bool terminal=false,pending=false,target_matches_accepted=false;
        ars_status core_status=ARS_E_INVALID_ARGUMENT;
        float core_value=0;
    };
    explicit PacketPublication(bool hardware=false,graphics::d3d11::QueryOps* ops=nullptr);
    ~PacketPublication();
    ars_status open();
    ars_status begin(std::uint64_t sequence,std::uint64_t nonce,std::uint32_t index,
                     std::uint64_t clock,float normalized,std::uint32_t lease_ms);
    graphics::Result stage(std::span<const std::uint8_t>,bool fail_target=false);
    graphics::Result poll();
    void cancel() noexcept;
    ars_status revoke();
    State state();
    std::vector<std::uint8_t> read_pixels();
    graphics::Result close();
};
}
#endif
