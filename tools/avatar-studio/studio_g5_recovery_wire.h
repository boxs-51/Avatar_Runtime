#pragma once
#include "studio_g5_bus.h"
#include <stddef.h>
/* Fixed strict, read-only recovery protocol.
   Requires a separately authenticated trusted ingress/session from the host.
   Wire is only "STATUS <decimal request ID>"; never accepts source or paths. */
G5Code g5_recovery_handle(const G5Bus *bus,G5Ingress ingress,
                          uint64_t trusted_session,
                          const unsigned char *request,size_t length,
                          char *response,size_t response_capacity);
