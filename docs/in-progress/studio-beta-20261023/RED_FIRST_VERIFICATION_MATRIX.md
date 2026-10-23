# VER-001 — RED-first test and experiment matrix

- **Version/date:** 0.1 / 2026-10-23
- **Status:** PLAN plus auditor findings; **no new test execution**
- **Source:** SYS-001, ADR-001, Beta QSA audit
- **Release:** HOLD while F01 is open

## Parser, IPC and storage

| ID | Synthetic/physical scenario | PASS oracle | Evidence |
|---|---|---|---|
| IPC-P01 | Valid command with alphabetic suffix | ERR, no state mutation | **RED** — auditor Windows physical |
| IPC-P02 | Extra numeric token, trailing whitespace, truncated/oversized command | Exact grammar, bounded EOF | PLANNED full opcode matrix |
| IPC-P03 | NaN/Inf, negative/out-of-range, overflow | ERR, zero mutation | PARTIAL historical tests |
| IPC-Q01 | Two writers race for same request | Exactly one admitted owner | **RED** — auditor synthetic two-caller |
| IPC-Q02 | Crash after claim, stale reply, restart | No loss/false ACK; bounded cleanup | PLANNED |
| IPC-Q03 | Windows sharing and concurrent cleanup | Ownership-safe recovery | PLANNED |
| PRIV-01 | Public-safe reporter fed model path/receipt/image-derived values | Reject/scrub to case ID + PASS/FAIL + class | PLANNED |
| PRIV-02 | Offline Git-object provenance/distribution check | Preserve custody; no public-exposure guess | OPEN; no prune |
| RIG-IO-01 | Short write, flush failure, failed atomic rename | Previous-good rig intact | PLANNED |
| RIG-IO-02 | Crash before/after replace; legacy V1–V4 | Complete load or previous-good; typed invalid rejection | PLANNED |

## Proposed bone and skinning gates

| ID | RED-first case | Expected invariant |
|---|---|---|
| BONE-01 | Known parent rotation and bind pivot | Deterministic world/local matrix composition |
| BONE-02 | Cycle, self-parent, missing parent, duplicate ID | No graph mutation |
| SKIN-01 | One-bone identity and known two-bone blend | Closed-form LBS numerical parity |
| SKIN-02 | Nonfinite/negative/duplicate weights or over 4 influences | Strict reject or documented identity policy |
| FRAME-01 | Replace model during frame solve | Generation fence; no stale publish |
| FRAME-02 | Animation/skin/XPBD with rejected candidate | One authoritative committed frame |
| UI-01 | Brush, pose, undo/redo, save/reload | State readback; original geometry immutable |
| PERF-01 | Controlled synthetic workload, same build | Frame pacing/allocations/CPU evidence before optimization claim |

## Evidence and reporting rules

Distinguish **AUDIT-OBSERVED**, **SOURCE-CONFIRMED**, **EXECUTED** and **PROPOSED**. Historical green API/XPBD tests are limited regressions, not IPC/parser clearance. Every future run needs: case ID, git baseline, OS/compiler/config, fixture classification, expected/actual, mutation oracle, generic error class, private evidence location and rollback outcome. Do not print raw receipts or model-derived metrics in versioned Markdown. Put detailed logs under ignored `build/studio-user/`; no CI/issue upload without privacy authorization.

**STOP:** invalid native command is accepted, request ownership ambiguous, private material in public artifact, or unapproved source/release action.
## 2026-10-23 implementation verification checkpoint

- `IPC-P01`: initial eight Windows Native grammar RED cases, now 48/48 scoped malformed grammar cases GREEN after native EOF and bounded request changes.
- `IPC-Q01`: synthetic multi-process exclusive writer admission PASS with OS lock.
- `IPC-Q02`: killed Python writer frees OS ownership but leaves any unfinished request **quarantined**; complete Native restart and idempotent replay remain NOT_RUN.
- `PRIV-01`: categorical reporter input validation synthetic PASS; artifact promotion enforcement NOT_RUN.
- `RIG-IO-01`: injected after-flush save failure preserves previous-good rig, subsequent save/load PASS.
- `RIG-IO-02`: real crash/power-loss and forced Windows replacement fault NOT_RUN.
- `BONE/SKIN/FRAME/PERF gates`: proposed only; development of new skeletal skinning remains on HOLD pending security gates.

Source-gated local regression ran successfully, but does not replace independent reviewer re-execution. See `FINDINGS_REMEDIATION_20261023.md` and ignored Host evidence.

## ADR-002 V2 discovery contract — 2026-10-23

V2-RED-01: import/probe of absent module: RED before implementation.
V2-SCHEMA-01: only capabilities/schema advertised; editing and real model
access denied: GREEN (synthetic).
V2-JSON-01: exact field set, version, UTF-8, duplicate key, finite
numbers, 64KiB input budget and request identifiers: GREEN (synthetic).
V2-FUZZ-01: 256 deterministic mutated JSON payloads: GREEN as a bounded
test, not a complete fuzzer certification.
V2-CLI-01: capabilities/schema and invalid/mutating request rejection:
GREEN (Windows local CLI).
IPC-Q04: own truncated ACK falsely successful: RED, then GREEN with
complete newline/type/receipt validation. Foreign ACK quarantine GREEN.

Not run: V2 Native named-pipe integration, journal, session generation,
real read snapshots, CAS transactions, GUI, cross-OS, production perf.
Privacy and release gates remain HOLD.

## G4 Local-only v1 adapter verification, 2026-10-23

G4-LOCAL-01: absent local bridge or private opt-in -> REJECTED (GREEN).
G4-LOCAL-02: strict typed status and metadata -> GREEN synthetic/live.
G4-LOCAL-03: max-four parameter and part page -> GREEN synthetic/live.
G4-LOCAL-04: invalid page, malformed native receipt, timeout -> fail
closed / TIMEOUT_UNKNOWN (GREEN synthetic).
G4-LOCAL-05: aggregate counts change while reading a page -> STALE_MODEL
and no partial result (RED before change, GREEN after).
G4-LOCAL-06: capabilities/schema advertise added reads only when local
private adapter is explicitly bound (RED before change, GREEN after).

All results above are development scoped. Same-count model replacement,
native generation/revision fences, ID lookup, geometry coverage,
UI/Agent accepted-frame parity, and V5 transaction tests remain NOT RUN.

## G4-EPOCH matrix results, 2026-10-23

- G4-EPOCH-01: missing epoch and unguarded V1 reads: synthetic
  RED, then native-generation guarded GREEN.
- G4-EPOCH-02: same-count model replacement during paging:
  synthetic RED, then STALE_MODEL GREEN.
- G4-EPOCH-03: absent epoch and stale generation: reject, no fallback.
- G4-EPOCH-04: caller expected_model_generation compare: RED then GREEN.
- G4-EPOCH-05: Windows physical Native stale generation: reject;
  matching generation: read-only receipt PASS.
- G4-EPOCH-06: rebuilt Studio Release + private local model E2E PASS.
- G4-EPOCH-07: existing Native grammar/API, V2 contracts, IPC race,
  rig durability and XPBD regression PASS.

NOT RUN: edit-revision fencing, snapshot across animation frame,
UI-vs-Agent CAS, interrupted model load, long-run generation churn,
independent audit, cross-OS port. G4 and RELEASE HOLD.

G4-EPOCH-08: sequential Native Studio restart with the same model,
old generation rejected by the replacement process and current
generation accepted. Windows physical PASS. Does not certify lost-ACK
replay, exactly-once mutation, or an independent audit.
