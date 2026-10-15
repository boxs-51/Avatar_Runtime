#include "publish_gate.hpp"
#include "../security/source_authority.hpp"
#include <algorithm>
#include <limits>
namespace ars::motion {
PublishGate::PublishGate(security::SourceAuthority &authority):authority_(authority) {}
Snapshot *PublishGate::reserve() {
    for(std::uint32_t i=0;i<snapshot_limit_;++i) {
        bool free=false;
        if(snapshots_[i].used.compare_exchange_strong(free,true,std::memory_order_acq_rel)) {
            auto &snapshot=snapshots_[i]; snapshot.references.store(1,std::memory_order_release);
            snapshot.state={}; snapshot.outputs={}; snapshot.generation=0; snapshot.channel_count=0;
            return &snapshot;
        }
    }
    return nullptr;
}
void PublishGate::discard(Stage *stage) noexcept {
    if(!stage) return;
    for(auto &input:stage->inputs) input.reset();
    Snapshot::release(stage->base); Snapshot::release(stage->reserved);
    stage->base=stage->reserved=nullptr;
    stage->used.store(false,std::memory_order_release);
}
ars_status PublishGate::sanitize(StagedState &state) {
    std::uint16_t invalid=0;
    for(std::uint32_t i=0;i<state.provenance.count;++i) {
        const auto &key=state.provenance.keys[i]; core::ObjectLease pin;
        auto status=authority_.pin_source(key.source,&pin);
        bool valid=false;
        if(status==ARS_OK) {
            auto *source=static_cast<security::SourceAuthority::Source *>(pin.payload());
            std::lock_guard source_lock(source->mutex);
            const bool dirty=source->tombstone.dirty.load(std::memory_order_acquire);
            const auto through=source->tombstone.invalidated_through.load(std::memory_order_acquire);
            auto time=authority_.clock_.observe(); if(time.status!=ARS_OK) return time.status;
            (void)authority_.observe_source(*source,time.now);
            valid=(!dirty || through<key.epoch) && source->state==security::SourceState::active &&
                source->epoch==key.epoch && security::same(source->character.id,key.character.id);
        } else if(status==ARS_E_BUSY) return status;
        if(!valid) invalid=static_cast<std::uint16_t>(invalid|(1U<<i));
    }
    state.purge(invalid); return ARS_OK;
}
ars_status PublishGate::prepare(ars_character_h character_id,std::span<const ars_source_admin_h> sources,const Program &program,Stage **out) {
    *out=nullptr;
    if(sources.size()>max_dependencies || program.count>max_nodes) return ARS_E_OUT_OF_BUDGET;
    for(std::size_t i=0;i<sources.size();++i) for(std::size_t j=0;j<i;++j)
        if(security::same(sources[i].id,sources[j].id)) return ARS_E_INVALID_ARGUMENT;
    Stage *stage=nullptr;
    for(std::uint32_t i=0;i<stage_limit_;++i) {
        bool free=false;
        if(stages_[i].used.compare_exchange_strong(free,true,std::memory_order_acq_rel)) { stage=&stages_[i]; break; }
    }
    if(!stage) return ARS_E_BUSY;
    stage->candidate={}; stage->base=nullptr; stage->lock_count=0; stage->reserved=reserve();
    if(!stage->reserved) { discard(stage); return ARS_E_OUT_OF_BUDGET; }
    auto fail=[&](ars_status status) { discard(stage); return status; };
    auto status=authority_.pin_character(character_id,&stage->reserved->character);
    if(status!=ARS_OK) return fail(status);
    auto *character=static_cast<core::CharacterAuthority *>(stage->reserved->character.payload());
    {
        std::lock_guard pose(character->pose);
        if(!character->alive || authority_.admission_.closing()) status=ARS_E_CLOSED;
        else {
            stage->expected_generation=character->pose_generation;
            stage->base=character->snapshot; Snapshot::retain(stage->base);
        }
    }
    if(status!=ARS_OK) return fail(status);
    if(stage->base) stage->candidate.state=stage->base->state;
    status=sanitize(stage->candidate.state); if(status!=ARS_OK) return fail(status);
    stage->candidate.input_count=static_cast<std::uint32_t>(sources.size());
    for(std::uint32_t i=0;i<sources.size();++i) {
        status=authority_.pin_source(sources[i],&stage->inputs[i]);
        if(status!=ARS_OK) return fail(status);
        auto *source=static_cast<security::SourceAuthority::Source *>(stage->inputs[i].payload());
        {
            std::lock_guard source_lock(source->mutex);
            auto time=authority_.clock_.observe(); status=time.status;
            if(status==ARS_OK) status=authority_.observe_source(*source,time.now);
            if(status==ARS_OK && !security::same(source->character.id,character_id.id)) status=ARS_E_SCOPE_DENIED;
            if(status==ARS_OK && authority_.queues_[source->data].queue.size()) {
                // Bounded, tracked input view; never pop the live queue here.
                stage->queue_view=authority_.queues_[source->data].queue;
                stage->candidate.present[i]=stage->queue_view.pop(&stage->candidate.inputs[i]);
            }
        }
        if(status!=ARS_OK) return fail(status);
    }
    status=stage->candidate.evaluate(program,std::span(character->catalog.data(),character->count));
    if(status!=ARS_OK) return fail(status);
    auto &snapshot=*stage->reserved;
    snapshot.state=stage->candidate.state; snapshot.outputs=stage->candidate.outputs; snapshot.channel_count=character->count;
    const auto count=snapshot.state.provenance.count;
    auto include_lock=[&](void *source,std::uint32_t slot) {
        for(std::uint32_t i=0;i<stage->lock_count;++i) if(stage->lock_order[i].source==source) return;
        stage->lock_order[stage->lock_count++]={source,slot};
    };
    for(std::uint32_t i=0;i<count;++i) {
        const auto &key=snapshot.state.provenance.keys[i];
        if(!security::same(key.character.id,character_id.id)) return fail(ARS_E_SCOPE_DENIED);
        status=authority_.pin_source(key.source,&snapshot.sources[i]); if(status!=ARS_OK) return fail(status);
        include_lock(snapshot.sources[i].payload(),key.source.id.slot);
    }
    for(std::uint32_t i=0;i<sources.size();++i) if(stage->candidate.present[i])
        include_lock(stage->inputs[i].payload(),sources[i].id.slot);
    std::sort(stage->lock_order.begin(),stage->lock_order.begin()+stage->lock_count,[](auto a,auto b) {
        return a.slot<b.slot;
    });
    *out=stage; return ARS_OK;
}
ars_status PublishGate::publish(Stage &stage) {
    auto &snapshot=*stage.reserved;
    auto *character=static_cast<core::CharacterAuthority *>(snapshot.character.payload());
    Snapshot *old=nullptr;
    {
        std::lock_guard commit(authority_.commit); std::lock_guard pose(character->pose);
        if(authority_.admission_.closing() || !character->alive) return ARS_E_CLOSED;
        if(authority_.callback_.active()) return ARS_E_BUSY;
        if(character->pose_exhausted) return ARS_E_HANDLE_EXHAUSTED;
        if(character->pose_generation!=stage.expected_generation) return ARS_E_BUSY;
        if(character->pose_generation==std::numeric_limits<std::uint64_t>::max()) {
            character->pose_exhausted=true; return ARS_E_HANDLE_EXHAUSTED;
        }
        std::array<std::unique_lock<std::mutex>,max_dependencies*2> source_locks;
        const auto count=snapshot.state.provenance.count;
        for(std::uint32_t i=0;i<stage.lock_count;++i) {
            auto *source=static_cast<security::SourceAuthority::Source *>(stage.lock_order[i].source);
            source_locks[i]=std::unique_lock(source->mutex);
        }
        auto time=authority_.clock_.observe(); if(time.status!=ARS_OK) return time.status;
        for(std::uint32_t i=0;i<count;++i) {
            auto *source=static_cast<security::SourceAuthority::Source *>(snapshot.sources[i].payload());
            const auto &key=snapshot.state.provenance.keys[i];
            const bool dirty=source->tombstone.dirty.load(std::memory_order_acquire);
            const auto through=source->tombstone.invalidated_through.load(std::memory_order_acquire);
            auto status=authority_.observe_source(*source,time.now); if(status!=ARS_OK) return status;
            if((dirty && through>=key.epoch) || source->epoch!=key.epoch ||
               !security::same(source->id.id,key.source.id) || !security::same(source->character.id,key.character.id)) return ARS_E_REVOKED;
        }
        for(std::uint32_t i=0;i<stage.candidate.input_count;++i) if(stage.candidate.present[i]) {
            auto *source=static_cast<security::SourceAuthority::Source *>(stage.inputs[i].payload());
            const auto &frame=stage.candidate.inputs[i];
            auto status=authority_.observe_source(*source,time.now); if(status!=ARS_OK) return status;
            if(source->epoch!=frame.epoch || !security::same(source->character.id,frame.character.id)) return ARS_E_REVOKED;
            if(source->front_ordinal!=frame.ordinal || !authority_.queues_[source->data].queue.size()) return ARS_E_BUSY;
        }
        // Every head/epoch was validated under the same complete source lock set.
        // No remaining operation can fail: input consumption and pose/state swap
        // become visible together when these gates are released.
        for(std::uint32_t i=0;i<stage.candidate.input_count;++i) if(stage.candidate.present[i]) {
            auto *source=static_cast<security::SourceAuthority::Source *>(stage.inputs[i].payload());
            auto &queue=authority_.queues_[source->data].queue;
            (void)queue.pop(&stage.candidate.inputs[i]);
            source->front_ordinal=queue.size()?stage.candidate.inputs[i].ordinal+1:0;
        }
        snapshot.generation=character->pose_generation+1;
        old=character->snapshot; character->snapshot=&snapshot; character->pose_generation=snapshot.generation;
        stage.reserved=nullptr;
    }
    Snapshot::release(old); return ARS_OK;
}
ars_status PublishGate::read(ars_character_h id,Snapshot **out) {
    *out=nullptr; core::ObjectLease pin; auto status=authority_.pin_character(id,&pin); if(status!=ARS_OK) return status;
    auto *character=static_cast<core::CharacterAuthority *>(pin.payload());
    std::lock_guard pose(character->pose);
    if(!character->alive || authority_.admission_.closing()) return ARS_E_CLOSED;
    if(!character->snapshot) return ARS_E_BUSY;
    *out=character->snapshot; Snapshot::retain(*out); return ARS_OK;
}
}
