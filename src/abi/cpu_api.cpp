#include "api_validation.hpp"
#include "../core/runtime_lifecycle.hpp"
#include <new>
#include "../assets/asset_limits.hpp"

namespace {
template<class Fn> ars_status boundary(Fn operation) noexcept {
    try { return operation(); }
    catch(const ars::assets::Failure &error) { return error.status; }
    catch(const std::bad_alloc &) { return ARS_E_OUT_OF_BUDGET; }
    catch(...) { return ARS_E_INTERNAL; }
}
}
extern "C" {
ARS_API ars_status ARS_CALL ars_runtime_create(const ars_runtime_desc *desc,ars_runtime_h *out) {
    ars::abi::clear(out);
    if(!out || !ars::abi::valid(desc)) return ARS_E_INVALID_ARGUMENT;
    const auto copy=*desc;
    return boundary([&]{return ars::core::global_router().create(copy,out);});
}
ARS_API ars_status ARS_CALL ars_runtime_get_capabilities(ars_runtime_h runtime,ars_capabilities *out) {
    if(!ars::abi::prefix(out) || out->reserved_flags) return ARS_E_INVALID_ARGUMENT;
    ars::abi::clear(out);
    return boundary([&]() -> ars_status {
        ars::core::RuntimeLease lease;
        auto status=ars::core::global_router().acquire(runtime,ars::core::LeaseKind::work,&lease);
        if(status!=ARS_OK) return status;
        out->struct_size=sizeof(*out); out->abi_version=ARS_ABI_VERSION;
        out->feature_bits=ARS_CAP_HOST_SCOPED_MOTION_INGRESS;
        out->max_active_sources=16;out->max_channels_per_frame=ARS_MAX_CHANNELS_PER_FRAME;out->max_texture_edge=8192;
        return ARS_OK;
    });
}
ARS_API ars_status ARS_CALL ars_runtime_begin_close(ars_runtime_h runtime) {
    return boundary([&]{return ars::core::global_router().begin_close(runtime);});
}
ARS_API ars_status ARS_CALL ars_runtime_query_close(ars_runtime_h runtime,ars_close_state *out) {
    if(!ars::abi::prefix(out) || out->reserved0) return ARS_E_INVALID_ARGUMENT;
    ars::abi::clear(out);
    return boundary([&]{
        (void)ars::core::global_router().cpu_reap(runtime);
        return ars::core::global_router().query_close(runtime,out);
    });
}
ARS_API ars_status ARS_CALL ars_runtime_finish_close(ars_runtime_h runtime) {
    return boundary([&]{
        auto &router=ars::core::global_router();
        auto status=router.finish_close(runtime);
        if(status!=ARS_E_BUSY)return status;
        // A retained cleanup work pin blocks finalization. Reap only after that
        // check, without adding management pins to ordinary finalizer races.
        status=router.cpu_reap(runtime);if(status!=ARS_OK)return status;
        return router.finish_close(runtime);
    });
}

ARS_API ars_status ARS_CALL ars_model_load(ars_runtime_h r,const ars_asset_source *s,ars_model_h *out) {ars::abi::clear(out);return boundary([&]{return ars::core::global_router().cpu_load(r,s,out);});}
ARS_API ars_status ARS_CALL ars_model_release(ars_runtime_h r,ars_model_h m) {return boundary([&]{return ars::core::global_router().cpu_release(r,m);});}
ARS_API ars_status ARS_CALL ars_character_create(ars_runtime_h r,ars_model_h m,const ars_character_desc *d,ars_character_h *out) {ars::abi::clear(out);return boundary([&]{return ars::core::global_router().cpu_create_character(r,m,d,out);});}
ARS_API ars_status ARS_CALL ars_character_destroy(ars_runtime_h r,ars_character_h c) {return boundary([&]{return ars::core::global_router().cpu_destroy_character(r,c);});}
ARS_API ars_status ARS_CALL ars_character_set_mode(ars_runtime_h r,ars_character_h c,std::uint32_t mode) {return boundary([&]{return ars::core::global_router().cpu_mode(r,c,mode);});}
ARS_API ars_status ARS_CALL ars_character_set_visual_layer(ars_runtime_h r,ars_character_h c,const ars_layer_change *d) {return boundary([&]{return ars::core::global_router().cpu_layer(r,c,d);});}
ARS_API ars_status ARS_CALL ars_source_register(ars_runtime_h r,ars_character_h c,const ars_source_desc *d,ars_source_admin_h *out,ars_ingress_cap *cap) {ars::abi::clear(out);ars::abi::clear(cap);return boundary([&]{return ars::core::global_router().cpu_register_source(r,c,d,out,cap);});}
ARS_API ars_status ARS_CALL ars_source_admin_release(ars_runtime_h r,ars_source_admin_h a) {return boundary([&]{return ars::core::global_router().cpu_source_control(r,a,2);});}
ARS_API ars_status ARS_CALL ars_source_rotate(ars_runtime_h r,ars_source_admin_h a,ars_ingress_cap *out) {ars::abi::clear(out);if(!out)return ARS_E_INVALID_ARGUMENT;return boundary([&]{return ars::core::global_router().cpu_source_control(r,a,0,out);});}
ARS_API ars_status ARS_CALL ars_source_revoke(ars_runtime_h r,ars_source_admin_h a) {return boundary([&]{return ars::core::global_router().cpu_source_control(r,a,1);});}
ARS_API ars_status ARS_CALL ars_motion_submit(ars_runtime_h r,ars_ingress_cap cap,const ars_motion_frame *frame) {return boundary([&]{return ars::core::global_router().submit_frame(r,cap,frame);});}
ARS_API ars_status ARS_CALL ars_runtime_update(ars_runtime_h r,std::uint64_t time) {return boundary([&]{return ars::core::global_router().cpu_update(r,time);});}
ARS_API ars_status ARS_CALL ars_runtime_render(ars_runtime_h r,const ars_render_target *target) {(void)r;return ars::abi::valid(target)?ARS_E_UNSUPPORTED:ARS_E_INVALID_ARGUMENT;}
ARS_API ars_status ARS_CALL ars_runtime_drain_gpu(ars_runtime_h r) {(void)r;return ARS_E_UNSUPPORTED;}
}
