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