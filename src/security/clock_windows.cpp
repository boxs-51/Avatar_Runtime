#include "security_clock.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ars::security {
ars_status native_clock_init(NativeClock *clock) noexcept {
    LARGE_INTEGER frequency{};
    if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0) return ARS_E_CLOCK_FAILURE;
    clock->frequency=static_cast<std::uint64_t>(frequency.QuadPart); return ARS_OK;
}
ars_status native_clock_now(void *user,std::uint64_t *out) noexcept {
    LARGE_INTEGER counter{};
    if(!QueryPerformanceCounter(&counter) || counter.QuadPart<0) return ARS_E_CLOCK_FAILURE;
    return counter_nanoseconds(static_cast<std::uint64_t>(counter.QuadPart),static_cast<NativeClock *>(user)->frequency,out);
}
}
