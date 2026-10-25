# G5 T3 checkpoint: Native full Rig candidate + physical frame witness

**Date:** 2026-10-24
**Branch:** exp/g5-local-cas-transactions, local and uncommitted
**Existing baseline:** 0f8602fef213310af64a37815068cdfd44a38fe2
**Owner requirements:** Finish G5 without introducing Agent administration, public
model disclosure, unsafe retries, or false G5 APPLIED acknowledgments.

## Delivered in this stage

**1. G5 native full-Rig staging.** A new internal
`g5_txn_stage_native_candidate` operation in `studio_g5_txn.c` and
`g5_bus_stage_native_candidate_trusted` in `studio_g5_bus.c` stage a complete
Native owner-generated Rig candidate (role, beam, parent, weight, cross-link,
etc.) into an existing G5 draft. The operation checks lease owner, session,
generation, model signature/drawable, vertex count and Rig validation BEFORE
changing any draft bytes. Native caller-supplied snapshots are **NOT** accepted
from model or Agent IPC payloads; no model bytes are placed into receipts.
Commit, undo/redo and receipt handling reuse the existing G5 engine.
`tests/studio_g5_full_rig_stage_tests.c` proves atomic rejection and restore.

**2. Legacy physical solver/render frame witness.** New private C11
`studio_rig_frame_fence.c/.h` provides reset, bind, prepare, GPU accept and
invalidation tracking against exact model generation, Rig revision and frame
serial. The product Studio calls it only when a Rig is bound to XPBD and only
after the real XPBD prepare and GL finish/render-accept path. A modified
Rig revision immediately invalidates the old current-frame claim until a
fresh bind and successful render. The local-only V1 operation
`studio.g5.legacy_frame.status` returns bounded, source-free witness metadata
to `AgentStudioTools.legacy_frame_status()`. It always identifies G5 writer
as HOLD and G5 APPLIED as FALSE.

**This does NOT connect a G5 COMMIT to an accepted frame yet.** The accepted
revision is from the LEGACY Rig route. No caller can promote that status into
a G5 commit receipt; product G5 writer remains HOLD. No new public ABI20
symbols, public API services, SDK asset installation or Agent registrations.

## Physical and regression evidence

- Windows MSVC Release: `avatar_studio.exe` builds.
- `ctest --test-dir build/avatar-studio -C Release -R "^g5_"`: 16/16 PASS,
  including `g5_full_rig_native_stage` and `g5_legacy_frame_fence`.
- Python `pytest tests/moc3 tests/test_model_publication_guard.py`:
  **294 PASS, 19 SKIP, 204 subtests PASS**.
- Top-level Release project/ABI20 CTest: **10/10 PASS**.
- Real isolated Windows Studio:
  `python -B -m scripts.test_studio_g5_legacy_frame_live` PASS. GPU accepts
  a bound legacy Rig, a subsequent edit invalidates the old frame, and a
  successful rebind/accept creates a fresh witness. Never returns G5 APPLIED.
- Real product V1 recovery E2E: PASS two Studio sessions, lost ACK,
  duplicate denial, explicit reconcile, UNKNOWN after restart.
- G4 physical E2E: 37 cases PASS across 2 sessions, 9 cross-drawable PASS,
  and UI/Agent ABA stale-revision denial PASS.

## Remaining mandatory G5 gates

1. **Live G5Bus writer convergence — HOLD:** G5Bus is not yet the single
   authoritative document writer for all UI/V1/Agent operations. Present
   product Rig edits share a separate legacy candidate/commit dispatcher.
   Stage-native-candidate allows more complex G5 transactions, but by design
   is not exposed as a production IPC write.
2. **G5 document-to-frame binding — HOLD:** A real G5 COMMITTED document
   revision must be tied to a specific solver binding and GPU-accepted frame
   before `G5 APPLIED` can be acknowledged. The legacy witness is proof of
   physical accept machinery only, not this end-to-end invariant.
3. **Recovery durability — HOLD:** G5/V1 in-memory bounded receipts and
   explicit UNKNOWN handling are not restart-durable exactly-once guarantees.
   The live product V1 receipt ring stores 64 recent outcomes; expiry must
   never authorize a replay.
4. **Cross-surface concurrency / security review — HOLD:** Physical UI/V1/
   Agent edit convergence with active G5 draft leases, same-frame ABA,
   shutdown/restart, rollback, security regression and independent review
   still need evidence before enabling writer in the real Studio.

## Confidentiality and delivery

ADR-005 forbids publication of private model files, geometry, textures,
identifiers, screenshot evidence and paths. Model-dependent physical tests
ran only with an existing local fixture and cleared their private logs.
The new status reports contain numeric transaction/frame metadata only.
No commit, push, merge, production deploy or public SDK release occurred.


## Receipt provenance hardening

A Native full-Rig stage has an additional local-only semantic
`native_candidate_sha256` attached to its immutable COMMITTED receipt.
The SHA-256 input is a versioned, compiler-independent encoding of
validated Rig structure and excludes padding, paths, texture bytes,
vertex position buffers and logging. The original `payload_sha256`
continues to identify the command's canonical envelope, preserving
existing receipt lookup/reconciliation behavior and fixed test vectors.

The adversarial C11 test confirms that two different native candidates
with the SAME request envelope have identical envelope digests but
DIFFERENT candidate digests. Both values remain in private memory;
no candidate fingerprint or private model information is published.
This is not yet a trusted, production IPC proof of document provenance.

## Final verification snapshot (2026-10-24, Windows Release)

- Python unit/regression: **294 PASS, 19 SKIP, 204 subtests PASS**.
- Opt-in Native G5 CTest: **16/16 PASS**.
- Project ABI20/Release CTest: **10/10 PASS**.
- Real isolated Studio legacy XPBD/GPU frame test: **PASS**.
- Real V1 recovery and G4 native/cross-drawable/ABA regression: **PASS**.
- Working tree privacy preflight: **48 modified/untracked text files scanned,
  zero flagged categories**; committed HEAD scanned 662 Git blobs PASS.
- No push/merge, public model export, or production G5 writer activation.

**Review status:** Independent Codex/security review and real G5Bus product
writer release checks have not passed. Therefore the overall G5 completion
verdict is **REVISE / HOLD**, regardless of passing component regressions.
