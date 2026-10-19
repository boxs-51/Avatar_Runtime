#pragma once
#include "package_admission.hpp"
#include "physics_checkpoint.hpp"
#include <memory>
#include <thread>
namespace ars::moc3::admission {
class RetainedPhysics {
    struct Data;
    struct State;
    std::unique_ptr<Data> data_;
    std::shared_ptr<const State> state_;
    std::thread::id owner_;
    std::uint64_t generation_=0;
public:
    class Candidate {
        friend class RetainedPhysics;
        const RetainedPhysics* owner_=nullptr;
        std::uint64_t generation_=0;
        std::shared_ptr<const State> state_;
        std::vector<Parameter> parameters_;
        unsigned steps_=0;
    public:
        const auto& parameters() const noexcept {return parameters_;}
        unsigned steps() const noexcept {return steps_;}
    };
    RetainedPhysics(std::span<const std::uint8_t>,const std::vector<ParameterInfo>&,
                    assets::Resource&,const assets::Limits&,assets::Work&);
    ~RetainedPhysics();
    RetainedPhysics(const RetainedPhysics&)=delete;
    RetainedPhysics& operator=(const RetainedPhysics&)=delete;
    Candidate prepare(std::uint64_t,std::span<const Parameter>,bool enabled,bool reset) const;
    void commit(const Candidate&);
    PhysicsCheckpoint checkpoint(const Candidate&,const Digest&) const;
    void restore(const PhysicsCheckpoint&);
};
}
