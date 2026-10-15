#pragma once
#include "authority_types.hpp"
#include <condition_variable>
#include <mutex>

namespace ars::security {
class AdmissionGate {
    std::mutex mutex_;
    std::condition_variable cv_;
    bool closing_=false;
    std::atomic<bool> requested_{false};
    std::uint32_t active_=0,limit_=max_admissions;
    void leave() noexcept;
    friend struct ars::core::ComponentFixture;
public:
    class Lease {
        AdmissionGate *owner_=nullptr;
        friend class AdmissionGate;
    public:
        Lease()=default;
        Lease(const Lease &)=delete;
        Lease &operator=(const Lease &)=delete;
        ~Lease();
    };
    ars_status enter(Lease *);
    void request_close();
    void drain();
    bool closing();
    std::uint32_t active();
};
}
