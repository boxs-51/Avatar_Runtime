#include "security_clock.hpp"
#include <limits>

namespace ars::security {
ars_status counter_nanoseconds(std::uint64_t ticks,std::uint64_t frequency,std::uint64_t *out) noexcept {
    if(!out || !frequency) return ARS_E_CLOCK_FAILURE;
    const auto whole=ticks/frequency,part=ticks%frequency;
    if(whole>std::numeric_limits<std::uint64_t>::max()/credit_unit) return ARS_E_CLOCK_FAILURE;
    // Exact fractional multiply/divide without 128-bit compiler extensions.
    std::uint64_t quotient=0,remainder=0;
    for(int bit=29;bit>=0;--bit) {
        quotient*=2;
        if(remainder>=frequency-remainder) { remainder-=frequency-remainder; ++quotient; }
        else remainder*=2;
        if((credit_unit>>bit)&1U) {
            if(remainder>=frequency-part) { remainder-=frequency-part; ++quotient; }
            else remainder+=part;
        }
    }
    const auto base=whole*credit_unit;
    if(quotient>std::numeric_limits<std::uint64_t>::max()-base) return ARS_E_CLOCK_FAILURE;
    *out=base+quotient; return ARS_OK;
}
SecurityClock::SecurityClock() noexcept { init_=native_clock_init(&native_); }
SecurityClock::Observation SecurityClock::observe() {
    std::unique_lock lock(mutex_); std::uint64_t now=0;
    ars_status status=ARS_OK;
    if(fault_) status=ARS_E_SECURITY_CLOCK_FAULT;
    else if(init_!=ARS_OK || read_(user_,&now)!=ARS_OK) { fault_=true; status=ARS_E_CLOCK_FAILURE; }
    else if(now<highwater_) { fault_=true; status=ARS_E_CLOCK_REGRESSION; }
    else highwater_=now;
    return {std::move(lock),status,now};
}
ars_status SecurityClock::sample(std::uint64_t *out) {
    if(!out) return ARS_E_INVALID_ARGUMENT;
    auto observation=observe(); *out=observation.status==ARS_OK?observation.now:0;
    return observation.status;
}
}
