#include <ars/ars.h>
#include <cstddef>
#include <cstdio>
#include <thread>
#include <type_traits>
static_assert(std::is_standard_layout_v<ars_render_target> && sizeof(ars_runtime_h)==16);
int main() {
    ars_runtime_desc desc{};desc.struct_size=sizeof(desc);desc.abi_version=ARS_ABI_VERSION;
    ars_runtime_h runtime{};
    if(ars_runtime_create(&desc,&runtime)!=ARS_OK)return 1;
    ars_status status=ARS_OK;
    std::thread other([&]{status=ars_runtime_begin_close(runtime);});other.join();
    if(status!=ARS_E_WRONG_THREAD || ars_runtime_begin_close(runtime)!=ARS_OK)return 2;
    std::thread finalizer([&]{status=ars_runtime_finish_close(runtime);});finalizer.join();
    if(status!=ARS_OK || ars_runtime_finish_close(runtime)!=ARS_E_STALE_HANDLE)return 3;
    std::puts("INSTALLED_ABI20_CPP20_THREAD_LIFETIME_PASS");return 0;
}
