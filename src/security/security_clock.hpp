#pragma once
#include "authority_types.hpp"
#include <mutex>

namespace ars::core { struct ComponentFixture; }
namespace ars::security {
struct NativeClock { std::uint64_t frequency=0; };
ars_status native_clock_init(NativeClock *) noexcept;
ars_status native_clock_now(void *,std::uint64_t *) noexcept;
ars_status counter_nanoseconds(std::uint64_t,std::uint64_t,std::uint64_t *) noexcept;
class SecurityClock {
    using Read=ars_status(*)(void *,std::uint64_t *) noexcept;
    NativeClock native_{};
    Read read_=native_clock_now;
    void *user_=&native_;
    ars_status init_=ARS_OK;
    std::uint64_t highwater_=0;
    bool fault_=false;
    std::mutex mutex_;
    friend struct ars::core::ComponentFixture;
public:
    struct Observation {
        std::unique_lock<std::mutex> lock;
        ars_status status;
        std::uint64_t now;
    };
    SecurityClock() noexcept;
    Observation observe();
    ars_status sample(std::uint64_t *);
};
}
