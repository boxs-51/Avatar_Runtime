# LOG-001 — Evidence, reporting and log custody policy

- **Version/date:** 0.1 / 2026-10-23
- **Status:** PROCEDURE PROPOSED / active privacy constraints inherited
- **Owner:** Repository owner; audit author remains review-only
- **Sources:** Beta QSA F03/F04, SYS-001, VER-001, Studio AGENT_API.md

## Classify every output before creation

| Tier | Allowed content | Destination |
|---|---|---|
| PUBLIC-SAFE | Case ID, source revision, generic pass/fail, broad error class, test name, gate status | `docs/in-progress/studio-beta-20261023/` |
| LOCAL-SENSITIVE | Raw native stdout/stderr, frame timing, vertex/model-derived values, full JSON receipts, detailed call arguments | Git-ignored `build/studio-user/` |
| PRIVATE-ASSET | Original MOC3/texture/model manifest, rendered PNG, screenshots, derived hashes, recovered Git objects | Existing owner-approved offline local custody only; never stage |
| AUDITOR-OWNED | Auditor marker, isolated evidence and IPC | Preserve unchanged; not a development team's log folder |

**Hard exclusions from distributable Markdown and Git:** private model/texture bytes, model names, local private paths, full logs, screenshots, frame/geometry counts, hash/signatures of model material and complete raw API receipts.

## Private execution log structure

Create a directory under `build/studio-user/studio-beta-evidence/` per run. Keep raw outputs private. A run manifest should include: UTC/local timestamp, exact git HEAD, branch, owner role, command category, executable/config, run status, synthetic vs private classification, path to raw files and whether cleanup was performed. If a field might reveal the model, omit it from the public report.

Suggested safe file names: `run-private.json`, `raw-native-private.log`, `test-private.log`, `summary-public-safe.md`. Do not stage or upload this directory.

## Reporting workflow

1. Capture the **sanitized verdict** in VER-001 or AUD-001; raw traces remain in ignored local storage.
2. Include negative controls and observed mutation invariants, not only success receipts.
3. Link a private log only in a **local-only** register. Public-safe reports use case IDs and status without path to sensitive files.
4. Before commit, run exact staged-index allowlist, binary/magic rejection, private identifier/path regex scan and `git diff --cached --check`.
5. Before copying output to issue/PR/CI, perform a separate privacy review; default DENY.
6. Preserve unreachable local Git objects pending authorized investigation; never run `git gc` / `git prune` for cleanup.
7. Crash/timeout reports must record whether the previous good rig and IPC owner state survived.

## Verification status

The repository's `build/studio-user/` ignore rule was checked on this Host. This document is a custody procedure, **not** evidence that every historical log or unreachable object is safe. The Beta audit reports an unresolved local risk; no publication or exfiltration conclusion is asserted.

**Documentation updates:** after every implementation slice, append case IDs, test outcome, model-free summary and source revision to the record, leaving private logs private.