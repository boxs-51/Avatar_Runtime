# PUB-04-B independent review

2026-10-19, Asia/Bangkok. Separate read-only reviewer agent
`/root/f2_independent_review`, delta from `c925bb0` against
[CP-01..04 C0](../../in-progress/M2_MOC3_PUB04_B_CHECKPOINT_DESIGN.md).
The [source closure](../../reference/audits/M2_MOC3_PUB04_B_SOURCE.json) pins
the reviewed/tested implementation; reports are subsequent handoff artifacts.

**Final scoped code/evidence verdict: PASS. No supported open P0/P1/P2.**

## Independent source and architecture observations

Package identity, exact predecessor digest and typed checkpoint/drawable
validation precede GPU submission. Restored chain/particle shape is checked
against the actual linked solver before update. Actual state remains speculative
until the existing final core authorization; immutable checkpoint pointer and
cursor assignment precede target advertisement without fallible worker IPC.
Failed prepared sessions retain the old checkpoint/image and cannot remint
authority. Hashes are custody checks, not worker-fidelity authentication.

The final legacy begin wrapper checks owner thread before reading step-session
state. The rebuilt fence test exercises wrong-thread begin, begin_step,
stage_step, state, checkpoint access and close. Generated tests cover hostile
ACK/model/nonfinite/trailing output, pre-GPU rejection, exact checkpoint/frame
retention and final cancel/revoke/elapsed-expiry behavior. Native multichain
restore compares exact solver output and serialized state with continuous
execution, including fixed-step remainder, OFF and reset.

The reviewer initially marked acceptance UNVERIFIED pending direct numeric,
negative and source-pinned actual evidence. The primary agent subsequently
caught stale host/fence binaries after late CMake regeneration and rebuilt them.
Superseded runs `5ade0d2718b9429d997e1cd0` and `f958d2d7eee8422c8433468a` were
excluded from final closure. No unsupported code finding or gate relaxation was
used to explain away that verification gap.

## Independently executed checks

Each command ran once with elevation for the native GPU/temp-file environment:

* `python -B -m unittest discover -s tests/moc3 -p test_step_publication.py -v`: **5/5 PASS**, exit0,
  `build/pub04b-review-step.log`.
* `build/moc3-public-isolation/bin/Release/ars_moc3_checkpoint_tests.exe tests/moc3/generated_physics3_multichain.json`:
  **PASS**,741 malformed cases rejected, exit0, `build/pub04b-review-native.log`.
* `build/m2-d-private/bin/Release/ars_moc3_publication_fence_tests.exe`:
  **PASS**, exit0, `build/pub04b-review-fence.log`.

Reviewer reconciliation is retained at `build/pub04b-review-reconciliation.json`.
Final checkpoint `68bdf1b16b4f45c4b3a86898` and legacy publication
`2ac740fd29654df69f37f256` have native/API/UI PASS. Each child before/after and
parent before-creation/after-exit capture equals the current code/binary/dependency
capture. All eight accepted sequences per backend were observed; mapping,
image/sample identities,401 negative and descriptor removal agree. Final logs
record integrated66/66 and isolated32/32 PASS. Private hashes/data remain ignored.

Final review supports CP-01..04 in this bounded method. Architecture assessment:
**SUPPORTED**. Project structure assessment: **SUPPORTED**. Optimization proposals:
**NO_RECOMMENDATION**. Final document/source/goal reconciliation is completed by
the primary agent in the [acceptance report](M2_MOC3_PUB04_B_ACCEPTANCE.md).
Persistent PUB-02, full product PUB-04, SDK and physical D06/D08 remain HOLD.
This review is not merge/release authority or author-equivalence qualification.
