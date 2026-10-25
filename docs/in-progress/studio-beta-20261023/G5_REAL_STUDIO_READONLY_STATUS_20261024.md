# G5 Native Studio read-only ingress — integration checkpoint

**Date:** 2026-10-24
**Repo / branch:** Avatar_Runtime / exp/g5-local-cas-transactions
**Working baseline:** 0f8602fef213310af64a37815068cdfd44a38fe2
**Status:** Studio read-only capability bridge implemented; G5 writer HOLD.

## Product ingress contract

The test-only Agent G5 tooling already supports synthetic Native transactions
through two IPC lanes. The shipping Studio still directly mutates its Rig in
multiple UI and V1 paths, so it is not safe to claim G5 transaction writes
or XPBD/GPU accepted-frame proof.

The first safe product connection is an exact, metadata-only handshake on the
existing Studio local V1 request channel.

- `studio.g5.status` -> `G5_LOCAL_STATUS` (no arguments), validated by the
  existing local AgentBridge.
- `studio_agent_extended.inc` reads only owner-thread
  `s.model_generation` and `s.rig_revision`.
- Reusable internal C11 function
  `studio_g5_live_status_format` in `studio_g5_live_status.c/.h`
  formats a bounded read-only response, linked to the **real**
  `avatar_studio` executable, not to the public ABI20 SDK.
- Exact reply grammar: `writer=HOLD txn=OFF frame=UNVERIFIED gen=N rigrev=N`.
  It contains no model asset, path, geometry, parameter ID or capture.
- `AgentStudioTools.g5_status()` validates the reply and returns
  `classification=LOCAL_PRIVATE`, model generation, **legacy**
  Rig revision and three explicit false/HOLD flags.
- Legacy Rig revision is **not** G5 document CAS revision.
- Unsupported old Studio, forged extra fields, overclaiming of a connected
  writer/frame and malformed receipts fail closed.

## Verification

- RED-first Python tests fail before the operation parser and helper are
  implemented; post-implementation 7/7 initial tests PASS.
- C11 model-free `g5_live_status_hold` passes on Windows Release.
- `avatar_studio.exe` Release compiles and links the new status module.
- The change does not enable G5 UI/Agent writing or change ABI20.
- Separate tests and two-process synthetic G5 writer remain available and
  must not be mistaken for product model editing evidence.

## Remaining G5 HOLD gates

1. Route all physical UI/V1/Agent rig edits into one Native transaction
   dispatcher (direct `s.rig` edits presently bypass G5).
2. Bind a real local recovery IPC path to committed owner receipts with
   pre/post-effect loss and crash/restart evidence.
3. Prove accepted solver/GPU frame fences for each committed document revision
   before returning `APPLIED`; `COMMITTED` alone is not `APPLIED`.
4. Independent review, full regression, model-publication safety gate.

**Security and scope:** per ADR-005, model information stays non-public.
A local status read is not authorization to publish private model metadata.
No new Agent identity registry, permissions administrator, public endpoint,
main merge, deployment, or live G5 writer enablement.

## Real Windows Studio owner-thread E2E

The `scripts.test_studio_g5_live_readiness` test launches two independent
Studio trial sessions against an existing **local private** E2E model.
Agent calls the exact V1 `studio.g5.status` operation and checks that the
writer/transaction/frame gates remain OFF. A real V1 `rig.anchor` edit
advances the legacy Rig revision reported by that status, but does not
report G5 COMMITTED or APPLIED. Two process lifetimes have distinct model
generation tags. Trailing arguments fail and V2 `draft.commit` stays denied.
All private fixture logs are temporary local files and cleaned up, with
no model identifiers, paths or geometry printed to public output.

**Observed:** `G5_REAL_STUDIO_STATUS_PASS` in both isolated sessions.
This is **not** a real G5 transaction-write or accepted-frame E2E gate.
