#pragma once
/* Experimental single-owner command bus. NOT connected to Studio/IPC/V2.
   Ingress is supplied by trusted Native routing, never decoded from user JSON. */
#include "studio_g5_txn.h"
typedef enum {G5_UI=1,G5_LEGACY=2,G5_AGENT=3} G5Ingress;
/* The authenticated Host/session adapter creates this; never deserialize from JSON.
   Actor IDs are stable within the Native session and scoped to ingress. */
typedef struct {G5Ingress ingress;uint64_t owner;} G5TrustedActor;
#define G5_ACTOR_QUARANTINE_CAP 16
typedef struct {G5Ingress ingress;uint64_t owner,request_id;} G5ActorQuarantine;
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
 uint64_t quarantined_request[4]; /* old standalone tests only; NOT owner-secure */
 G5ActorQuarantine actor_quarantine[G5_ACTOR_QUARANTINE_CAP];
} G5Bus;
G5Code g5_bus_init(G5Bus*,const StudioRig*,uint64_t session,
                   uint64_t generation,uint64_t revision);
#ifdef G5_TEST_ONLY_UNBOUND_BUS
/* Existing standalone v0 tests only: never export in trusted builds. */
G5Code g5_bus_submit(G5Bus*,const G5BusCommand*,G5BusResult*);
/* Transport injects uncertainty; absence of a receipt can never clear this. */
G5Code g5_bus_mark_unknown(G5Bus*,G5Ingress,uint64_t original_request);
/* Read-only regardless of mutation queue quarantine. */
G5Code g5_bus_recover(const G5Bus*,G5Ingress,uint64_t session,
                      uint64_t original_request,G5Receipt*);
/* Explicit reconciliation requires immutable owner receipt; not on timeout. */
G5Code g5_bus_reconcile(G5Bus*,G5Ingress,uint64_t session,
                        uint64_t original_request);
#endif /* G5_TEST_ONLY_UNBOUND_BUS */
/* G5 T2 trusted-owner API: new integrations MUST use these calls.
   The legacy unbound methods above exist solely for existing standalone tests
   and MUST NOT be connected to Studio IPC, UI or V2 writer. */
G5Code g5_bus_submit_trusted(G5Bus*,const G5TrustedActor*,
                              const G5BusCommand*,G5BusResult*);
G5Code g5_bus_mark_unknown_trusted(G5Bus*,const G5TrustedActor*,
                                    uint64_t original_request);
G5Code g5_bus_recover_trusted(const G5Bus*,const G5TrustedActor*,
                              uint64_t session,uint64_t original_request,
                              G5Receipt*);
G5Code g5_bus_reconcile_trusted(G5Bus*,const G5TrustedActor*,
                                uint64_t session,uint64_t original_request);
