# LOG-002 — Engineering progress and decision log (public-safe)

- **Date:** 2026-10-23, Asia/Ho_Chi_Minh
- **Repository branch:** `exp/layer-physics-native-studio-local`
- **Inspected HEAD:** `e38eba5f4b1f21e8590520ef1aefb3a7cfa8407e`
- **Role:** Development documentation; NOT independent auditor certification
- **Status:** Architecture documentation completed for this checkpoint; source/security remediation NOT_STARTED

## Checkpoint STUDIO-BETA-DOC-001

**User goal:** Maintain complete system architecture, test plan, report and logs throughout development, and continue design of real hierarchical bones/skinning.

**Read/compared:** New Beta expanded audit report, auditor session authority marker, current Native Studio source/API and Studio design material. Current HEAD matched the auditor's pin. Verified static presence of numeric-tail native parse pattern, file IPC nonexclusive admission sequence and direct rig overwrite. Did **not** re-run the auditor's physical reproducer.

**Findings carried forward:** F01 P1 OPEN; F02–F05 P2 OPEN; F06–F07 P3 OPEN. Native Studio write-path integrity and privacy/release gates remain HOLD. Successful historic regression evidence is scoped and does not close these findings.

**Artifacts produced:** SYS-001, ADR-001, AUD-001, VER-001, LOG-001, this progress log and the dossier index. All are source-only Markdown. A session manifest is retained under the Git-ignored private log location described in LOG-001.

**Design outcome:** Prefer a feature-gated internal bone-DAG/inverse-bind linear blend skinning evaluator with explicit frame ownership and rollback. Do not equate existing XPBD parent links with skeletal transforms.

**Tests executed in this checkpoint:** No new native/API test execution. Source inspection and document-file verification only. RED scenarios are imported from the auditor's report or marked PLANNED explicitly.

**Work remaining before implementation:** Owner design review, P1 strict native parser RED-first remediation authorization, IPC lifecycle design and test, evidence privacy gate, atomic rig-save design, then synthetic skinning spec and tests.

**Safety/custody:** No model/texture/screenshot/raw native log has been added to these documents. Auditor files and IPC remain untouched; no GitHub write/push, history rewrite, deployment or merge undertaken as part of documentation.
## Checkpoint STUDIO-BETA-REMED-001 — 2026-10-23

Observed scope: F01 exact Native wire grammar, F02 cooperative OS writer-lock admission, F04 privacy-safe verdict reporter, F05 durable temp-and-replace rig save, and F07 regression extensions. F03 local Git-object custody is preserved without garbage collection or history rewriting. F06 extraction/optimization remains pending.

RED-first: 8 invalid native suffix cases reproduced on the Windows Host and a synthetic interrupted rig save failed against old source. GREEN: native syntax 48-case matrix, concurrent callers, abrupt writer termination, rig fault injection, API/rig/XPBD/local-model E2E and strict public-report schema. No new independent auditor sign-off is claimed.

Detailed local-only stdout, toolchain outputs and evidence remain under ignored build/studio-user. Read `FINDINGS_REMEDIATION_20261023.md` for exact gate distinctions: F01/F05 provisional GREEN, F02/F04/F07 partial, F03 HOLD, F06 OPEN. Product release remains HOLD.

## Checkpoint ADR002-G4-DISCOVERY — 2026-10-23

A new isolated Avatar Toolkit V2 decoder, read-only capabilities/schema
catalog and local CLI were implemented with RED-first Python tests.
V2 Native IPC, session manager, named pipe, transaction journal and edit
operations remain DISABLED while Beta G2/G3 are outstanding.
F02 added a real synthetic regression for incomplete ACK false success:
the client no longer accepts an unfinished OK frame or clears a foreign ACK.
See ADR002_G4_DISCOVERY_IMPLEMENTATION_20261023.md for evidence and limits.

No independent auditor has re-reviewed these changes. Release gate HOLD.
