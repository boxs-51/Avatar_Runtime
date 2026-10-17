#pragma once
#ifdef _WIN32
#include "../backend.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <thread>

namespace ars::graphics::d3d11 {
// Injectable only inside the private backend. Production uses Direct3D calls.
struct QueryOps {
    virtual ~QueryOps()=default;
    virtual HRESULT removed_reason(ID3D11Device *device) = 0;
    virtual HRESULT get_data(ID3D11DeviceContext *context, ID3D11Query *query, BOOL *done) = 0;
    virtual void flush(ID3D11DeviceContext *context) = 0;
};
// One submission per object. The host must keep the borrowed device/context
// alive and destroy this object on its R thread after completion or teardown.
class QueryRetirement {
    std::thread::id owner_=std::this_thread::get_id();
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
    Microsoft::WRL::ComPtr<ID3D11Query> query_;
    std::shared_ptr<const void> snapshot_pin_;
    std::uint64_t generation_=0;
    bool started_=false, ended_=false, flushed_=false, complete_=false, quarantined_=false, detached_lost_=false;
    QueryOps *ops_;
public:
    explicit QueryRetirement(QueryOps *ops=nullptr);
    ~QueryRetirement();
    QueryRetirement(const QueryRetirement &)=delete;
    QueryRetirement &operator=(const QueryRetirement &)=delete;
    Result begin(ID3D11Device *, ID3D11DeviceContext *, ID3D11RenderTargetView *,
                 std::uint64_t, std::shared_ptr<const void> snapshot_pin);
    Result prepare(ID3D11Device *, ID3D11DeviceContext *, ID3D11RenderTargetView *,
                   std::uint64_t, std::shared_ptr<const void> snapshot_pin);
    Result end();
    Result cancel_unsubmitted();
    Result mark_lost();
    Result poll(std::uint64_t generation);
    // Caller must first establish the host's lost-device teardown barrier on R.
    Result detach_lost(std::uint64_t generation);
    bool complete() const noexcept { return complete_; }
    bool quarantined() const noexcept { return quarantined_; }
    bool holds_target() const noexcept { return target_!=nullptr; }
    bool holds_pin() const noexcept { return snapshot_pin_!=nullptr; }
    bool detached_lost() const noexcept { return detached_lost_; }
};
}
#endif
