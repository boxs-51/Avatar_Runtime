#pragma once
#include "runtime_lifecycle.hpp"
#include <thread>
namespace ars::rendering { class PublicD3D11Host; }
namespace ars::core {
#ifdef ARS_PUBLIC_CPU_ASSETS
ars_runtime_desc cpu_descriptor(const ars_runtime_desc &,Runtime *) noexcept;
#endif
struct Runtime {
    #ifdef ARS_PUBLIC_CPU_ASSETS
    const ars_runtime_desc host_descriptor;
#endif
    const ars_runtime_desc descriptor;
    const std::thread::id control=std::this_thread::get_id();
    HandleRegistry registry;
    ars_runtime_h handle{};
    std::uint32_t work=0,management=0,ingress=0,snapshots=0;
    bool closing=false,quiescing=false,finalizer=false;
    security::SourceAuthority *authority=nullptr;
    motion::PublishGate *publication=nullptr;
#ifdef ARS_PUBLIC_CPU_ASSETS
    CpuContext *cpu=nullptr;
    std::atomic_flag cpu_operation=ATOMIC_FLAG_INIT;
    std::atomic<bool> asset_callback{false};
#ifdef ARS_PUBLIC_D3D11
    std::shared_ptr<rendering::PublicD3D11Host> gpu;
#endif
#endif
    bool publication_allocating=false;
    security::CallbackState callback;
    const std::uint64_t authority_budget;
    const std::uint64_t publication_budget;
    Runtime(const ars_runtime_desc &desc,std::uint64_t uid,const RuntimeBudget &budget)
        :
#ifdef ARS_PUBLIC_CPU_ASSETS
         host_descriptor(desc),descriptor(cpu_descriptor(desc,this)),
#else
         descriptor(desc),
#endif
         registry(uid,budget.slots,budget.pins),authority_budget(budget.authority_bytes),publication_budget(budget.publication_bytes) {}
};
#ifdef ARS_PUBLIC_CPU_ASSETS
void free_cpu_context(Runtime *) noexcept;
#endif
}
