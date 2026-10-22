#include "public_d3d11.hpp"
#include "committed_scene.hpp"
#include "d3d11_scene2d.hpp"
#include "d3d11_scene3d.hpp"
#include "../graphics/d3d11/creation_status.hpp"
#include "../graphics/d3d11/state_scope.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>

namespace ars::rendering {
using graphics::Result;
using graphics::d3d11::ContextOwnership;
using graphics::d3d11::QueryRetirement;
using Microsoft::WRL::ComPtr;
namespace {
ars_status status(Result r) {
    switch(r){case Result::ok:return ARS_OK;case Result::busy:return ARS_E_BUSY;
    case Result::invalid_argument:return ARS_E_INVALID_ARGUMENT;case Result::unsupported:return ARS_E_UNSUPPORTED;
    case Result::wrong_thread:return ARS_E_WRONG_THREAD;case Result::device_lost:return ARS_E_DEVICE_LOST;
    case Result::out_of_budget:return ARS_E_OUT_OF_BUDGET;default:return ARS_E_INTERNAL;}
}
struct Bundle {
    core::RenderFrameSet frames; // Last destroyed: allocator/model custody.
    std::vector<std::pair<std::size_t,PreparedScene2D>> two;
    PreparedScene3D three;
    Camera3D camera;
};
ars_status project(core::RuntimeRouter &router,ars_runtime_h runtime,std::uint32_t w,std::uint32_t h,
    std::shared_ptr<Bundle> &out) {
    auto next=std::make_shared<Bundle>();auto result=router.capture_render_set(runtime,&next->frames);
    if(result!=ARS_OK)return result;
    std::size_t bytes=std::size_t(w)*h*8,pose_bytes=0;
    if(bytes>256*assets::MiB)return ARS_E_OUT_OF_BUDGET;
    bool camera_chosen=false;
    for(std::size_t i=0;i<next->frames.count;++i){
        const auto &frame=*next->frames.frames[i];const auto &model=*frame.model();
        const auto pose=std::span(frame.pose()->outputs.data(),frame.pose()->channel_count);
        if(frame.mode()==ARS_MODE_2D){
            std::vector<LayerControl> controls;for(auto l:frame.layers())controls.push_back({l.visible,l.opacity,l.z});
            PreparedScene2D scene;result=prepare_committed_2d(model,pose,controls,scene);if(result!=ARS_OK)return result;
            bytes+=model.image().rgba.size();
            for(const auto &draw:scene.draws){pose_bytes+=draw.vertices.size()*sizeof(assets::Pair)*2+draw.triangles.size()*12;
                bytes+=draw.vertices.size()*36+draw.triangles.size()*12;}
            next->two.emplace_back(i,std::move(scene));
        }else if(frame.mode()==ARS_MODE_3D){
            PreparedScene3D scene;result=prepare_committed_3d(model,pose,scene);if(result!=ARS_OK)return result;
            bytes+=scene.estimated_gpu_bytes;
            if(!next->three.source)next->three.source=scene.source;
            // ABI20 has no camera setter: the first 3D character in stable
            // slot order owns the world camera. Pick its lowest camera node.
            if(!camera_chosen){
                if(!scene.cameras.empty()){
                    const auto camera=std::min_element(scene.cameras.begin(),scene.cameras.end(),
                        [](const auto &a,const auto &b){return a.node<b.node;});
                    result=select_gltf_camera(scene,camera->node,w,h,next->camera);if(result!=ARS_OK)return result;
                }else{
                    // No authored camera: fixed right-handed orthographic
                    // world volume [-2,2]x[-2,2], near0.01 far100, eye z=5.
                    next->camera.clip_from_world={0.5,0,0,0,0,0.5,0,0,0,0,-1./99.99,0,0,0,4.99/99.99,1};
                    next->camera.front_counter_clockwise=true;
                }
                camera_chosen=true;
            }
            for(auto &draw:scene.draws){pose_bytes+=draw.vertices.size()*sizeof(Vertex3D)+draw.indices.size()*4;
                draw.source=scene.source;next->three.draws.push_back(std::move(draw));}
        }else return ARS_E_UNSUPPORTED;
        if(bytes>256*assets::MiB||pose_bytes>64*assets::MiB)return ARS_E_OUT_OF_BUDGET;
    }
    const auto passes=next->two.size()+(!next->three.draws.empty()?1:0);
    if(!next->three.draws.empty())bytes+=std::size_t(w)*h*4; // One shared depth target.
    bytes+=std::max<std::size_t>(passes,1)*64*1024; // Fixed buffers/states per retained pass.
    if(bytes>256*assets::MiB)return ARS_E_OUT_OF_BUDGET;
    next->three.gpu_budget=256*assets::MiB;out=std::move(next);return ARS_OK;
}
struct GpuTarget {
    ContextOwnership::Participant owner;
    ComPtr<ID3D11Texture2D> candidate,destination;
    ComPtr<ID3D11RenderTargetView> candidate_view,destination_view;
};
}
struct PublicD3D11Host::Impl {
    std::shared_ptr<ContextOwnership> root;
    std::unique_ptr<graphics::d3d11::HostDevice> backend;
    struct Pending {
        std::shared_ptr<Bundle> bundle;
        std::shared_ptr<GpuTarget> target;
        std::vector<std::unique_ptr<QueryRetirement>> queries;
        ars_status failure=ARS_OK;
        bool copied=false;
        bool observed_draw=false;
        AcceptedRender publication;
    };
    std::unique_ptr<Pending> pending;
    ars_runtime_h runtime{};
    core::RuntimeRouter *router=nullptr;
    bool lost=false;
    AcceptedRender accepted;
    bool surrendered=false;
    ID3D11Device *retired_identity=nullptr;
    PublicD3D11Hooks *hooks=nullptr;
    // Polls exact queries once; never waits or spins inside the API.
    ars_status poll(bool publish) {
        if(!pending)return ARS_OK;
        auto operation=root->enter(root->closing());if(operation.result()!=Result::ok)return status(operation.result());
        for(auto &query:pending->queries){
            if(query->complete())continue;
            const auto r=query->poll(root->generation());if(r!=Result::ok){
                if(r!=Result::busy)lost=true;return status(r);
            }
        }
        if(!pending->copied&&pending->failure==ARS_OK&&publish){
            if(!pending->observed_draw){pending->observed_draw=true;
                if(hooks&&hooks->observe)hooks->observe(hooks->user,PublicD3D11Hooks::Fence::draw_ready);}
            auto ready=router->validate_render_set(runtime,pending->bundle->frames);
            if(ready==ARS_E_BUSY)return ready;
            if(ready!=ARS_OK){pending.reset();return ready;}
            if(accepted.target_epoch==UINT64_MAX){pending.reset();return ARS_E_HANDLE_EXHAUSTED;}
            auto &record=pending->publication;record.target_generation=root->generation();record.target_epoch=accepted.target_epoch+1;
            record.camera=pending->bundle->camera;record.count=pending->bundle->frames.count;
            for(std::size_t i=0;i<record.count;++i){const auto &f=*pending->bundle->frames.frames[i];
                record.characters[i]={f.character_id(),f.model_id(),f.generation(),f.configuration(),f.mode()};}
            // Candidate draw completion + CPU publication fence accept this
            // immutable frame before presentation. A later H/S mutation cannot
            // relabel it. The copy has a separate exact completion fence.
            auto query=std::make_unique<QueryRetirement>(hooks?hooks->queries:nullptr,root);
            graphics::d3d11::StateScope restore(operation.context(),root);
            if(restore.result()!=Result::ok)return status(restore.result());
            const auto prepared=query->prepare_split(operation.device(),operation.context(),
                pending->target->destination_view.Get(),root->generation(),pending->bundle,pending->target);
            if(prepared!=Result::ok)return status(prepared);
            operation.context()->CopyResource(pending->target->destination.Get(),pending->target->candidate.Get());
            query->end();pending->queries.clear();pending->queries.push_back(std::move(query));pending->copied=true;
            return ARS_E_BUSY;
        }
        if(publish&&pending->copied&&pending->failure==ARS_OK){
            if(hooks&&hooks->observe)hooks->observe(hooks->user,PublicD3D11Hooks::Fence::copy_ready);
            accepted=pending->publication;
        }
        const auto result=pending->failure;pending.reset();return result;
    }
};
PublicD3D11Host::PublicD3D11Host(PublicD3D11Hooks *hooks):hooks_(hooks){}
PublicD3D11Host::~PublicD3D11Host(){if(impl_)std::terminate();}
const AcceptedRender *PublicD3D11Host::accepted() const noexcept {
    return on_owner()&&impl_&&impl_->accepted.target_epoch?&impl_->accepted:nullptr;
}
ars_status PublicD3D11Host::render(core::RuntimeRouter &router,ars_runtime_h runtime,
    const ars_render_target &target,std::shared_ptr<const void> runtime_pin) {
    if(!on_owner())return ARS_E_WRONG_THREAD;
    if(!target.d3d11_device||!target.d3d11_immediate_context||!target.d3d11_rtv||!target.device_generation||
       !target.width_px||!target.height_px||target.width_px>8192||target.height_px>8192)return ARS_E_INVALID_ARGUMENT;
    if(target.dxgi_format!=DXGI_FORMAT_R8G8B8A8_UNORM||target.sample_count!=1||
       target.color_space!=ARS_COLOR_SRGB||target.alpha_mode!=ARS_ALPHA_PREMULTIPLIED)return ARS_E_UNSUPPORTED;
    auto *device=static_cast<ID3D11Device*>(target.d3d11_device);
    auto *context=static_cast<ID3D11DeviceContext*>(target.d3d11_immediate_context);
    auto *view=static_cast<ID3D11RenderTargetView*>(target.d3d11_rtv);
    if(!impl_){
        auto next=std::make_unique<Impl>();next->runtime=runtime;next->router=&router;next->hooks=hooks_;
        auto r=ContextOwnership::create(device,context,target.device_generation,std::move(runtime_pin),next->root);
        if(r!=Result::ok)return status(r);
        {auto operation=next->root->enter();if(operation.result()!=Result::ok)return status(operation.result());
            next->backend=std::make_unique<graphics::d3d11::HostDevice>(device,context,target.device_generation,next->root);}
        impl_=std::move(next);active_.store(true,std::memory_order_release);
    }
    auto &state=*impl_;if(state.lost)return ARS_E_DEVICE_LOST;
    {
        auto operation=state.root->enter();if(operation.result()!=Result::ok)return status(operation.result());
        if(!state.root->owns(device,context,target.device_generation))return ARS_E_INVALID_ARGUMENT;
        const auto valid=state.backend->validate(view,{target.device_generation,target.width_px,target.height_px});
        if(valid!=Result::ok)return status(valid);
    }
    if(state.pending){
        if(state.pending->target->destination_view.Get()!=view)return ARS_E_BUSY;
        return state.poll(true);
    }
    auto candidate=std::make_unique<Impl::Pending>();
    auto result=project(router,runtime,target.width_px,target.height_px,candidate->bundle);if(result!=ARS_OK)return result;
    auto operation=state.root->enter();if(operation.result()!=Result::ok)return status(operation.result());
    candidate->target=std::make_shared<GpuTarget>();auto &gpu=*candidate->target;
    auto r=state.root->retain(gpu.owner);if(r!=Result::ok)return status(r);
    gpu.destination_view=view;ComPtr<ID3D11Resource> resource;view->GetResource(&resource);
    if(FAILED(resource.As(&gpu.destination)))return ARS_E_INVALID_ARGUMENT;
    D3D11_TEXTURE2D_DESC desc{};gpu.destination->GetDesc(&desc);
    if(desc.ArraySize!=1||desc.MipLevels!=1||desc.Usage!=D3D11_USAGE_DEFAULT)return ARS_E_UNSUPPORTED;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;desc.CPUAccessFlags=desc.MiscFlags=0;
    auto *probe=hooks_?hooks_->creates:nullptr;
    auto hr=probe&&!probe->allow()?probe->failure:device->CreateTexture2D(&desc,nullptr,&gpu.candidate);
    if(FAILED(hr))return status(graphics::d3d11::creation_status(hr,device->GetDeviceRemovedReason()));
    hr=probe&&!probe->allow()?probe->failure:device->CreateRenderTargetView(gpu.candidate.Get(),nullptr,&gpu.candidate_view);
    if(FAILED(hr))return status(graphics::d3d11::creation_status(hr,device->GetDeviceRemovedReason()));
    // Reserve all container storage before the first GPU submission.
    const auto passes=candidate->bundle->two.size()+(!candidate->bundle->three.draws.empty()?1:0);
    candidate->queries.reserve(std::max<std::size_t>(passes,1));
    for(std::size_t i=0;i<std::max<std::size_t>(passes,1);++i)
        candidate->queries.push_back(std::make_unique<QueryRetirement>(hooks_?hooks_->queries:nullptr,state.root));
    const graphics::TargetInfo info{target.device_generation,target.width_px,target.height_px};
    std::size_t submitted=0;
    if(!candidate->bundle->three.draws.empty()){
        std::shared_ptr<const PreparedScene3D> scene(candidate->bundle,&candidate->bundle->three);
        r=render_offscreen_3d(*state.backend,gpu.candidate_view.Get(),info,scene,candidate->bundle->camera,*candidate->queries[submitted],nullptr,probe);
        if(candidate->queries[submitted]->holds_target())++submitted;
        if(r!=Result::ok)candidate->failure=status(r);
    }
    for(const auto &[index,scene]:candidate->bundle->two){
        if(candidate->failure!=ARS_OK)break;
        r=render_offscreen_2d(*state.backend,gpu.candidate_view.Get(),info,scene,
            candidate->bundle->frames.frames[index]->model()->image(),*candidate->queries[submitted],candidate->bundle,submitted==0,probe);
        if(candidate->queries[submitted]->holds_target())++submitted;
        if(r!=Result::ok)candidate->failure=status(r);
    }
    if(!passes){
        graphics::d3d11::StateScope restore(context,state.root);
        if(restore.result()!=Result::ok)return status(restore.result());
        auto &query=*candidate->queries[0];r=query.prepare_split(device,context,gpu.candidate_view.Get(),target.device_generation,candidate->bundle,candidate->target);
        if(r!=Result::ok)return status(r);
        constexpr FLOAT clear[4]{};context->ClearRenderTargetView(gpu.candidate_view.Get(),clear);query.end();submitted=1;
    }
    candidate->queries.resize(submitted);
    if(candidate->failure==ARS_E_DEVICE_LOST)state.lost=true;
    if(!submitted)return candidate->failure;
    state.pending=std::move(candidate);return ARS_E_BUSY;
}
ars_status PublicD3D11Host::drain(bool closing) {
    if(!on_owner())return ARS_E_WRONG_THREAD;
    if(!impl_)return ARS_OK;
    auto &state=*impl_;
    if(state.lost){
        if(!closing)return ARS_E_DEVICE_LOST;
        state.root->begin_close();
        if(!state.surrendered){
            auto operation=state.root->enter(true);if(operation.result()!=Result::ok)return status(operation.result());
            state.retired_identity=operation.device();
            if(state.pending){
                for(auto &query:state.pending->queries)if(!query->complete()){
                    query->mark_lost();auto r=query->surrender_lost(state.root->generation());if(r!=Result::ok)return status(r);
                }
                // Surrender only SDK references. External host references are
                // neither counted nor claimed to have completed here.
                state.pending->target.reset();
            }
            state.backend->release_owned_references();state.backend.reset();state.surrendered=true;
        }
        graphics::d3d11::HostTeardownReceipt receipt;auto r=state.root->issue(receipt);if(r!=Result::ok)return status(r);
        if(!receipt.matches(state.root->authority(),state.root->generation(),state.retired_identity))return ARS_E_INTERNAL;
        if(state.pending)for(auto &query:state.pending->queries)if(!query->complete()){
            r=query->detach_lost_impl(state.root->generation());if(r!=Result::ok)return status(r);
        }
        state.pending.reset();receipt.consume();impl_.reset();active_.store(false,std::memory_order_release);return ARS_OK;
    }
    auto result=state.poll(!closing);if(result!=ARS_OK)return result;
    if(!closing)return ARS_OK;
    state.root->begin_close();
    {auto operation=state.root->enter(true);if(operation.result()!=Result::ok)return status(operation.result());
        if(state.backend){state.backend->release_owned_references();state.backend.reset();}}
    graphics::d3d11::HostTeardownReceipt receipt;auto ready=state.root->issue(receipt);if(ready!=Result::ok)return status(ready);
    // Healthy exact queries are complete; dropping the root/receipt releases
    // final runtime custody only after all GPU references are gone on R.
    impl_.reset();active_.store(false,std::memory_order_release);return ARS_OK;
}
}
