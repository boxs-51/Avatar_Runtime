#pragma once
#include "ars/ars.h"
#include "runtime_lifecycle.hpp"
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace ars::core {
// Defined only in test translation units, before any fixture handle/lease exists.
// There is no production counter override, reset entry point or test export.
struct ComponentFixture {
    static void initial_uid(RuntimeRouter &router,std::uint64_t uid) {
        router.next_uid_=uid; router.uid_exhausted_=uid==0;
    }
    static void initial_generation(HandleRegistry &registry,std::uint32_t generation) {
        for(auto &slot:registry.slots_) {
            slot.generation=generation; slot.retired=generation==0;
        }
    }
};
}

namespace test {
inline void check(bool ok,const char *id) {
    std::printf("%s %s\n",ok?"PASS":"FAIL",id);
    if(!ok) std::exit(EXIT_FAILURE);
}
inline void status(ars_status expected,ars_status actual,const char *id) {
    std::printf("%s %s expected=%d actual=%d\n",expected==actual?"PASS":"FAIL",id,
                static_cast<int>(expected),static_cast<int>(actual));
    if(expected!=actual) std::exit(EXIT_FAILURE);
}
class Gate {
    std::mutex mutex_;
    std::condition_variable cv_;
    bool entered_=false,released_=false;
public:
    void arrive_and_wait() {
        std::unique_lock lock(mutex_); entered_=true; cv_.notify_all();
        cv_.wait(lock,[&]{return released_;});
    }
    void wait_entered() { std::unique_lock lock(mutex_); cv_.wait(lock,[&]{return entered_;}); }
    void release() { std::lock_guard lock(mutex_); released_=true; cv_.notify_all(); }
};
}
