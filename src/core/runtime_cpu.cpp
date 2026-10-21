#include "runtime_state.hpp"
#include "cpu_context.hpp"
#include "../security/source_authority.hpp"
#include "../abi/api_validation.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
namespace ars::core {
class CpuOperation {
    Runtime *runtime_=nullptr;
  public:
    bool enter(Runtime *r) noexcept {if(r->cpu_operation.test_and_set(std::memory_order_acq_rel))return false;runtime_=r;return true;}
    ~CpuOperation(){if(runtime_)runtime_->cpu_operation.clear(std::memory_order_release);}
};
namespace {
struct Callback {
    Runtime *runtime;
    bool armed;
    explicit Callback(Runtime *r,bool enabled=true):runtime(r),armed(enabled){if(armed)runtime->asset_callback.store(true,std::memory_order_release);}
    ~Callback(){if(armed)runtime->asset_callback.store(false,std::memory_order_release);}
};
void *ARS_CALL relay_allocate(void *user,std::uint64_t bytes,std::uint32_t alignment) {
    auto *r=static_cast<Runtime *>(user);Callback callback(r,r->cpu_operation.test(std::memory_order_acquire));
    return r->host_descriptor.alloc?r->host_descriptor.alloc(r->host_descriptor.allocator_user_data,bytes,alignment):
        ::operator new(static_cast<std::size_t>(bytes),std::align_val_t{alignment},std::nothrow);
}
void ARS_CALL relay_release(void *user,void *p,std::uint64_t bytes,std::uint32_t alignment) {
    auto *r=static_cast<Runtime *>(user);Callback callback(r,r->cpu_operation.test(std::memory_order_acquire));
    if(r->host_descriptor.free_fn)r->host_descriptor.free_fn(r->host_descriptor.allocator_user_data,p,bytes,alignment);
    else ::operator delete(p,std::align_val_t{alignment});
}
CpuContext::Character *find_character(CpuContext &c,ars_character_h id) {
    for(auto &record:c.characters)if(record.live && security::same(record.id.id,id.id))return &record;
    return nullptr;
}
ars_status mode(const assets::ValidatedCpuModel &model,std::uint32_t requested,std::uint32_t *out) {
    if(requested<ARS_MODE_2D || requested>ARS_MODE_AUTO)return ARS_E_INVALID_ARGUMENT;
    auto actual=requested==ARS_MODE_AUTO?(model.default_mode()==0?ARS_MODE_2D:ARS_MODE_3D):requested;
    if(actual==ARS_MODE_2D?model.parts().empty():!model.glb())return ARS_E_UNSUPPORTED;
    *out=static_cast<std::uint32_t>(actual);return ARS_OK;
}
std::uint32_t kind(std::string_view s) {
    return s=="float"?ARS_VALUE_SCALAR:s=="bool"?ARS_VALUE_BOOL:s=="vec2"?ARS_VALUE_VEC2:s=="vec3"?ARS_VALUE_VEC3:s=="quat"?ARS_VALUE_QUAT_XYZW:ARS_VALUE_TRIGGER;
}
} // namespace
ars_runtime_desc cpu_descriptor(const ars_runtime_desc &desc,Runtime *runtime) noexcept {
    auto wrapped=desc;wrapped.alloc=relay_allocate;wrapped.free_fn=relay_release;wrapped.allocator_user_data=runtime;return wrapped;
}
void free_cpu_context(Runtime *runtime) noexcept {
    auto *c=runtime->cpu;if(!c)return;
    // finish_close first drains runtime pins and destroys publication/authority.
    // All model references die while the Resource/allocator relay still lives.
    c->~CpuContext();try{relay_release(runtime,c,sizeof(CpuContext),alignof(CpuContext));}catch(...){}
    runtime->cpu=nullptr;
}
ars_status RuntimeRouter::cpu_enter(ars_runtime_h handle,bool simulation,RuntimeLease *lease,CpuOperation *operation,CpuContext **out) {
    *out=nullptr;auto status=acquire(handle,LeaseKind::work,lease);if(status!=ARS_OK)return status;
    auto *r=lease->runtime_;
    if(!simulation && r->control!=std::this_thread::get_id())return ARS_E_WRONG_THREAD;
    if(!operation->enter(r))return ARS_E_BUSY;
    if(r->cpu && r->cpu->quarantine)return ARS_E_BUSY;
    if(!r->cpu) {
        void *storage=relay_allocate(r,sizeof(CpuContext),alignof(CpuContext));if(!storage)return ARS_E_OUT_OF_BUDGET;
        if(reinterpret_cast<std::uintptr_t>(storage)%alignof(CpuContext)) {relay_release(r,storage,sizeof(CpuContext),alignof(CpuContext));return ARS_E_INVALID_ARGUMENT;}
        try{r->cpu=new(storage)CpuContext(relay_allocate,relay_release,r);}catch(...){relay_release(r,storage,sizeof(CpuContext),alignof(CpuContext));throw;}
    }
    if(simulation && r->cpu->simulation_bound && r->cpu->simulation!=std::this_thread::get_id())return ARS_E_WRONG_THREAD;
    if(auto *authority=authority_for(*lease))authority->collect_retired();
    *out=r->cpu;return ARS_OK;
}
ars_status RuntimeRouter::cpu_reap(ars_runtime_h handle) {
    RuntimeLease lease;auto status=acquire(handle,LeaseKind::management,&lease);if(status!=ARS_OK)return status;
    auto *r=lease.runtime_;if(r->asset_callback.load(std::memory_order_acquire))return ARS_E_BUSY;
    CpuOperation operation;if(!operation.enter(r))return ARS_E_BUSY;
    if(!assets::AdmissionCustody::reap_orphan())return ARS_E_BUSY;
    if(r->cpu && r->cpu->quarantine) {
        if(!r->cpu->quarantine->reap())return ARS_E_BUSY;
        r->cpu->quarantine.reset();r->cpu->quarantine_pin.reset();
    } else if(r->cpu) r->cpu->quarantine_pin.reset();
    return ARS_OK;
}
ars_status RuntimeRouter::cpu_load(ars_runtime_h handle,const ars_asset_source *source,ars_model_h *out) {
    if(out)*out={};
    if(!out || !ars::abi::valid(source) || source->kind!=ARS_ASSET_MEMORY_BUNDLE || !source->bytes || !source->byte_count)return ARS_E_INVALID_ARGUMENT;
    if(source->byte_count>assets::packet::byte_cap)return ARS_E_OUT_OF_BUDGET;
    RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,false,&lease,&operation,&c);if(status!=ARS_OK)return status;
    auto *r=lease.runtime_;CpuContext::Model *record=nullptr;for(auto &m:c->models)if(!m.live){record=&m;break;}
    if(!record)return ARS_E_OUT_OF_BUDGET;
    std::shared_ptr<const assets::ValidatedCpuModel> candidate;
    status=assets::isolated_cpu_load({source->bytes,static_cast<std::size_t>(source->byte_count)},c->memory,candidate,c->quarantine);
    if(c->quarantine){c->quarantine_pin=std::move(lease);return status==ARS_OK?ARS_E_BUSY:status;}
    if(status!=ARS_OK)return status;
    {
        std::lock_guard lock(mutex_);if(r->closing)return ARS_E_CLOSED;
        status=r->registry.insert(ObjectType::model,record,&record->id.id);
        if(status==ARS_OK){record->value=std::move(candidate);record->live=true;*out=record->id;}
    }
    return status;
}
ars_status RuntimeRouter::cpu_release(ars_runtime_h handle,ars_model_h id) {
    RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,false,&lease,&operation,&c);if(status!=ARS_OK)return status;
    RegistryLease pin;status=lease.runtime_->registry.pin(id.id,ObjectType::model,&pin);if(status!=ARS_OK)return status;
    auto *m=static_cast<CpuContext::Model *>(pin.payload());status=lease.runtime_->registry.invalidate(id.id,ObjectType::model);if(status!=ARS_OK)return status;
    auto retired=std::move(m->value);m->live=false;return ARS_OK;
}
ars_status RuntimeRouter::cpu_create_character(ars_runtime_h handle,ars_model_h id,const ars_character_desc *desc,ars_character_h *out) {
    if(out)*out={};if(!out || !ars::abi::valid(desc))return ARS_E_INVALID_ARGUMENT;
    RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,false,&lease,&operation,&c);if(status!=ARS_OK)return status;
    RegistryLease pin;status=lease.runtime_->registry.pin(id.id,ObjectType::model,&pin);if(status!=ARS_OK)return status;
    auto model=static_cast<CpuContext::Model *>(pin.payload())->value;
    std::uint32_t actual=0;status=mode(*model,desc->initial_mode,&actual);if(status!=ARS_OK)return status;
    if(model->semantics().size()>security::max_channels)return ARS_E_UNSUPPORTED;
    CpuContext::Character *record=nullptr;for(auto &ch:c->characters)if(!ch.live){record=&ch;break;}if(!record)return ARS_E_OUT_OF_BUDGET;
    std::array<security::SemanticChannel,security::max_channels> catalog{};
    for(std::size_t i=0;i<model->semantics().size();++i){const auto &s=model->semantics()[i];auto &v=catalog[i];
        std::memcpy(v.id.data(),s.id.data(),s.id.size());v.kind=kind(s.type);for(unsigned j=0;j<4;++j)v.neutral[j]=static_cast<float>(s.initial[j]);}
    security::SourceAuthority *authority=nullptr;status=ensure_authority(lease,&authority);if(status!=ARS_OK)return status;
    status=authority->bind(std::span(catalog.data(),model->semantics().size()),out,model,true);if(status!=ARS_OK)return status;
    record->id=*out;record->live=true;record->mode=actual;record->model=std::move(model);
    for(std::size_t i=0;i<record->model->layers().size();++i){const auto &l=record->model->layers()[i];record->layers[i]={l.local_visible,l.local_opacity,l.z};}
    return ARS_OK;
}
ars_status RuntimeRouter::cpu_destroy_character(ars_runtime_h handle,ars_character_h id) {
    RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,false,&lease,&operation,&c);if(status!=ARS_OK)return status;
    status=lease.runtime_->registry.check(id.id,ObjectType::character);if(status!=ARS_OK)return status;
    auto *ch=find_character(*c,id);if(!ch)return ARS_E_STALE_HANDLE;
    status=destroy_character(handle,id);if(status!=ARS_OK)return status;
    ch->live=false;auto retired=std::move(ch->model);
    for(auto &s:c->sources)if(s.live && security::same(s.character.id,id.id))s.cache={};
    if(auto *a=authority_for(lease))a->collect_retired();return ARS_OK;
}
ars_status RuntimeRouter::cpu_mode(ars_runtime_h handle,ars_character_h id,std::uint32_t requested) {
    RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,false,&lease,&operation,&c);if(status!=ARS_OK)return status;
    status=lease.runtime_->registry.check(id.id,ObjectType::character);if(status!=ARS_OK)return status;
    auto *ch=find_character(*c,id);if(!ch)return ARS_E_STALE_HANDLE;std::uint32_t actual=0;status=mode(*ch->model,requested,&actual);if(status==ARS_OK)ch->mode=actual;return status;
}
ars_status RuntimeRouter::cpu_layer(ars_runtime_h handle,ars_character_h id,const ars_layer_change *desc) {
    if(!ars::abi::valid(desc) || desc->visible>1 || !std::isfinite(desc->opacity) || desc->opacity<0 || desc->opacity>1 || desc->z_index < -10000 || desc->z_index>10000 || !security::valid_utf8(desc->layer_id,128))return ARS_E_INVALID_ARGUMENT;
    RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,false,&lease,&operation,&c);if(status!=ARS_OK)return status;
    status=lease.runtime_->registry.check(id.id,ObjectType::character);if(status!=ARS_OK)return status;
    auto *ch=find_character(*c,id);if(!ch)return ARS_E_STALE_HANDLE;if(ch->mode!=ARS_MODE_2D)return ARS_E_UNSUPPORTED;
    const std::string_view name(desc->layer_id.ptr,desc->layer_id.byte_count);
    for(std::size_t i=0;i<ch->model->layers().size();++i)if(ch->model->layers()[i].id==name){ch->layers[i]={desc->visible!=0,desc->opacity,desc->z_index};return ARS_OK;}
    return ARS_E_INVALID_ARGUMENT;
}
ars_status RuntimeRouter::cpu_register_source(ars_runtime_h handle,ars_character_h character,const ars_source_desc *desc,ars_source_admin_h *out,ars_ingress_cap *cap) {
    if(out)*out={};if(cap)*cap={};if(!out || !cap || !ars::abi::valid(desc))return ARS_E_INVALID_ARGUMENT;
    RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,false,&lease,&operation,&c);if(status!=ARS_OK)return status;
    status=lease.runtime_->registry.check(character.id,ObjectType::character);if(status!=ARS_OK)return status;
    if(!find_character(*c,character))return ARS_E_STALE_HANDLE;
    CpuContext::Source *record=nullptr;for(auto &s:c->sources)if(!s.live){record=&s;break;}if(!record)return ARS_E_OUT_OF_BUDGET;
    const auto copy=*desc;status=register_source(handle,character,&copy,out,cap);if(status==ARS_OK){*record={};record->live=true;record->id=*out;record->character=character;record->priority=copy.fixed_priority;}return status;
}
ars_status RuntimeRouter::cpu_source_control(ars_runtime_h handle,ars_source_admin_h id,unsigned command,ars_ingress_cap *out) {
    if(out)*out={};RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,false,&lease,&operation,&c);if(status!=ARS_OK)return status;
    status=lease.runtime_->registry.check(id.id,ObjectType::source_admin);if(status!=ARS_OK)return status;
    CpuContext::Source *record=nullptr;for(auto &s:c->sources)if(s.live && security::same(s.id.id,id.id)){record=&s;break;}if(!record)return ARS_E_STALE_HANDLE;
    status=command==0?rotate_source(handle,id,out):command==1?revoke_source(handle,id):release_source(handle,id);
    if(status==ARS_OK){record->cache={};if(command==2)record->live=false;}return status;
}
ars_status RuntimeRouter::cpu_update(ars_runtime_h handle,std::uint64_t time) {
    RuntimeLease lease;CpuOperation operation;CpuContext *c=nullptr;auto status=cpu_enter(handle,true,&lease,&operation,&c);if(status!=ARS_OK)return status;
    auto *r=lease.runtime_;
    if(!time && r->descriptor.simulation_now_ns) {
        Callback callback(r);
        status=security::sample_simulation_clock(r->callback,r->descriptor,time,ARS_E_CLOCK_FAILURE);
        if(status!=ARS_OK)return status;
    }
    if(c->has_time && time<c->simulation_time)return ARS_E_CLOCK_REGRESSION;
    BatchLease batch(c->batch);std::size_t prepared=0;
    auto *authority=authority_for(lease);
    for(auto &character:c->characters)if(character.live) {
        auto &resolution=c->resolutions[prepared++];resolution.count=0;resolution.constraints={};
        std::array<ars_source_admin_h,motion::max_dependencies> sources{};
        for(auto &source:c->sources)if(source.live && security::same(source.character.id,character.id.id)) {
            std::uint64_t epoch=0;status=authority->active_epoch(source.id,character.id,&epoch);
            if(status==ARS_E_REVOKED || status==ARS_E_SOURCE_EXPIRED || status==ARS_E_STALE_HANDLE || status==ARS_E_HANDLE_EXHAUSTED)continue;
            if(status!=ARS_OK)return status;
            const auto i=resolution.count++;if(i>=motion::max_dependencies)return ARS_E_OUT_OF_BUDGET;
            sources[i]=source.id;resolution.sources[i]={source.id,epoch,source.priority,&source.cache};
        }
        for(std::size_t i=0;i<character.model->semantics().size();++i){const auto &s=character.model->semantics()[i];resolution.constraints[i]={static_cast<float>(s.minimum),static_cast<float>(s.maximum),s.has_minimum,s.has_maximum};}
        motion::Program program;program.public_resolution=&resolution;StageLease stage;
        status=prepare_cpu(handle,character.id,std::span(sources.data(),resolution.count),program,&stage);if(status!=ARS_OK)return status;
        status=append_cpu_batch(&stage,&batch);if(status!=ARS_OK)return status;
    }
    struct Commit {CpuContext *context;std::thread::id thread;std::uint64_t time;} commit{c,std::this_thread::get_id(),time};
    auto finish=[](void *data) noexcept {auto &v=*static_cast<Commit *>(data);v.context->simulation=v.thread;v.context->simulation_bound=true;v.context->simulation_time=v.time;v.context->has_time=true;};
    if(prepared)return publish_cpu_batch(&batch,finish,&commit);
    { std::lock_guard lock(mutex_);if(r->closing)return ARS_E_CLOSED;finish(&commit); }
    return ARS_OK;
}
} // namespace ars::core
