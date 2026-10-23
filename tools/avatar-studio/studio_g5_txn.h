#pragma once
/* Local-only G5 Native owner-thread transaction core (no IPC or V2 enablement). */
#include <stdint.h>
#include "studio_rig.h"
#define G5_HISTORY_CAP 16
#define G5_RECEIPT_CAP 32
typedef enum {
 G5_OK=0,G5_BUSY,G5_REVISION_CONFLICT,G5_STALE_SESSION,
 G5_STALE_MODEL,G5_INVALID_DRAFT,G5_OUT_OF_BUDGET,
 G5_ID_CONFLICT,G5_UNKNOWN
} G5Code;
typedef enum {G5_COMMITTED=1,G5_REJECTED=2,G5_ABORTED=3} G5Outcome;
typedef struct {
 uint64_t owner,request_id,revision;
 G5Outcome outcome;
} G5Receipt;
typedef struct {
 uint64_t session_epoch,model_generation,revision,applied_revision;
 uint64_t accepted_frame,solver_revision,draft_serial;
 uint64_t lease_owner,draft_id,base_revision;
 unsigned int has_draft,validated,undo_count,redo_count,receipt_count;
 StudioRig live,draft;
 StudioRig undo[G5_HISTORY_CAP],redo[G5_HISTORY_CAP];
 G5Receipt receipts[G5_RECEIPT_CAP];
} G5Txn;
G5Code g5_txn_init(G5Txn*,const StudioRig*,uint64_t session,
                   uint64_t model_generation,uint64_t revision);
G5Code g5_txn_begin(G5Txn*,uint64_t session,uint64_t model_generation,
                    uint64_t expected,uint64_t owner,uint64_t *draft_id);
G5Code g5_txn_anchor(G5Txn*,uint64_t session,uint64_t generation,uint64_t owner,uint64_t draft,
                     int vertex,int enabled);
/* Trusted ingress supplies owner; never accept raw user-authored owner IDs. */
G5Code g5_txn_beam(G5Txn*,uint64_t session,uint64_t generation,
                   uint64_t owner,uint64_t draft,int a,int b,int enabled);
G5Code g5_txn_parent(G5Txn*,uint64_t session,uint64_t generation,
                     uint64_t owner,uint64_t draft,int parent,int child,
                     float stiffness,int enabled);
G5Code g5_txn_weight(G5Txn*,uint64_t session,uint64_t generation,
                     uint64_t owner,uint64_t draft,int vertex,float value);
G5Code g5_txn_cross(G5Txn*,uint64_t session,uint64_t generation,
                    uint64_t owner,uint64_t draft,int parent,int target,
                    int child,int target_vertices,float stiffness,int enabled);
G5Code g5_txn_validate(G5Txn*,uint64_t session,uint64_t generation,uint64_t owner,uint64_t draft);
G5Code g5_txn_commit(G5Txn*,uint64_t session,uint64_t generation,uint64_t owner,uint64_t draft,
                     uint64_t expected,uint64_t request_id,uint64_t *revision);
G5Code g5_txn_abort(G5Txn*,uint64_t session,uint64_t generation,uint64_t owner,uint64_t draft);
G5Code g5_txn_undo(G5Txn*,uint64_t session,uint64_t generation,
                   uint64_t expected,uint64_t owner,uint64_t request_id,
                   uint64_t *revision);
G5Code g5_txn_redo(G5Txn*,uint64_t session,uint64_t generation,
                   uint64_t expected,uint64_t owner,uint64_t request_id,
                   uint64_t *revision);
G5Code g5_txn_receipt(const G5Txn*,uint64_t session,uint64_t owner,
                      uint64_t request_id,G5Receipt *receipt);
G5Code g5_txn_apply(G5Txn*,uint64_t document_revision,
                     uint64_t solver_revision,uint64_t frame_generation,
                     int accepted);
