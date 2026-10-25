#pragma once
/* Reusable C11 bounded LOCAL-ONLY status for a shipping Studio whose G5
   writer is intentionally disabled. Neither private model data nor G5
   transaction claims are exported. No public runtime ABI20 symbols. */
#include <stddef.h>
#include <stdint.h>
int studio_g5_live_status_format(char *buffer,size_t capacity,
                                  uint64_t model_generation,
                                  uint64_t legacy_rig_revision);
