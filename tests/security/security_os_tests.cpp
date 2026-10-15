#include "runtime_test_support.hpp"
#include "../../src/security/security_clock.hpp"
#include "../../src/security/os_entropy.hpp"
#include <limits>
#include <algorithm>
#include <cerrno>
#include <memory>

namespace {
struct Chunks {
    std::array<std::ptrdiff_t,8> result{};
    std::array<int,8> error{};
    unsigned calls=0;
    static std::ptrdiff_t read(void *user,std::span<std::byte> bytes,int *error) noexcept {
        auto &state=*static_cast<Chunks *>(user); const auto index=state.calls++;
        if(index>=state.result.size()) return -1;
        *error=state.error[index]; const auto count=state.result[index];
        if(count>0 && static_cast<std::size_t>(count)<=bytes.size()) std::fill_n(bytes.begin(),count,std::byte{0x5a});
        return count;
    }
};
}

int main() {
    ars::security::SecurityClock clock;
    std::uint64_t first=0,second=0;
    test::status(ARS_OK,clock.sample(&first),"OS-real-security-clock");
    test::status(ARS_OK,clock.sample(&second),"OS-second-security-clock");
    test::check(second>=first,"OS-native-monotonic-order");
    std::array<std::byte,16> bytes{};
    test::status(ARS_OK,ars::security::native_entropy(nullptr,bytes),"OS-real-128-bit-entropy");
    ars::security::wipe(bytes);
    test::check(std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==std::byte{0};}),"OS-secret-buffer-wiped");
    test::status(ARS_OK,ars::security::counter_nanoseconds(3,2,&first),"OS-exact-counter-fraction");
    test::check(first==1500000000ULL,"OS-counter-integer-result");
    test::status(ARS_E_CLOCK_FAILURE,ars::security::counter_nanoseconds(1,0,&first),"OS-zero-frequency");
    test::status(ARS_E_CLOCK_FAILURE,ars::security::counter_nanoseconds(std::numeric_limits<std::uint64_t>::max(),1,&first),"OS-ns-overflow");
    test::status(ARS_OK,ars::security::counter_nanoseconds(UINT64_MAX,1000000000,&first),"OS-exact-wide-counter");
    test::check(first==UINT64_MAX,"OS-wide-no-floating-rounding");
    test::status(ARS_OK,ars::security::counter_nanoseconds(UINT64_MAX-123,UINT64_MAX-17,&first),"OS-wide-remainder");
    test::check(first==999999999,"OS-wide-remainder-exact");
    {
        ars::security::SecurityClock isolated; test::Clock fake;
        ars::core::ComponentFixture::clock(isolated,test::Clock::read,&fake);
        fake.now=180; test::status(ARS_OK,isolated.sample(&first),"CLOCK-first-monotonic");
        test::status(ARS_OK,isolated.sample(&second),"CLOCK-equal-samples-allowed");
        fake.now=170; test::status(ARS_E_CLOCK_REGRESSION,isolated.sample(&first),"CLOCK-first-regression");
        fake.now=190; test::status(ARS_E_SECURITY_CLOCK_FAULT,isolated.sample(&first),"CLOCK-no-fault-reset");
        test::check(first==0,"CLOCK-failure-zero-output");
    }
    {
        ars::security::SecurityClock isolated; test::Clock fake;
        ars::core::ComponentFixture::clock(isolated,test::Clock::read,&fake);
        fake.fail=true; test::status(ARS_E_CLOCK_FAILURE,isolated.sample(&first),"CLOCK-native-error-injection");
        fake.fail=false; test::status(ARS_E_SECURITY_CLOCK_FAULT,isolated.sample(&first),"CLOCK-native-failure-sticky");
    }
    {
        ars::security::OsEntropy backend; Chunks script; script.result[0]=16;
        ars::core::ComponentFixture::chunks(backend,Chunks::read,&script);
        test::status(ARS_OK,backend.fill(bytes),"ENTROPY-platform-fill-through-real-loop");
        test::check(script.calls==1,"ENTROPY-complete-read-once");
        script={}; script.result[0]=-1; script.error[0]=EAGAIN;
        test::status(ARS_E_INTERNAL,backend.fill(bytes),"ENTROPY-unavailable-no-fallback");
        test::check(std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==std::byte{0};}),"ENTROPY-failure-zeroes-partial-buffer");
        script={}; script.result[0]=0;
        test::status(ARS_E_INTERNAL,backend.fill(bytes),"ENTROPY-zero-read-no-spin");
#ifndef _WIN32
        script={}; script.result[0]=-1; script.error[0]=EINTR; script.result[1]=5; script.result[2]=11;
        test::status(ARS_OK,backend.fill(bytes),"ENTROPY-EINTR-and-partial-getrandom");
        test::check(script.calls==3,"ENTROPY-partial-exact-progress");
        script={}; script.result.fill(-1); script.error.fill(EINTR);
        test::status(ARS_E_INTERNAL,backend.fill(bytes),"ENTROPY-EINTR-bounded-retries");
        test::check(script.calls==8,"ENTROPY-syscall-retry-budget");
        script={}; script.result[0]=17;
        test::status(ARS_E_INTERNAL,backend.fill(bytes),"ENTROPY-impossible-count-fails-closed");
#else
        script={}; script.result[0]=8;
        test::status(ARS_E_INTERNAL,backend.fill(bytes),"ENTROPY-BCrypt-incomplete-result-rejected");
#endif
    }
    {
        auto ledger=std::make_unique<ars::security::TokenLedger>(); test::Entropy fake;
        ars::core::ComponentFixture::entropy(*ledger,test::Entropy::read,&fake);
        ars::security::TokenLedger::Reservation reserved,next;
        ars_source_admin_h binding{{1,2,3}},found{};
        fake.zero=true;
        test::status(ARS_E_HANDLE_EXHAUSTED,ledger->reserve(binding,&reserved),"LEDGER-zero-token-never-issued");
        test::check(fake.calls==8,"LEDGER-zero-token-retry-bound"); fake.zero=false;
        test::status(ARS_OK,ledger->reserve(binding,&reserved),"LEDGER-unique-reservation");
        test::check(!ledger->lookup(reserved.token,&found),"LEDGER-pending-token-not-eligible");
        fake.constant=true; fake.next=reserved.token.lo;
        test::status(ARS_E_HANDLE_EXHAUSTED,ledger->reserve(binding,&next),"LEDGER-pending-collision-also-forbidden");
        test::check(ledger->activate(reserved) && ledger->lookup(reserved.token,&found) && ars::security::same(found.id,binding.id),"LEDGER-atomic-activation-binding");
        ledger->retire(reserved.index);
        test::check(!ledger->lookup(reserved.token,&found) && !ledger->activate(reserved),"LEDGER-terminal-reservation-no-reactivation");
        test::status(ARS_E_HANDLE_EXHAUSTED,ledger->reserve(binding,&next),"LEDGER-retired-token-never-reissued");
    }
    std::puts("PASS B2 native OS security backend smoke; no token values logged");
}
