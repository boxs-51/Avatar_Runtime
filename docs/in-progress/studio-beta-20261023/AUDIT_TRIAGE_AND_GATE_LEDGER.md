# AUD-001 — Beta audit triage and release gate ledger

- **Version:** 0.1, 2026-10-23
- **Status:** TRIAGED / OPEN; reviewed subject BLOCKED / REVISE
- **Primary source:** `docs/BETA_EXPANDED_QUALITY_SECURITY_ARCH_AUDIT_20261023.md` at inspected baseline `e38eba5f4b1f21e8590520ef1aefb3a7cfa8407e`
- **Authority:** Auditor artifact = review-only, SELF_REVIEW_ONLY; this document is a development handoff, **not** auditor approval or issue closure
- **Freshness check:** Local current HEAD matched the auditor pin when read. Source check independently saw vulnerable numeric-tail parsing, shared IPC check-and-replace and direct rig overwrite.

| Finding | Severity | Observed audit outcome | Next required gate | Status |
|---|---|---|---|---|
| F01 — Native parser suffix acceptance | **P1** | Physical Native RED: extra nonnumeric suffix accepted by some state-changing opcodes | Strict per-opcode exact EOF before mutation; adversarial physical RED -> GREEN with state readback | **OPEN / BLOCKER** |
| F02 — Non-exclusive single-file IPC | P2 | Two-caller synthetic admission race; no real consumer exactly-once proof | Atomic exclusive owner claim, per-client ACK, cleanup/restart matrix | OPEN |
| F03 — Private unreachable Git blobs | P2 | Local unreachable binary signatures; **no public/remote exposure proved** | Authorized offline origin/distribution investigation; preserve objects, no gc/prune/history rewrite | OPEN / PRIVATE |
| F04 — Report/log privacy leakage risk | P2 | Model-derived data can be written locally; no automatic public upload shown | Public-safe allowlist, artifact redaction tests, private/public output separation | OPEN |
| F05 — Direct rig overwrite | P2 | Target file opened in truncate/write mode; crash safety unproved | Temp sibling + flush + atomic Windows replace + rollback/fault injection | OPEN |
| F06 — Coupled controller/warnings | P3 | Controller coupling and compiler warning debt | Parser extraction and warning baseline after P1 is fixed | OPEN |
| F07 — E2E coverage gaps | P3 | Current command receipts do not certify full correctness, crash behavior or images | Native malformed grammar matrix, effect/readback, crash, fuzz, GUI and cross-platform tests | OPEN |

## Gate order

1. **G-P1:** Exact native grammar for every opcode; invalid input must return ERR and leave observable state unchanged. Owner authorization and independent retest required.
2. **G-IPC:** Multi-agent exclusive admission, timeout/crash recovery, stale ACK isolation and compatibility policy.
3. **G-PRIV:** No distribution of model-derived reports, captures, paths, counts, object IDs or hashes; public output restricted to case IDs, PASS/FAIL and generic error classes. Private orphan objects remain preserved.
4. **G-IO:** Atomic durable rig save/load/recovery with Windows physical fault injection.
5. **G-QA:** Fresh rebuilt binaries, parser fuzz, effect-based API E2E, private local visual checks and measured performance.
6. **G-DESIGN:** Review ADR-001 only after security gates; feature remains disabled by default.

## Controlled remediation protocol

For each bug: record reproduction in isolated synthetic/local evidence, matrix of normal/adversarial cases, unmodified oracle, bounded implementation authorization, code changes on a separate scoped commit, physical GREEN, rollback demonstration and reviewer sign-off. Never turn an advisory idea into a P1 without evidence. Never delete or rewrite Git object history while provenance is unresolved.

**Decisions not granted by this report:** source security patch, publishing model evidence, GitHub write, merge, release, driver/system intervention or history cleanup. **Current release gate:** HOLD.
## Development remediation delta — 2026-10-23

Scoped code and test commits now present in local branch:
- F01 parser: `eb2b292` — 48 native malformed command cases GREEN, original eight RED reproduced before patch.
- F02 IPC writer: `ccfa9b9` — OS-backed cooperative writer lock, process race GREEN, killed-owner OS lock release GREEN; stale request still requires safe recovery.
- F05 rig durability: `80251d7` — sibling temporary rig file + flush/commit/replace; after-flush fault injection preserves prior rig.
- F04 reporter guard: `6f4076c` — categorical allowlist, private/path/model-measurement inputs rejected.
- Documentation evidence: `e1399c1` — privacy-safe remediation report.

**Current decisions:** F01 PROVISIONAL GREEN pending independent native grammar audit; F02 PARTIAL (direct native IPC writing, replay/ACK lifecycle and restart ownership); F03 HOLD (private local Git object custody, no prune); F04 PARTIAL (new safe reporter not yet mandatory for all uploads); F05 PROVISIONAL GREEN (power-loss and filesystem fault matrix unrun); F06 OPEN (native-controller coupling and warning baselines); F07 PARTIAL (expanded local adversarial tests; fuzz/visual/cross-OS outstanding).

The prior auditor findings remain historically valid for the pinned pre-remediation source. This development update does not alter auditor files or represent independent approval. The **release gate remains HOLD**.

## ADR-002 Toolkit V2 follow-up — 2026-10-23

At the current implementation checkpoint, only offline schema discovery
and capability negotiation are callable. Native Studio V2 and H5 Avatar
tool registration are explicitly disabled. RED-first strict JSON grammar,
duplicate fields, nonfinite values, size caps and 256 synthetic mutated
payload checks are GREEN. This does not complete ADR-002 G4 live snapshots.

F02 received a new negative test for an incomplete native response. A
truncated token-plus-OK frame was previously accepted as success. The
V1 Python adapter now requires a complete bounded newline-terminated
response with a recognized status and nonempty receipt. Foreign-token ACK
records still remain isolated. The native direct-writer and restart
journal cases remain open; F02 stays PARTIAL.

F01 PROVISIONAL GREEN; F02 PARTIAL; F03 HOLD; F04 PARTIAL; F05
PROVISIONAL GREEN; F06 OPEN; F07 PARTIAL. Product RELEASE HOLD.
See ADR002_G4_DISCOVERY_IMPLEMENTATION_20261023.md.

## 2026-10-23 Local-only G4 scope correction

The updated 11:08 ADR-002/ADR-003 Local-only appendices supersede the
earlier suggestion that named pipes, an external broker, a distributed
journal or mandatory Computer Bridge/H5 registration are required.
F02 still requires local input ownership, no false ACK, crash/timeout
recovery and frame/revision distinction; it does NOT require a
distributed journal. F04 permits authorized model/geometry inspection
on Host but still forbids exporting private data to Git, CI or reports.
F01 provisional GREEN; F02 PARTIAL; F03 HOLD; F04 PARTIAL; F05 provisional
GREEN; F06 OPEN; F07 PARTIAL. Product RELEASE HOLD.

G4 implemented only a bounded LOCAL_PRIVATE legacy V1 read adapter:
studio.local.status, catalog.parameters.page, catalog.parts.page with
up to four values per page and count-drift rejection. It cannot detect
same-count model reload and does not replace the Native generation
fence or complete feature parity. See ADR002_LOCAL_ONLY_G4_READBACK_20261023.md.
