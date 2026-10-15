#pragma once
#include "ars/ars.h"
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>

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
