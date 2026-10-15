#include "cpu_snapshot.hpp"
namespace ars::motion {
void Snapshot::retain(Snapshot *snapshot) noexcept { if(snapshot) snapshot->references.fetch_add(1,std::memory_order_acq_rel); }
void Snapshot::release(Snapshot *snapshot) noexcept {
    if(snapshot && snapshot->references.fetch_sub(1,std::memory_order_acq_rel)==1) {
        for(auto &source:snapshot->sources) source.reset();
        snapshot->character.reset(); snapshot->used.store(false,std::memory_order_release);
    }
}
}
