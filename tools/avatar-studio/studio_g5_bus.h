#pragma once
/* Experimental single-owner command bus. NOT connected to Studio/IPC/V2.
   Ingress is supplied by trusted Native routing, never decoded from user JSON. */
#include "studio_g5_txn.h"
typedef enum {G5_UI=1,G5_LEGACY=2,G5_AGENT=3} G5Ingress;
typedef enum {
 G5_BEGIN=1,G5_ANCHOR,G5_VALIDATE,G5_COMMIT,G5_ABORT,
 G5_UNDO,G5_REDO,G5_LEGACY_ANCHOR
} G5BusOp;
typedef struct {
 G5Ingress ingress;G5BusOp operation;
 uint64_t session,generation,expected_revision;
 uint64_t draft_id,request_id;
 int vertex,enabled;
} G5BusCommand;
typedef struct {
 G5Code code;uint64_t document_revision,draft_id;
 unsigned committed;
} G5BusResult;
typedef struct {
 G5Txn txn;
 uint64_t quarantined_request[4]; /* per ingress, not process-global */
} G5Bus;
G5Code g5_bus_init(G5Bus*,const StudioRig*,uint64_t session,
                   uint64_t generation,uint64_t revision);
G5Code g5_bus_submit(G5Bus*,const G5BusCommand*,G5BusResult*);
/* Transport injects uncertainty; absence of a receipt can never clear this. */
G5Code g5_bus_mark_unknown(G5Bus*,G5Ingress,uint64_t original_request);
/* Read-only regardless of mutation queue quarantine. */
G5Code g5_bus_recover(const G5Bus*,G5Ingress,uint64_t session,
                      uint64_t original_request,G5Receipt*);
/* Explicit reconciliation requires immutable owner receipt; not on timeout. */
G5Code g5_bus_reconcile(G5Bus*,G5Ingress,uint64_t session,
                        uint64_t original_request);
