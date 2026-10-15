#include "api_validation.hpp"
#include "../core/runtime_lifecycle.hpp"
#include <new>

namespace {
template<class Fn> ars_status boundary(Fn operation) noexcept {
    try { return operation(); }
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
        return ARS_OK; // No graphics, asset or motion capabilities implemented.
    });
}
ARS_API ars_status ARS_CALL ars_runtime_begin_close(ars_runtime_h runtime) {
    return boundary([&]{return ars::core::global_router().begin_close(runtime);});
}
ARS_API ars_status ARS_CALL ars_runtime_query_close(ars_runtime_h runtime,ars_close_state *out) {
    if(!ars::abi::prefix(out) || out->reserved0) return ARS_E_INVALID_ARGUMENT;
    ars::abi::clear(out);
    return boundary([&]{return ars::core::global_router().query_close(runtime,out);});
}
ARS_API ars_status ARS_CALL ars_runtime_finish_close(ars_runtime_h runtime) {
    return boundary([&]{return ars::core::global_router().finish_close(runtime);});
}
}
