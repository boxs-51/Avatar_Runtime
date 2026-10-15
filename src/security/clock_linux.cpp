#include "security_clock.hpp"
#include <ctime>
#include <limits>

namespace ars::security {
ars_status native_clock_init(NativeClock *) noexcept { return ARS_OK; }
ars_status native_clock_now(void *,std::uint64_t *out) noexcept {
    timespec time{};
    if(clock_gettime(CLOCK_MONOTONIC,&time)!=0 || time.tv_sec<0 || time.tv_nsec<0 || time.tv_nsec>=1000000000)
        return ARS_E_CLOCK_FAILURE;
    auto seconds=static_cast<std::uint64_t>(time.tv_sec),nanoseconds=static_cast<std::uint64_t>(time.tv_nsec);
    if(seconds>(std::numeric_limits<std::uint64_t>::max()-nanoseconds)/credit_unit) return ARS_E_CLOCK_FAILURE;
    *out=seconds*credit_unit+nanoseconds; return ARS_OK;
}
}
