/* Independent C11 build-tree consumer. Does not link the M0 design stub. */
#include "ars/ars.h"
#include <stdio.h>

int main(void) {
    ars_runtime_desc desc = {0};
    ars_runtime_h runtime = {{1, 2, 3}};
    ars_capabilities caps = {0};
    desc.struct_size = (uint32_t)sizeof(desc); desc.abi_version = ARS_ABI_VERSION;
    caps.struct_size = (uint32_t)sizeof(caps); caps.abi_version = ARS_ABI_VERSION;
    if (ars_get_abi_version() != ARS_ABI_VERSION) return 1;
#ifdef ARS_CONSUMER_RUNTIME_CORE
    if (ars_runtime_create(&desc, &runtime) != ARS_OK || !runtime.id.runtime_uid) return 2;
    if (ars_runtime_get_capabilities(runtime, &caps) != ARS_OK || caps.feature_bits) return 3;
    if (ars_runtime_begin_close(runtime) != ARS_OK || ars_runtime_finish_close(runtime) != ARS_OK) return 4;
    if (ars_runtime_finish_close(runtime) != ARS_E_STALE_HANDLE) return 5;
    puts("PASS standalone C11 core consumer: real CPU create/close, stale ID denied; no source/GPU features");
#else
    if (ars_runtime_create(&desc, &runtime) != ARS_E_UNSUPPORTED) return 2;
    if (runtime.id.runtime_uid || runtime.id.slot || runtime.id.generation) return 3;
    if (ars_runtime_get_capabilities(runtime, &caps) != ARS_E_UNSUPPORTED || caps.feature_bits) return 4;
    puts("PASS BUILD-02 standalone C11 consumer: foundation ABI link; runtime remains UNSUPPORTED");
#endif
    return 0;
}
