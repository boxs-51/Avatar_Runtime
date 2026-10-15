#include "ars/ars.h"

extern "C" ARS_API uint32_t ARS_CALL ars_get_abi_version(void) {
    return ARS_ABI_VERSION;
}
