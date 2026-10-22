#include "context_ownership.hpp"
#ifdef _WIN32
#include <array>
#include <mutex>
#include <exception>
#include <new>
namespace ars::graphics::d3d11 {
ContextOwnership::ContextOwnership(ID3D11Device *d,ID3D11DeviceContext *c,std::uint64_t g,std::shared_ptr<const void> pin)
    :state_(std::make_shared<State>(g,d,c,std::move(pin))),device_(d),context_(c){}
ContextOwnership::~ContextOwnership(){if(!on_owner())std::terminate();}
Result ContextOwnership::create(ID3D11Device *d,ID3D11DeviceContext *c,std::uint64_t g,
                               std::shared_ptr<const void> pin,std::shared_ptr<ContextOwnership> &out) {
    if(out || !d || !c || !g || !pin)return Result::invalid_argument;
    Microsoft::WRL::ComPtr<ID3D11Device> actual;Microsoft::WRL::ComPtr<IUnknown> expected_id,actual_id;
    c->GetDevice(&actual);
    if(c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE || !actual || FAILED(d->QueryInterface(IID_PPV_ARGS(&expected_id))) ||
       FAILED(actual.As(&actual_id)) || expected_id.Get()!=actual_id.Get())return Result::invalid_argument;
    try {
        auto candidate=std::shared_ptr<ContextOwnership>(new ContextOwnership(d,c,g,std::move(pin)));
        // No D3D/user callback under the canonical registry lock.
        struct Entry {ID3D11DeviceContext *context=nullptr;std::weak_ptr<State> state;};
        static std::mutex mutex;static std::array<Entry,capacity> domains;
        std::lock_guard lock(mutex);std::size_t free=capacity;
        for(std::size_t i=0;i<domains.size();++i){
            if(domains[i].state.expired()){if(free==capacity)free=i;}else if(domains[i].context==c)return Result::busy;}
        if(free==capacity)return Result::out_of_budget;
        domains[free]={c,candidate->state_};out=std::move(candidate);return Result::ok;
    }catch(const std::bad_alloc &){return Result::out_of_budget;}
}
void ContextOwnership::Participant::reset() noexcept {
    if(!state_)return;if(state_->owner!=std::this_thread::get_id() || !state_->gpu_owners)std::terminate();
    --state_->gpu_owners;state_.reset();
}
Result ContextOwnership::retain(Participant &out) {
    if(!on_owner())return Result::wrong_thread;
    if(out || closing() || state_->issued)return Result::busy;
    if(state_->gpu_owners==capacity)return Result::out_of_budget;
    out.state_=state_;++state_->gpu_owners;return Result::ok;
}
ContextOwnership::Operation ContextOwnership::enter(bool drain) {
    Operation op;
    if(!on_owner())op.result_=Result::wrong_thread;
    else if(state_->issued || (closing()!=drain) || (drain && state_->active_users))op.result_=Result::busy;
    else if(state_->active_users==capacity)op.result_=Result::out_of_budget;
    else {op.owner_=shared_from_this();++state_->active_users;}
    return op;
}
ContextOwnership::Operation ContextOwnership::nested() {
    Operation op;
    if(!on_owner())op.result_=Result::wrong_thread;
    else if(!state_->active_users || state_->issued)op.result_=Result::busy;
    else if(state_->active_users==capacity)op.result_=Result::out_of_budget;
    else {op.owner_=shared_from_this();++state_->active_users;}
    return op;
}
ContextOwnership::Operation::~Operation(){if(owner_){if(!owner_->on_owner() || !owner_->state_->active_users)std::terminate();--owner_->state_->active_users;}}
ID3D11DeviceContext *ContextOwnership::Operation::context() const noexcept{return owner_&&owner_->on_owner()?owner_->context_.Get():nullptr;}
ID3D11Device *ContextOwnership::Operation::device() const noexcept{return owner_&&owner_->on_owner()?owner_->device_.Get():nullptr;}
ContextOwnership::Ticket ContextOwnership::ticket() const {Ticket t;if(on_owner()&&!closing()&&!state_->issued)t.state_=state_;return t;}
ContextOwnership::Operation ContextOwnership::enter(const Ticket &ticket) {
    if(ticket.state_.lock()!=state_){Operation op;op.result_=Result::invalid_argument;return op;}
    return enter();
}
Result ContextOwnership::begin_close() noexcept {state_->closing.store(true,std::memory_order_release);return Result::ok;}
Result ContextOwnership::issue(HostTeardownReceipt &out) {
    if(!on_owner())return Result::wrong_thread;
    if(out.state_ || !closing() || state_->issued || !device_ || !context_)return Result::invalid_argument;
    if(state_->active_users || state_->gpu_owners)return Result::busy;
    context_->ClearState();context_.Reset();device_.Reset();
    state_->issued=true;out=HostTeardownReceipt(state_);return Result::ok;
}
}
#endif
