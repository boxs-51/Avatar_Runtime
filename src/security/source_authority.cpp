#include "source_authority.hpp"
#include "../abi/api_validation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ars::security {
namespace {
thread_local bool capture_callback=false;
struct WorkspaceRelease {
    std::atomic<bool> *flag;
    ~WorkspaceRelease() { flag->store(false,std::memory_order_release); }
};
}
SourceAuthority::SourceAuthority(core::HandleRegistry &registry,const ars_runtime_desc &descriptor,std::uint64_t uid)
    :registry_(registry),descriptor_(descriptor),uid_(uid) { capture_init_=native_clock_init(&capture_clock_); }
ars_status SourceAuthority::pin_character(ars_character_h id,core::ObjectLease *out) {
    out->reset();
    if(id.id.runtime_uid && id.id.runtime_uid!=uid_) return ARS_E_STALE_HANDLE;
    std::lock_guard lock(objects_);
    auto status=registry_.pin(id.id,core::ObjectType::character,&out->registry_);
    if(status==ARS_OK) {
        auto *character=static_cast<core::CharacterAuthority *>(out->payload());
        character->users.fetch_add(1,std::memory_order_acq_rel); out->users_=&character->users;
    }
    return status;
}
ars_status SourceAuthority::pin_source(ars_source_admin_h id,core::ObjectLease *out) {
    out->reset();
    if(id.id.runtime_uid && id.id.runtime_uid!=uid_) return ARS_E_STALE_HANDLE;
    std::lock_guard lock(objects_);
    auto status=registry_.pin(id.id,core::ObjectType::source_admin,&out->registry_);
    if(status==ARS_OK) {
        auto *source=static_cast<Source *>(out->payload());
        source->users.fetch_add(1,std::memory_order_acq_rel); out->users_=&source->users;
    }
    return status;
}
ars_status SourceAuthority::bind(std::span<const SemanticChannel> catalog,ars_character_h *out) {
    if(out) *out={};
    if(!out) return ARS_E_INVALID_ARGUMENT;
    auto status=core::validate_catalog(catalog); if(status!=ARS_OK) return status;
    core::CharacterAuthority *character=nullptr; core::ObjectLease lease;
    {
        std::lock_guard lock(objects_);
        for(std::uint32_t i=0;i<character_limit_;++i)
            if(!characters_[i].allocated && characters_[i].users.load(std::memory_order_acquire)==0) { character=&characters_[i]; break; }
        if(!character) return ARS_E_OUT_OF_BUDGET;
        character->count=static_cast<std::uint32_t>(catalog.size());
        std::copy(catalog.begin(),catalog.end(),character->catalog.begin());
        character->alive=false;
        status=registry_.insert(core::ObjectType::character,character,&character->id.id);
        if(status!=ARS_OK) return status;
        character->allocated=true;
        status=registry_.pin(character->id.id,core::ObjectType::character,&lease.registry_);
        if(status!=ARS_OK) { registry_.invalidate(character->id.id,core::ObjectType::character); character->allocated=false; return status; }
        character->users.fetch_add(1,std::memory_order_acq_rel); lease.users_=&character->users;
    }
    {
        std::lock_guard commit_lock(commit); std::lock_guard pose(character->pose);
        if(!admission_.closing()) { character->alive=true; *out=character->id; return ARS_OK; }
    }
    std::lock_guard lock(objects_);
    registry_.invalidate(character->id.id,core::ObjectType::character); character->allocated=false;
    return ARS_E_CLOSED;
}
ars_status SourceAuthority::state_error(const Source &source) const noexcept {
    switch(source.state) {
        case SourceState::active: return ARS_OK;
        case SourceState::expired: return ARS_E_SOURCE_EXPIRED;
        case SourceState::exhausted: return ARS_E_HANDLE_EXHAUSTED;
        case SourceState::closed: return ARS_E_CLOSED;
        case SourceState::rotating: return ARS_E_BUSY;
        default: return ARS_E_REVOKED;
    }
}
void SourceAuthority::terminal(Source &source,SourceState state) noexcept {
    const auto old_epoch=source.epoch;
    ledger_.retire(source.ledger); source.ledger=ledger_capacity;
    if(source.data<max_sources) {
        auto &slot=queues_[source.data]; slot.queue.clear(); source.data=max_sources;
        slot.used.store(false,std::memory_order_release);
    }
    const auto maximum=std::numeric_limits<std::uint64_t>::max();
    if(source.epoch==maximum || source.control==maximum || source.ticket==maximum)
        state=SourceState::exhausted;
    else { ++source.epoch; ++source.control; ++source.ticket; }
    source.state=state; source.tombstone.publish(old_epoch);
}
ars_status SourceAuthority::observe_source(Source &source,std::uint64_t now) {
    if(now>=source.expiry && (source.state==SourceState::active || source.state==SourceState::rotating || source.state==SourceState::rotate_failed))
        terminal(source,SourceState::expired);
    return state_error(source);
}
ars_status SourceAuthority::destroy(ars_character_h id) {
    core::ObjectLease character_pin; auto status=pin_character(id,&character_pin);
    if(status!=ARS_OK) return status;
    auto *character=static_cast<core::CharacterAuthority *>(character_pin.payload());
    {
        std::lock_guard commit_lock(commit); std::lock_guard pose(character->pose);
        if(!character->alive) return ARS_E_STALE_HANDLE;
        character->alive=false;
        for(auto &source:sources_) {
            core::ObjectLease retained;
            {
                std::lock_guard objects(objects_);
                if(!source.allocated) continue;
                source.users.fetch_add(1,std::memory_order_acq_rel); retained.users_=&source.users;
            }
            std::lock_guard lock(source.mutex);
            if(same(source.character.id,id.id) && source.state!=SourceState::unused && source.state!=SourceState::closed)
                terminal(source,SourceState::closed);
        }
    }
    std::lock_guard lock(objects_);
    status=registry_.invalidate(id.id,core::ObjectType::character); character->allocated=false;
    return status;
}
void SourceAuthority::abandon(Source &source) {
    { std::lock_guard lock(source.mutex); terminal(source,SourceState::closed); }
    std::lock_guard objects(objects_);
    registry_.invalidate(source.id.id,core::ObjectType::source_admin); source.allocated=false;
}
ars_status SourceAuthority::register_source(ars_character_h id,const ars_source_desc *desc,ars_source_admin_h *out,ars_ingress_cap *cap) {
    if(out) *out={};
    if(cap) *cap={};
    if(!out || !cap || !ars::abi::valid(desc) || desc->allowed_channel_count>max_channels ||
       (desc->allowed_channel_count && !desc->allowed_channel_ids) || !valid_utf8(desc->diagnostic_label,256) ||
       !desc->max_channels_per_frame || desc->max_channels_per_frame>max_channels || !desc->max_frames_per_second ||
       desc->max_frames_per_second>240 || !desc->lease_ttl_ns || desc->lease_ttl_ns>max_ttl ||
       (desc->allowed_channel_count && desc->max_channels_per_frame>desc->allowed_channel_count)) return ARS_E_INVALID_ARGUMENT;
    core::ObjectLease character_pin; auto status=pin_character(id,&character_pin);
    if(status!=ARS_OK) return status;
    auto *character=static_cast<core::CharacterAuthority *>(character_pin.payload());
    std::array<bool,max_channels> allowed{};
    std::array<char,257> label{};
    if(desc->diagnostic_label.byte_count) std::memcpy(label.data(),desc->diagnostic_label.ptr,desc->diagnostic_label.byte_count);
    for(std::uint32_t i=0;i<desc->allowed_channel_count;++i) {
        const auto name=desc->allowed_channel_ids[i];
        if(!canonical_id(name)) return ARS_E_INVALID_ARGUMENT;
        std::uint32_t index=character->count;
        for(std::uint32_t j=0;j<character->count;++j)
            if(std::strlen(character->catalog[j].id.data())==name.byte_count && std::memcmp(character->catalog[j].id.data(),name.ptr,name.byte_count)==0) { index=j; break; }
        if(index==character->count || allowed[index]) return ARS_E_INVALID_ARGUMENT;
        allowed[index]=true;
    }
    Source *source=nullptr; std::uint32_t data=max_sources;
    core::ObjectLease source_pin;
    {
        std::lock_guard lock(objects_);
        for(std::uint32_t i=0;i<record_limit_;++i)
            if(!sources_[i].allocated && sources_[i].users.load(std::memory_order_acquire)==0) { source=&sources_[i]; break; }
        if(!source) return ARS_E_OUT_OF_BUDGET;
        for(std::uint32_t i=0;i<source_limit_;++i) {
            bool free=false;
            if(queues_[i].used.compare_exchange_strong(free,true,std::memory_order_acq_rel)) { data=i; break; }
        }
        if(data==max_sources) return ARS_E_OUT_OF_BUDGET;
        queues_[data].queue.clear();
        source->character=id; source->state=SourceState::unused;
        source->epoch=source->control=source->ticket=1;
        source->sequence=source->ordinal=0; source->ledger=ledger_capacity; source->data=data;
        source->channels=desc->max_channels_per_frame; source->rate=desc->max_frames_per_second;
        source->priority=desc->fixed_priority; source->allowed=allowed; source->label=label;
        source->tombstone.invalidated_through.store(0,std::memory_order_relaxed);
        source->tombstone.dirty.store(false,std::memory_order_relaxed);
        status=registry_.insert(core::ObjectType::source_admin,source,&source->id.id);
        if(status!=ARS_OK) { queues_[data].used.store(false,std::memory_order_release); source->data=max_sources; return status; }
        source->allocated=true;
        status=registry_.pin(source->id.id,core::ObjectType::source_admin,&source_pin.registry_);
        if(status!=ARS_OK) {
            registry_.invalidate(source->id.id,core::ObjectType::source_admin); source->allocated=false;
            queues_[data].used.store(false,std::memory_order_release); source->data=max_sources; return status;
        }
        source->users.fetch_add(1,std::memory_order_acq_rel); source_pin.users_=&source->users;
    }
    const auto source_id=source->id;
    std::uint64_t reserved_epoch=0,reserved_control=0,reserved_ticket=0;
    const auto character_id=character->id;
    {
        std::lock_guard commit_lock(commit); std::lock_guard pose(character->pose); std::lock_guard lock(source->mutex);
        auto time=clock_.observe(); status=time.status;
        if(admission_.closing()) status=ARS_E_CLOSED;
        else if(!character->alive) status=ARS_E_STALE_HANDLE;
        else if(status==ARS_OK) {
            if(desc->lease_ttl_ns>std::numeric_limits<std::uint64_t>::max()-time.now) status=ARS_E_INVALID_ARGUMENT;
            else {
                source->expiry=time.now+desc->lease_ttl_ns; source->refill=time.now;
                source->credit=static_cast<std::uint64_t>(source->rate)*credit_unit;
                source->state=SourceState::rotating;
                reserved_epoch=source->epoch; reserved_control=source->control; reserved_ticket=source->ticket;
            }
        }
    }
    if(status!=ARS_OK) { abandon(*source); return status; }
    TokenLedger::Reservation reservation;
    status=ledger_.reserve(source_id,&reservation);
    {
        std::lock_guard commit_lock(commit); std::lock_guard pose(character->pose); std::lock_guard lock(source->mutex);
        auto time=clock_.observe();
        if(admission_.closing() || !character->alive) status=ARS_E_CLOSED;
        else if(time.status!=ARS_OK) status=time.status;
        else if(time.now>=source->expiry) { terminal(*source,SourceState::expired); status=state_error(*source); }
        else if(source->state!=SourceState::rotating || source->epoch!=reserved_epoch || source->control!=reserved_control ||
                source->ticket!=reserved_ticket || !same(source->id.id,source_id.id) || !same(character->id.id,character_id.id))
            { status=state_error(*source); if(status==ARS_OK) status=ARS_E_REVOKED; }
        else if(status==ARS_OK && ledger_.activate(reservation)) {
            source->ledger=reservation.index; source->state=SourceState::active;
            *out=source_id; *cap=reservation.token; return ARS_OK;
        } else if(status==ARS_OK) status=ARS_E_REVOKED;
        ledger_.retire(reservation.index);
    }
    abandon(*source); return status;
}

ars_status SourceAuthority::rotate(ars_source_admin_h id,ars_ingress_cap *out) {
    if(out) *out={};
    if(!out) return ARS_E_INVALID_ARGUMENT;
    core::ObjectLease source_pin,character_pin;
    auto status=pin_source(id,&source_pin); if(status!=ARS_OK) return status;
    auto *source=static_cast<Source *>(source_pin.payload());
    status=pin_character(source->character,&character_pin); if(status!=ARS_OK) return status;
    auto *character=static_cast<core::CharacterAuthority *>(character_pin.payload());
    const auto character_id=character->id;
    std::uint64_t epoch=0,control=0,ticket=0;
    {
        std::lock_guard commit_lock(commit); std::lock_guard pose(character->pose); std::lock_guard lock(source->mutex);
        if(admission_.closing() || !character->alive) return ARS_E_CLOSED;
        auto time=clock_.observe(); if(time.status!=ARS_OK) return time.status;
        (void)observe_source(*source,time.now);
        if(source->state!=SourceState::active && source->state!=SourceState::rotate_failed) return state_error(*source);
        const auto maximum=std::numeric_limits<std::uint64_t>::max();
        if(source->epoch==maximum || source->control==maximum || source->ticket==maximum) {
            terminal(*source,SourceState::exhausted); return ARS_E_HANDLE_EXHAUSTED;
        }
        // A failed rotation retains its bounded queue reservation for safe retry.
        ledger_.retire(source->ledger); source->ledger=ledger_capacity;
        source->tombstone.publish(source->epoch);
        ++source->epoch; ++source->control; ++source->ticket;
        epoch=source->epoch; control=source->control; ticket=source->ticket;
        source->state=SourceState::rotating; queues_[source->data].queue.clear();
    }
    TokenLedger::Reservation reservation;
    const auto mint=ledger_.reserve(id,&reservation);
    {
        std::lock_guard commit_lock(commit); std::lock_guard pose(character->pose); std::lock_guard lock(source->mutex);
        status=mint;
        auto time=clock_.observe();
        if(admission_.closing() || !character->alive) status=ARS_E_CLOSED;
        else if(time.status!=ARS_OK) status=time.status;
        else {
            (void)observe_source(*source,time.now);
            if(source->state!=SourceState::rotating || source->epoch!=epoch || source->control!=control || source->ticket!=ticket ||
               !same(source->id.id,id.id) || !same(character->id.id,character_id.id) || !same(source->character.id,character_id.id))
                { status=state_error(*source); if(status==ARS_OK) status=ARS_E_REVOKED; }
            else if(mint==ARS_OK && ledger_.activate(reservation)) {
                source->ledger=reservation.index; source->state=SourceState::active;
                source->sequence=0; source->credit=static_cast<std::uint64_t>(source->rate)*credit_unit;
                source->refill=time.now; *out=reservation.token; return ARS_OK;
            } else if(mint==ARS_OK) status=ARS_E_REVOKED;
        }
        ledger_.retire(reservation.index);
        if(source->state==SourceState::rotating && source->epoch==epoch && source->control==control && source->ticket==ticket)
            source->state=SourceState::rotate_failed;
    }
    return status;
}
ars_status SourceAuthority::revoke(ars_source_admin_h id) {
    core::ObjectLease source_pin,character_pin;
    auto status=pin_source(id,&source_pin); if(status!=ARS_OK) return status;
    auto *source=static_cast<Source *>(source_pin.payload());
    status=pin_character(source->character,&character_pin);
    if(status!=ARS_OK) {
        std::lock_guard lock(source->mutex);
        if(source->state==SourceState::closed) return ARS_OK;
        return status;
    }
    auto *character=static_cast<core::CharacterAuthority *>(character_pin.payload());
    std::lock_guard pose(character->pose); std::lock_guard lock(source->mutex);
    if(source->state==SourceState::explicit_revoked || source->state==SourceState::closed) return ARS_OK;
    if(source->state==SourceState::expired || source->state==SourceState::exhausted) return state_error(*source);
    terminal(*source,SourceState::explicit_revoked); return source->state==SourceState::exhausted?ARS_E_HANDLE_EXHAUSTED:ARS_OK;
}
ars_status SourceAuthority::release(ars_source_admin_h id) {
    core::ObjectLease source_pin; auto status=pin_source(id,&source_pin);
    if(status!=ARS_OK) return status;
    auto *source=static_cast<Source *>(source_pin.payload());
    status=revoke(id);
    if(status!=ARS_OK && status!=ARS_E_SOURCE_EXPIRED && status!=ARS_E_HANDLE_EXHAUSTED) return status;
    std::lock_guard objects(objects_);
    status=registry_.invalidate(id.id,core::ObjectType::source_admin);
    if(status==ARS_OK || status==ARS_E_HANDLE_EXHAUSTED) source->allocated=false;
    return status;
}
ars_status SourceAuthority::prepare(const ars_motion_frame *input,PreparedFrame *out) {
    if(!ars::abi::valid(input) || !input->channels || !input->channel_count || input->channel_count>max_channels ||
       (input->timestamp_mode!=ARS_TIMESTAMP_HOST_MONOTONIC && input->timestamp_mode!=ARS_TIMESTAMP_ON_ARRIVAL) ||
       !std::isfinite(input->confidence) || input->confidence<0 || input->confidence>1) return ARS_E_INVALID_ARGUMENT;
    out->frame={}; auto &frame=out->frame;
    frame.sequence=input->sequence; frame.timestamp=input->timestamp_ns;
    frame.confidence=input->confidence; frame.count=input->channel_count;
    std::uint64_t bytes=sizeof(*input)+static_cast<std::uint64_t>(input->channel_count)*sizeof(ars_motion_channel);
    for(std::uint32_t i=0;i<input->channel_count;++i) {
        const auto &channel=input->channels[i];
        if(!canonical_id(channel.semantic_id) || channel.reserved0 || !valid_value(channel.kind,channel.value)) return ARS_E_INVALID_FRAME;
        bytes+=channel.semantic_id.byte_count; if(bytes>max_input_bytes) return ARS_E_INVALID_FRAME;
        auto &name=out->names[i]; name.fill(0); std::memcpy(name.data(),channel.semantic_id.ptr,channel.semantic_id.byte_count);
        for(std::uint32_t j=0;j<i;++j) if(std::strcmp(name.data(),out->names[j].data())==0) return ARS_E_INVALID_FRAME;
        frame.channels[i].kind=channel.kind;
        std::copy(std::begin(channel.value),std::end(channel.value),frame.channels[i].value.begin());
    }
    if(input->timestamp_mode==ARS_TIMESTAMP_ON_ARRIVAL) {
        if(descriptor_.simulation_now_ns) {
            if(capture_callback) return ARS_E_BUSY;
            capture_callback=true;
            try { frame.timestamp=descriptor_.simulation_now_ns(descriptor_.clock_user_data); }
            catch(...) { capture_callback=false; return ARS_E_INTERNAL; }
            capture_callback=false;
        } else if(capture_init_!=ARS_OK || native_clock_now(&capture_clock_,&frame.timestamp)!=ARS_OK) return ARS_E_CLOCK_FAILURE;
    }
    return ARS_OK;
}
ars_status SourceAuthority::submit(ars_ingress_cap cap,const ars_motion_frame *input) {
    if(capture_callback) return ARS_E_BUSY;
    Workspace *workspace=nullptr;
    for(std::uint32_t i=0;i<workspace_limit_;++i) {
        bool free=false;
        if(workspaces_[i].used.compare_exchange_strong(free,true,std::memory_order_acq_rel)) { workspace=&workspaces_[i]; break; }
    }
    if(!workspace) return ARS_E_BUSY;
    WorkspaceRelease workspace_release{&workspace->used};
    auto status=prepare(input,&workspace->prepared); if(status!=ARS_OK) return status;
    ars_source_admin_h source_id{};
    if(!ledger_.lookup(cap,&source_id)) return ARS_E_SCOPE_DENIED;
    core::ObjectLease source_pin,character_pin;
    status=pin_source(source_id,&source_pin); if(status!=ARS_OK) return status==ARS_E_BUSY?status:ARS_E_SCOPE_DENIED;
    auto *source=static_cast<Source *>(source_pin.payload());
    status=pin_character(source->character,&character_pin); if(status!=ARS_OK) return status==ARS_E_BUSY?status:ARS_E_SCOPE_DENIED;
    auto *character=static_cast<core::CharacterAuthority *>(character_pin.payload());
    auto &frame=workspace->prepared.frame;
    // Catalog is immutable for the whole character pin; no pose/source locks.
    for(std::uint32_t i=0;i<frame.count;++i) {
        std::uint32_t index=character->count;
        for(std::uint32_t j=0;j<character->count;++j)
            if(std::strcmp(workspace->prepared.names[i].data(),character->catalog[j].id.data())==0) { index=j; break; }
        if(index==character->count) return ARS_E_SCOPE_DENIED;
        if(frame.channels[i].kind!=character->catalog[index].kind) return ARS_E_INVALID_FRAME;
        frame.channels[i].index=index;
    }
    AdmissionGate::Lease admission; status=admission_.enter(&admission); if(status!=ARS_OK) return status;
    std::lock_guard lock(source->mutex);
    if(source->state!=SourceState::active || source->ledger>=ledger_capacity) return ARS_E_SCOPE_DENIED;
    // Verify the current immutable ledger binding again after taking authority.
    if(!ledger_.lookup(cap,&source_id) || !same(source_id.id,source->id.id)) return ARS_E_SCOPE_DENIED;
    auto time=clock_.observe(); if(time.status!=ARS_OK) return time.status;
    status=observe_source(*source,time.now); if(status!=ARS_OK) return status;
    if(frame.count>source->channels) return ARS_E_SCOPE_DENIED;
    for(std::uint32_t i=0;i<frame.count;++i) if(!source->allowed[frame.channels[i].index]) return ARS_E_SCOPE_DENIED;
    if(frame.sequence<=source->sequence) return ARS_E_INVALID_FRAME;
    if(source->ordinal==std::numeric_limits<std::uint64_t>::max()) { terminal(*source,SourceState::exhausted); return ARS_E_HANDLE_EXHAUSTED; }
    const std::uint64_t capacity=static_cast<std::uint64_t>(source->rate)*credit_unit;
    // Saturate elapsed before multiplying: no overflow or denied-call charge.
    const auto elapsed=std::min(time.now-source->refill,credit_unit);
    const auto credit=std::min(capacity,source->credit+elapsed*source->rate);
    if(credit<credit_unit) return ARS_E_RATE_LIMIT;
    frame.source=source->id; frame.character=source->character; frame.epoch=source->epoch; frame.ordinal=source->ordinal+1;
    status=queues_[source->data].queue.append(frame); if(status!=ARS_OK) return status;
    source->sequence=frame.sequence; source->ordinal=frame.ordinal;
    source->credit=credit-credit_unit; source->refill=time.now; return ARS_OK;
}
ars_status SourceAuthority::dequeue(ars_source_admin_h id,OwnedFrame *out,core::ObjectLease *source_pin,core::ObjectLease *character_pin) {
    if(!out || !source_pin || !character_pin) return ARS_E_INVALID_ARGUMENT;
    *out={}; auto status=pin_source(id,source_pin); if(status!=ARS_OK) return status;
    auto *source=static_cast<Source *>(source_pin->payload());
    status=pin_character(source->character,character_pin); if(status!=ARS_OK) return status;
    std::lock_guard lock(source->mutex);
    auto time=clock_.observe(); if(time.status!=ARS_OK) return time.status;
    status=observe_source(*source,time.now); if(status!=ARS_OK) return status;
    return queues_[source->data].queue.pop(out)?ARS_OK:ARS_E_BUSY;
}
void SourceAuthority::request_close() { admission_.request_close(); }
void SourceAuthority::drain() { admission_.drain(); }
void SourceAuthority::close_all() {
    // Caller holds commit and has drained admissions. Internal references use
    // fixed context storage plus the caller's runtime lease, never raw handles.
    for(auto &character:characters_) {
        core::ObjectLease character_ref;
        {
            std::lock_guard objects(objects_);
            if(!character.allocated) continue;
            character.users.fetch_add(1,std::memory_order_acq_rel); character_ref.users_=&character.users;
        }
        std::lock_guard pose(character.pose); character.alive=false;
        for(auto &source:sources_) {
            core::ObjectLease source_ref;
            {
                std::lock_guard objects(objects_);
                if(!source.allocated) continue;
                source.users.fetch_add(1,std::memory_order_acq_rel); source_ref.users_=&source.users;
            }
            std::lock_guard lock(source.mutex);
            if(same(source.character.id,character.id.id) && source.state!=SourceState::unused && source.state!=SourceState::closed)
                terminal(source,SourceState::closed);
        }
    }
}
}
