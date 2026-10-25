# G5 checkpoint: Legacy Rig semantic CAS hardening (2026-10-25)

**Branch:** `exp/g5-local-cas-transactions`, local uncommitted workspace.
**Baseline:** `0f8602f`.
**Scope:** Owner-thread legacy Rig dispatcher and model-free Native test only.
**Not enabled:** G5Bus product writer, new public Agent API, model export, ABI20 changes, release, merge or push.

## Finding, RED-first reproduction and fix

- Finding: `studio_rig_dispatch` used `memcmp` of the complete `StudioRig` to
  decide whether a command was an unchanged edit. Inactive array tails and
  padding are not the logical Rig state, but can differ after editing/removal.
  This could advance legacy CAS revision without a meaningful Rig change.
- RED-first: `studio_rig_dispatch_tests` added an owner-thread command that
  modifies only an inactive array tail. The old implementation returned an
  accepted mutation instead of `STUDIO_RIG_DISPATCH_UNCHANGED`
  (`RIG_DISPATCH_RED line=34`).
- Fix: compare identity, role, counts and all ACTIVE Rig elements by value.
  Ignore inactive tails and structure padding. Before committing a candidate,
  reject a changed model identity or out-of-bounds Rig counts. Keep candidate
  copying, atomic rollback on command failure, generation/revision CAS fence,
  and a single revision increment for real accepted edits.
- Negative regressions: changed model signature and count overflow are rejected
  without changing the live Rig or revision.

## Evidence from the local Windows Host

- `cmake --build build/avatar-studio --config Release --target studio_rig_dispatch_tests --parallel 1`: PASS after change.
- `ctest --test-dir build/avatar-studio -C Release -R ^g5_rig_owner_dispatch$ --output-on-failure`: RED before fix, GREEN after fix.
- `ctest --test-dir build/avatar-studio -C Release -R ^g5_ -E g5_(process_recovery_fixture|agent_tools_native|agent_tools_ipc) --output-on-failure`: 13/13 PASS (prebuilt tests except rebuilt dispatcher test).
- `python -B -m pytest -p no:cacheprovider -q` on the seven selected
  G5/dispatcher/IPC/frame/recovery modules: 60 PASS.
- Host available memory observed at 0.58 GiB (92% RAM usage) during this
  checkpoint. Expensive Studio Release rebuild, GPU/XPBD physical E2E, full
  Python regression and three multi-process fixture tests are NOT rerun in
  this low-memory session.

## Gate decision

**G5 writer: HOLD.** This removes one false revision edge in the legacy route.
It does NOT prove unified UI/V1/G5Bus writer authority, G5 document-to-frame
GPU acceptance, crash-durable exactly-once receipt recovery, or live
same-frame ABA/cross-surface concurrency. The separate legacy CAS counter
must not be misrepresented as G5 document CAS or `APPLIED`.

No model files, geometry, screenshots, private paths, textures, or model
identifiers are written to this checkpoint. Preserve ADR-005 privacy rules.

## Next safe evidence gate

When the Host has sufficient available RAM, build `avatar_studio` Release
with one MSBuild job; run the full opt-in G5 CTest, Python regression and
isolated physical Studio UI/V1/Agent/XPBD-GPU trials. Obtain independent
review of the semantic equality/validation approach before any writer
convergence or production enablement.
