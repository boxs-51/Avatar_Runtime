#include "d3d11_dual_mode_host.hpp"
#ifdef _WIN32
#include <new>
#include <thread>

namespace ars::rendering {
using graphics::Result;
using graphics::TargetInfo;
using graphics::d3d11::QueryRetirement;
using Microsoft::WRL::ComPtr;
struct D3D11DualModeHost::Pending {
    explicit Pending(graphics::d3d11::QueryOps *ops,std::shared_ptr<graphics::d3d11::ContextOwnership> owner) : query(ops,std::move(owner)) {}
    Mode mode=Mode::none;
    std::uint64_t pose=0;
    std::uint32_t width=0,height=0;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
    QueryRetirement query;
    std::shared_ptr<PublicationFence> fence;
};
namespace {
struct Pins2D {
    std::shared_ptr<const PreparedScene2D> scene;
    std::shared_ptr<const assets::Image> atlas;
};
}
D3D11DualModeHost::D3D11DualModeHost(ID3D11Device *device,ID3D11DeviceContext *context,
                                     std::uint64_t generation,graphics::d3d11::QueryOps *ops)
    : D3D11DualModeHost(device,context,generation,ops,{}) {}
D3D11DualModeHost::D3D11DualModeHost(ID3D11Device *device,ID3D11DeviceContext *context,
    std::uint64_t generation,graphics::d3d11::QueryOps *ops,
    std::shared_ptr<graphics::d3d11::ContextOwnership> owner)
    : ownership_(std::move(owner)),identity_(device),backend_(device,context,generation,ownership_),ops_(ops) {}
Result D3D11DualModeHost::create_owned(std::shared_ptr<graphics::d3d11::ContextOwnership> owner,
    graphics::d3d11::QueryOps *ops,std::unique_ptr<D3D11DualModeHost>& out) {
    if(!owner || out)return Result::invalid_argument;
    auto operation=owner->enter();if(operation.result()!=Result::ok)return operation.result();
    try {
        auto candidate=std::unique_ptr<D3D11DualModeHost>(new D3D11DualModeHost(operation.device(),operation.context(),owner->generation(),ops,owner));
        auto status=candidate->backend_.ownership_status();if(status!=Result::ok)return status;
        status=owner->retain(candidate->target_owner_);if(status!=Result::ok)return status;
        out=std::move(candidate);return Result::ok;
    }catch(const std::bad_alloc&){return Result::out_of_budget;}
}
D3D11DualModeHost::~D3D11DualModeHost() {
    if (!backend_.on_owner_thread()) std::terminate();
    // A pending query is intentionally fatal unless the owner explicitly
    // drains it or acknowledges loss through this host's teardown path.
}
Result D3D11DualModeHost::prepare_target(Pending &candidate,std::uint32_t width,
                                         std::uint32_t height,bool fail_create) {
    if (!width || !height || width>8192 || height>8192) return Result::invalid_argument;
    if (fail_create) return Result::out_of_budget;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    auto failure=[this](HRESULT hr) {
        if (hr==DXGI_ERROR_DEVICE_REMOVED || hr==DXGI_ERROR_DEVICE_RESET ||
            FAILED(backend_.device()->GetDeviceRemovedReason())) return Result::device_lost;
        return hr==E_OUTOFMEMORY?Result::out_of_budget:Result::failed;
    };
    auto hr=backend_.device()->CreateTexture2D(&desc,nullptr,&candidate.texture);
    if (FAILED(hr)) return failure(hr);
    hr=backend_.device()->CreateRenderTargetView(candidate.texture.Get(),nullptr,&candidate.view);
    if (FAILED(hr)) return failure(hr);
    candidate.width=width;candidate.height=height;
    return Result::ok;
}
Result D3D11DualModeHost::start(std::unique_ptr<Pending> candidate,Result result) {
    if (result==Result::ok) { pending_=std::move(candidate);return Result::ok; }
    if (candidate->query.holds_target()) {
        candidate->query.mark_lost();
        pending_=std::move(candidate);
        lost_=true;
    } else if (result==Result::device_lost) lost_=true;
    return result;
}
Result D3D11DualModeHost::request_2d(std::shared_ptr<const PreparedScene2D> scene,
    std::shared_ptr<const assets::Image> atlas,std::uint64_t pose,
    std::uint32_t width,std::uint32_t height,bool fail_target_create) {
    if (!backend_.on_owner_thread()) return Result::wrong_thread;
    auto operation=backend_.enter_context();if(operation.result()!=Result::ok)return operation.result();
    if (lost_) return Result::device_lost;
    if (closed_ || closing_.load(std::memory_order_acquire) || pending_) return Result::busy;
    if (!scene || !atlas || !pose) return Result::unsupported;
    try {
        auto candidate=std::make_unique<Pending>(ops_,ownership_);
        candidate->mode=Mode::two_d;candidate->pose=pose;
        auto ready=prepare_target(*candidate,width,height,fail_target_create);
        if (ready!=Result::ok) return ready;
        auto pins=std::make_shared<Pins2D>(Pins2D{std::move(scene),std::move(atlas)});
        const TargetInfo info{backend_.generation(),width,height};
        auto result=render_offscreen_2d(backend_,candidate->view.Get(),info,*pins->scene,
                                        *pins->atlas,candidate->query,pins);
        return start(std::move(candidate),result);
    } catch (const std::bad_alloc &) { return Result::out_of_budget;
    } catch (...) { return Result::failed; }
}
Result D3D11DualModeHost::request_3d(std::shared_ptr<const PreparedScene3D> scene,
    const Camera3D &camera,std::uint64_t pose,std::uint32_t width,std::uint32_t height,
    GpuCreateProbe *probe,bool fail_target_create) {
    if (!backend_.on_owner_thread()) return Result::wrong_thread;
    auto operation=backend_.enter_context();if(operation.result()!=Result::ok)return operation.result();
    if (lost_) return Result::device_lost;
    if (closed_ || closing_.load(std::memory_order_acquire) || pending_) return Result::busy;
    if (!scene || !pose) return Result::unsupported;
    try {
        auto candidate=std::make_unique<Pending>(ops_,ownership_);
        candidate->mode=Mode::three_d;candidate->pose=pose;
        auto ready=prepare_target(*candidate,width,height,fail_target_create);
        if (ready!=Result::ok) return ready;
        const TargetInfo info{backend_.generation(),width,height};
        auto result=render_offscreen_3d(backend_,candidate->view.Get(),info,std::move(scene),
                                        camera,candidate->query,nullptr,probe);
        return start(std::move(candidate),result);
    } catch (const std::bad_alloc &) { return Result::out_of_budget;
    } catch (...) { return Result::failed; }
}
Result D3D11DualModeHost::poll_commit() {
    auto operation=backend_.enter_context();if(operation.result()!=Result::ok)return operation.result();
    return poll_commit_impl();
}
Result D3D11DualModeHost::poll_commit_impl() {
    if (!backend_.on_owner_thread()) return Result::wrong_thread;
    if (!pending_) return Result::invalid_argument;
    if (lost_) return Result::device_lost;
    const auto result=pending_->query.poll(backend_.generation());
    if (result==Result::busy) return result;
    if (result!=Result::ok) { lost_=true;return result; }
    if(pending_->fence){
        const auto decision=closing_.load(std::memory_order_acquire)?Result::failed:pending_->fence->commit();
        if(decision!=Result::ok){pending_.reset();return decision;}
    }
    active_texture_=std::move(pending_->texture);
    active_view_=std::move(pending_->view);
    mode_=pending_->mode;pose_generation_=pending_->pose;
    width_=pending_->width;height_=pending_->height;
    ++target_epoch_;
    pending_.reset();
    return Result::ok;
}
Result D3D11DualModeHost::request_moc(const MocPass& pass,std::uint64_t pose,
    std::uint32_t width,std::uint32_t height,bool fail_target_create,std::shared_ptr<PublicationFence> fence) {
    if(!backend_.on_owner_thread())return Result::wrong_thread;
    auto operation=backend_.enter_context();if(operation.result()!=Result::ok)return operation.result();
    if(lost_)return Result::device_lost;
    if(closed_||closing_.load(std::memory_order_acquire)||pending_)return Result::busy;
    if(!pass||!pose||!width||!height||width>2048||height>2048)return Result::invalid_argument;
    try {
        auto candidate=std::make_unique<Pending>(ops_,ownership_);
        candidate->fence=std::move(fence);
        candidate->mode=Mode::moc3;candidate->pose=pose;
        auto result=prepare_target(*candidate,width,height,fail_target_create);
        if(result!=Result::ok)return result;
        // The candidate must stay alive across exceptions once the callback
        // has prepared/submitted its query. start() quarantines such pins.
        try {
            result=pass(backend_,candidate->view.Get(),{backend_.generation(),width,height},candidate->query);
        }catch(const std::bad_alloc&){result=Result::out_of_budget;
        }catch(...){result=Result::failed;}
        return start(std::move(candidate),result);
    }catch(const std::bad_alloc&){return Result::out_of_budget;}
}
Result D3D11DualModeHost::begin_close() noexcept {
    closing_.store(true,std::memory_order_release);
    if(ownership_)ownership_->begin_close();
    return Result::ok;
}
Result D3D11DualModeHost::poll_close() {
    if (!backend_.on_owner_thread()) return Result::wrong_thread;
    if (!closing_.load(std::memory_order_acquire)) return Result::invalid_argument;
    if (closed_) return Result::ok;
    auto operation=backend_.enter_context(true);if(operation.result()!=Result::ok)return operation.result();
    if (lost_) return Result::device_lost;
    if (pending_) {
        const auto result=poll_commit_impl();
        if (result!=Result::ok && (!ownership_ || pending_ || lost_)) return result;
    }
    active_view_.Reset();active_texture_.Reset();
    backend_.release_owned_references();target_owner_.reset();
    closed_=true;
    return Result::ok;
}
Result D3D11DualModeHost::detach_lost() {
    if(ownership_)return Result::invalid_argument;
    if (!backend_.on_owner_thread()) return Result::wrong_thread;
    if (!lost_ || !closing_.load(std::memory_order_acquire) || closed_)
        return Result::invalid_argument;
    // Only this isolated host's context users and targets are represented.
    // No external host teardown claim follows from this path.
    if (backend_.context()) backend_.context()->ClearState();
    active_view_.Reset();active_texture_.Reset();
    if (pending_) { pending_->view.Reset();pending_->texture.Reset(); }
    if (pending_) {
        const auto result=pending_->query.detach_lost(backend_.generation());
        if (result!=Result::ok) return result;
        pending_.reset();
    }
    backend_.release_owned_references();target_owner_.reset();
    closed_=true;
    return Result::ok;
}
Result D3D11DualModeHost::prepare_lost_teardown() {
    if(!backend_.on_owner_thread())return Result::wrong_thread;
    if(surrendered_)return Result::ok;
    if(!ownership_ || !lost_ || !closing_.load(std::memory_order_acquire) || closed_ || surrendered_)return Result::invalid_argument;
    auto operation=ownership_->enter(true);if(operation.result()!=Result::ok)return operation.result();
    active_view_.Reset();active_texture_.Reset();
    if(pending_){
        pending_->view.Reset();pending_->texture.Reset();
        const auto status=pending_->query.surrender_lost(backend_.generation());if(status!=Result::ok)return status;
    }
    backend_.release_owned_references();target_owner_.reset();surrendered_=true;return Result::ok;
}
Result D3D11DualModeHost::acknowledge_teardown(graphics::d3d11::HostTeardownReceipt &receipt) {
    if(!backend_.on_owner_thread())return Result::wrong_thread;
    if(!ownership_ || (!closed_ && !surrendered_) ||
       !receipt.matches(ownership_->authority(),backend_.generation(),identity_))return Result::invalid_argument;
    if(pending_){
        const auto status=pending_->query.detach_lost_impl(backend_.generation());if(status!=Result::ok)return status;
        pending_.reset();
    }
    receipt.consume();closed_=true;return Result::ok;
}
bool D3D11DualModeHost::accepts(ID3D11RenderTargetView *view,
                                std::uint64_t target_epoch) const noexcept {
    return backend_.on_owner_thread() && !closed_ && !lost_ &&
           !closing_.load(std::memory_order_acquire) && !pending_ &&
           view && view==active_view_.Get() && target_epoch && target_epoch==target_epoch_;
}
} // namespace ars::rendering
#endif
