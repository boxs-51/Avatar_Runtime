# F02 — Local UNKNOWN_OUTCOME intent quarantine

- Date: 2026-10-23
- Workspace: Beta, branch exp/layer-physics-native-studio-local
- Baseline: 86f351efcb48520ff2b559ffa5e487febabdf7f4
- Scope: cooperative local Python AgentBridge writer only
- Product release: HOLD
- Evidence class: Windows local synthetic + real Native integration; development self-test, not independent security approval

## Contract and authorized constraints

- F02-UO-REQ-01: never silently allow a follow-on command when the
  preceding command might have executed but a complete ACK was lost.
- F02-UO-REQ-02: continue accepting valid, complete OK/ERR responses
  and retain prior strict token/frame, OS writer-lock and Native behavior.
- F02-UO-REQ-03: no model assets, IDs, geometry or raw receipts in Git,
  tool outputs or public reports. No push, merge, deploy or Git GC.
- F02-UO-REQ-04: no claim of durable exactly-once execution, authorization
  against malicious same-user processes or accepted XPBD frame semantics.

## Root cause and RED proof

In AgentBridge.call, timeout used to remove a matching request/response
in finally and release the writer lock. If Native had already performed
a mutation before crashing or losing the ACK, the queue could look empty
to another cooperative client, allowing an unsafe manual replay.

Added tests/moc3/test_studio_unknown_outcome.py on unchanged baseline.
Observed BASELINE_RED: three specific tests failed/error because
there was no persistent uncertain-outcome marker; one normal complete
ACK control passed. The simulated worker wrote one synthetic "applied"
sentinel, then stopped before ACK without loading any private model.

## Minimal implementation

Before publishing request.txt, AgentBridge now creates
uncertain_outcome (exclusive file creation, flush and OS file fsync).
Its content is a random request token only, not a raw request or model
identifier. The cooperative writer checks this marker, request.txt,
response.txt and orphaned response.tmp before publishing a new request.

A complete own-token, newline-terminated typed OK/ERR sets
acknowledged; only after successful cleanup of outstanding queue files
is the local marker removed. On timeout, incomplete ACK or failure,
the marker remains and subsequent cooperative calls fail closed with
a recovery-inspection error. No request is automatically replayed.

## Test observations

- BASELINE_RED: 3 failing/quarantine scenarios; normal ACK control PASS.
- POSTFIX GREEN: all 4 synthetic unit scenarios.
- Windows PHYSICAL process-kill fixture: 5/5 PASS. Child agent was
  terminated after a disposable synthetic effect but before ACK;
  persistent intent prevented the next cooperative client from
  sending the same command after the OS writer lock was released.
- V1 Agent unit, V2 tests, public-safe reporter, ACK harness PASS.
- Local Native Studio API E2E, native grammar, model-generation
  readback, model-restart, IPC race and rig durability PASS.
- All results are local; GitHub CI NOT_RUN and no independent auditor
  performed a separate review of this change.

## Unresolved security boundaries

This marker is not a crash-proof Native commit journal, a privilege
boundary or a true accepted-frame receipt. Arbitrary same-user
processes can still edit/delete local IPC files. Crash at every
possible OS write boundary, power-loss/fsync directory durability,
reconciliation after UNKNOWN_OUTCOME, and replay idempotence remain
unverified. The marker intentionally requires explicit recovery
inspection rather than automatically retrying unknown writes.

F02 remains PARTIAL; F03 HOLD, F04 PARTIAL, F06 OPEN, F07 PARTIAL.
The G4 Local-only API and Studio RELEASE gates remain HOLD/PARTIAL.
