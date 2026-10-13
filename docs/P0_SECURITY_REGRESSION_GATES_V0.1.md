# P0 Security Regression Gates v0.1 — Handle Lifetime and Source Authority

**Status:** PROPOSED / NO EXECUTABLE ENGINE YET  
**Reference:** docs/P0_HANDLE_AND_SOURCE_AUTHORITY_CONTRACT_V0.1.md  
**Design-only C interface:** contracts/ars_p0_contract_draft.h  
**Does not amend G00 PASS automatically.** This is a regression requirement list; tests are pending actual implementation.

## 1. Evidence and verdict protocol

A case is PASS only with source at exact candidate SHA, reproducible command, deterministic fixture/scheduler trace, status and invariants actually asserted, sanitizer/runner/OS evidence and test result. A proposed algorithm, compiled header or reviewer confidence is NOT test evidence. A case is HOLD until executable implementation and logs exist. Use FAIL for any crash, UAF, incorrect pose publication, authorization widening or unbounded queue.

Each report includes test_id, PR, HEAD, build/compiler flags, OS, CPU/GPU where applicable, seed, command, raw stdout/stderr, exit code, event trace and reviewer. Linux ASan/UBSan + TSan (separate builds) for CPU races; Windows x64 MSVC + ASan where supported; real D3D11 GPU retirement only on compatible Windows GPU runners. Sanitizers are supplementary to deterministic race scheduling and cannot prove absence of races alone.

## 2. Handle registry negative tests

| Test ID | Setup / injected interleaving | Required result |
| --- | --- | --- |
| HDL-001 | use character handle after logical destroy | STALE_HANDLE without payload dereference/UAF |
| HDL-002 | destroy character A, reuse freed slot for B, call A handle | A stale, B unaffected; generation differs |
| HDL-003 | pass model handle ID as character handle | STALE_HANDLE or INVALID_ARGUMENT; type checked |
| HDL-004 | use handle from destroyed runtime against new runtime | stale runtime_uid; no alias even if same slot/gen |
| HDL-005 | force slot generation to UINT32_MAX then invalidate | permanent slot retirement; no generation wrap |
| HDL-006 | model release while 3 characters still refer to asset | user handle stale; 3 instances render and exit safely |
| HDL-007 | 8 ingress threads pin char, H destroy while frames are being copied | no UAF; admissions after close denied; queued stale frames never publish |
| HDL-008 | H attempts destroy at S update structural conflict | BUSY/wrong-thread contract; no racing mutation |
| HDL-009 | R has pinned snapshot, H destroys character | GPU retire deferred until R releases plus backend-safe completion |
| HDL-010 | call begin_close twice and finish_close before GPU drain | deterministic CLOSED/BUSY; no double-free |
| HDL-011 | begin_close races with fresh handle lookup | lookup either pinned before close or rejected after; never raw-pointer race |
| HDL-012 | user passes all-zeros, malformed runtime_uid, huge slot | INVALID_ARGUMENT/STALE_HANDLE; no out-of-bounds |
| HDL-013 | host clock callback intentionally reenters API | callback contract violation returns BUSY or controlled failure, never lock deadlock |
| HDL-014 | randomized create/destroy 100,000 times | no live-slot/reference leaks; bounded tombstones, no ABA |
| HDL-015 | H stops GPU pump before finish_close | BUSY with outstanding_gpu_retires >0, never cross-thread GPU free |
| HDL-016 | simulated OOM during registry pin/tombstone creation | typed failure; no corrupted slot/free-list |

## 3. Source authority, capability and replay tests

| Test ID | Setup / attack | Required result |
| --- | --- | --- |
| SRC-001 | submit arbitrary frame with no registered capability | SCOPE_DENIED, pose unchanged |
| SRC-002 | forge source_id and current-looking source_generation in JSON adapter payload | ignored as authority, still denied |
| SRC-003 | guess 128-bit cap; flip one bit of valid cap | SCOPE_DENIED; no existence disclosure in remote response |
| SRC-004 | RNG unavailable at register/rotate | typed failure, no weak fallback, no source activated |
| SRC-005 | source registered only for mouth.open submits head.yaw_rad | SCOPE_DENIED, other channels unaffected |
| SRC-006 | registered cap for Character A submitted with payload claiming B | B cannot be changed; character chosen by binding, not payload |
| SRC-007 | source sends priority=INT_MAX, layer override/mode switch fields | packet fields never grant authority; unrecognized command denied |
| SRC-008 | duplicate or decreasing sequence per active cap | invalid frame, pose generation does not rewind |
| SRC-009 | source sequence reaches UINT64_MAX and attempts wrap | reject/requires rotation; old cap stays bounded |
| SRC-010 | rotating source at unchanged diagnostic label | old cap rejected, new cap accepted; generation strictly advances |
| SRC-011 | revoke old cap, reconnect, replay old frame at plausible new timestamp | rejected at submission and/or pre-commit gate |
| SRC-012 | source TTL expires with queue backlog | no post-expiry new pose ownership; fallback on next update |
| SRC-013 | untrusted frame NaN, INF, invalid quaternion, invalid UTF-8 ID | rejected before queue; source cannot crash |
| SRC-014 | flood 100x intended rate with 16 sources | bounded queues and per-source quotas; drops visible in counters |
| SRC-015 | malformed frame array pointer, count overflow, struct_size too short | INVALID_ARGUMENT/INVALID_FRAME before invalid read |
| SRC-016 | same label with different cap ID and different Character generation | no cross-generation authority inheritance |
| SRC-017 | source descriptor allowed_channel_count=0 | default DENY, not wildcard/all channels |
| SRC-018 | source after 2D -> 3D mode switch | same semantic scope; cannot control unspecified native bone/morph |
| SRC-019 | source-admin handle passed as ingress cap/type confusion | rejected; no implicit role conversion |
| SRC-020 | trusted clock callback moves backward | CLOCK_REGRESSION, rejects frame/lease anomalies |
| SRC-021 | source registers mixed-case / Unicode alias channel IDs | reject noncanonical semantic IDs; no grant through aliasing |
| SRC-022 | force cap mint collision with a retired token, then exhaustion of issuance budget | retry fresh token or fail closed; retired token never valid again |

## 4. Required controlled concurrency traces

### RACE-A: ingress accepted before revoke but enqueue happens after it

~~~text
I: acquire runtime and source pins; validate old cap
I: PAUSE before queue append
H: source_revoke enters publish_gate, increments epoch, returns OK
I: resume; deep copy and/or append old frame
S: consume old stamped generation, DROP
S: publish baseline/other active sources only
ASSERT: no post-revoke pose_generation depends on old cap
~~~

### RACE-B: mixer computed candidate before revoke

~~~text
S: dequeue old cap frame and mix candidate; PAUSE before publish_gate
H: source_revoke acquires gate and invalidates old generation; returns OK
S: resume; recheck source generations at publish_gate
S: DISCARD candidate; next update mixes from active sources only
ASSERT: zero commits depending on revoked generation after revoke returns
~~~

### RACE-C: update commits before revoke

~~~text
S: verify source and publish pose P under gate
H: revoke waits gate then invalidates generation, returns OK
R: may still render P if already pinned
S: next update cannot publish a NEW pose containing old cap
ASSERT: historic P is labeled pre-revoke; no false promise of instant display scrub
~~~

### RACE-D: runtime close during submission and GPU snapshot

~~~text
I: pin runtime+character; PAUSE
R: pin committed snapshot; submit render; PAUSE
H: begin_close -> QUIESCING, no new work pins, revoke bound sources
I: finishes safe copy; release pins; no publish
H: finish_close => BUSY
R: release snapshot, drain GPU completion on R
H: finish_close => OK, unregister runtime
I: invoke old handles => STALE_HANDLE
ASSERT: no UAF, no cross-thread GPU free
~~~

### RACE-E: rotation at sequence rollover

~~~text
I: old cap sequence=UINT64_MAX accepted at most once
H: rotate, advance source generation and issue fresh cap
I: old cap sequence=0 or UINT64_MAX again => denied
I: new cap sequence=1 => valid
ASSERT: no ABA of cap, no replay across generations
~~~

Use deterministic scheduler barriers/latches, not sleeps, and retain exact interleaving event logs. Run each at least 1,000 randomized iterations in a stress suite in addition to deterministic controlled cases.

## 5. Behavioral state tables

| Object | Current | Event | Next | Visible result |
| --- | --- | --- | --- | --- |
| runtime | RUNNING | begin_close | QUIESCING | no new work pins |
| runtime | QUIESCING | GPU/pins remain | DRAINING | finish_close = BUSY |
| runtime | DRAINING | R retirement finished | DEAD | finish_close = OK |
| character | ACTIVE | destroy | CLOSING | old user handle stale |
| source | ACTIVE gen=N | revoke | REVOKED gen>N | old cap denied; new pose commit barred |
| source | ACTIVE gen=N | rotate | ACTIVE gen=N+1 | old cap denied; new random cap issued |
| source | REVOKED | admin_release | CLOSED | admin handle stale |
| motion frame | QUEUED old gen | S consume | DROPPED | no pose update |

## 6. G00 acceptance slice

**G00-P0-A:** reviewed handle and registry design, C ABI type sizes, threading rules, close management exceptions, source capability and publish-gate linearization are unambiguous.  
**G00-P0-B:** compile-only C11 and C++20 smoke tests on the header (do not mistake compile for security PASS).  
**G00-P0-C:** before production M1, minimal registry/mixer implementation passes HDL-001..016, SRC-001..022 and RACE-A..E on a declared CI matrix or explicit phased evidence approved by independent reviewer.  
**G00-P0-D:** architecture ADR-0001 device owner and ADR-0004 threading must lock GPU retirement and thread-safe revoke behavior; demonstrate exact source scopes/owners.  
**G00-P0-E:** no source/ABI/schema changes are merged until new exact HEAD independent re-audit, with all P0 addressed and tracked P1 still explicitly gated.

**Audit status at creation:** all runtime tests HOLD (there is no implementation); C header declaration-only GCC/G++ syntax smoke may have local evidence but is not a full ABI consumer implementation.
