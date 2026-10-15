#include "ars/ars.h"
#include <array>
#include <barrier>
#include <cstdio>
#include <thread>

int main() {
    static_assert(sizeof(ars_runtime_h)==16 && sizeof(ars_status)==4);
    for(unsigned iteration=0;iteration<1000;++iteration) {
        ars_runtime_desc desc{}; desc.struct_size=sizeof(desc); desc.abi_version=ARS_ABI_VERSION;
        ars_runtime_h runtime{};
        if(ars_runtime_create(&desc,&runtime)!=ARS_OK) return 1;
        ars_status wrong=ARS_OK;
        std::thread wrong_thread([&]{wrong=ars_runtime_begin_close(runtime);}); wrong_thread.join();
        if(wrong!=ARS_E_WRONG_THREAD || ars_runtime_begin_close(runtime)!=ARS_OK) return 2;
        std::barrier start(3);
        std::array<ars_status,2> result{};
        std::thread a([&]{start.arrive_and_wait(); result[0]=ars_runtime_finish_close(runtime);});
        std::thread b([&]{start.arrive_and_wait(); result[1]=ars_runtime_finish_close(runtime);});
        start.arrive_and_wait(); a.join(); b.join();
        if(!((result[0]==ARS_OK && result[1]==ARS_E_STALE_HANDLE)||
             (result[1]==ARS_OK && result[0]==ARS_E_STALE_HANDLE))) return 3;
    }
    std::puts("PASS B1 public C++ ABI: 1000 controlled two-finalizer races, one winner, no wrong-thread close");
    return 0;
}
