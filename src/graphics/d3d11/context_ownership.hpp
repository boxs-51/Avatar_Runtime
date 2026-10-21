#pragma once
#ifdef _WIN32
#include "teardown_receipt.hpp"
#include <utility>
namespace ars::graphics::d3d11 {
// One registered native host graph, never proof about unregistered native code.
class ContextOwnership final : public std::enable_shared_from_this<ContextOwnership> {
    using State=HostTeardownReceipt::State;
    std::shared_ptr<State> state_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    ContextOwnership(ID3D11Device *,ID3D11DeviceContext *,std::uint64_t,std::shared_ptr<const void>);
public:
    static constexpr std::size_t capacity=64;
    class Participant {
        std::shared_ptr<State> state_;
        friend class ContextOwnership;
    public:
        Participant()=default;
        Participant(const Participant &)=delete;
        Participant &operator=(const Participant &)=delete;
        Participant(Participant &&other) noexcept:state_(std::move(other.state_)){}
        Participant &operator=(Participant &&other) noexcept {if(this!=&other){reset();state_=std::move(other.state_);}return *this;}
        ~Participant(){reset();}
        void reset() noexcept;
        explicit operator bool() const noexcept {return bool(state_);}
    };
    class Operation {
        std::shared_ptr<ContextOwnership> owner_;
        Result result_=Result::ok;
        friend class ContextOwnership;
    public:
        Operation()=default;
        Operation(const Operation &)=delete;
        Operation &operator=(const Operation &)=delete;
        Operation(Operation &&other) noexcept:owner_(std::move(other.owner_)),result_(other.result_){}
        Operation &operator=(Operation &&)=delete;
        ~Operation();
        Result result() const noexcept{return result_;}
        ID3D11DeviceContext *context() const noexcept;
        ID3D11Device *device() const noexcept;
    };
    class Ticket {
        std::weak_ptr<State> state_;
        friend class ContextOwnership;
    public: Ticket()=default;
    };
    static Result create(ID3D11Device *,ID3D11DeviceContext *,std::uint64_t,
                         std::shared_ptr<const void>,std::shared_ptr<ContextOwnership> &);
    ~ContextOwnership();
    ContextOwnership(const ContextOwnership &)=delete;
    ContextOwnership &operator=(const ContextOwnership &)=delete;
    Result retain(Participant &);
    Operation enter(bool drain=false);
    Operation nested();
    Operation enter(const Ticket &);
    Ticket ticket() const;
    Result begin_close() noexcept;
    Result issue(HostTeardownReceipt &);
    HostTeardownReceipt::Authority authority() const{return HostTeardownReceipt::Authority(state_);}
    bool closing() const noexcept{return state_->closing.load(std::memory_order_acquire);}
    bool owns(ID3D11Device *d,ID3D11DeviceContext *c,std::uint64_t g) const noexcept {return admitted()&&device_.Get()==d&&context_.Get()==c&&state_->generation==g;}
    bool admitted() const noexcept{return on_owner() && state_->active_users>0;}
    bool on_owner() const noexcept{return state_->owner==std::this_thread::get_id();}
    std::size_t owners() const noexcept{return on_owner()?state_->gpu_owners:0;}
    std::size_t users() const noexcept{return on_owner()?state_->active_users:0;}
    std::uint64_t generation() const noexcept{return state_->generation;}
};
}
#endif
