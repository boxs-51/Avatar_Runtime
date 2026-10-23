# G4-META-05 / G4-CROSS-06 / G4-TEST-07 — RED-first remediation (2026-10-23)

## Workspace and authority
Repo boxs-51/Avatar_Runtime. Windows Computer Bridge workspace beta, branch exp/layer-physics-native-studio-local, committed HEAD 2d2fccfaeece6ec686ca09e2e44ae4230574100c. Pre-existing dirty worktree preserved. No commit, push, merge, deploy, model write or ABI20 public change. This is a coder validation packet, NOT independent review.

Before edits, created verified copies of six source/harness/fixture paths under ignored build/studio-user/g4-meta-cross-test-before-fix-20261023/.

## Explicit defect contract
- G4-META-05: malformed topology metadata must fail closed; island counts, largest component, vertices/used/loose/triangles are internally constrained according to studio_layer_components().
- G4-CROSS-06: reading target vertex count must be independent from triangle/index/topology availability, yet strict on model generation, drawable identity, vertex bounds and Native IPC EOF.
- G4-TEST-07: Native cross E2E must never PASS unless target vertex count exceeds source vertex count, and the tested child vertex is outside the source range but valid in target.

## Baseline RED
Command: python -B -m pytest -p no:cacheprovider -q tests/moc3/test_studio_g4_meta_cross_redfirst.py --tb=short.
Observed before source edits: 9 semantic failing cases/subcases, 2 healthy controls PASS. Failure categories: invalid topology exposed, cross readback dependent on topology, wrong target identity/bounds, absent vertex-count wire, and missing strict-target harness invariant.

## Focused correction
- scripts/avatar_toolkit_v2_local.py: validate used>=3; islands <= min(triangles,used//3); largest bounded by component count and at least 3 vertices per island; preserve used+loose=vertices. Add _g4_target_vertex_count with strict receipt, drawable equality and vertex range 3..2048. Cross validation now consumes this independent count, not topology metadata.
- tools/avatar-studio/studio_agent_extended.inc: add bounded, read-only, generation-fenced G4_MESH_VERTEX_COUNT; no dependency on triangle/index buffers; strict EOF, drawable range, vertex range.
- scripts/studio_agent_api.py: whitelist/serialize g4.mesh.vertex_count exact wire fields; reject malformed/extra arguments.
- scripts/test_studio_g4_cross_live.py: strict target selection, explicit BLOCKED when unavailable, four physical Native malformed/stale/out-of-range negative cases.
- tests/moc3/test_studio_g4_meta_cross_redfirst.py: baseline oracle preserved. Two pre-existing synthetic fixtures updated solely to answer new target-count opcode.

## Postfix evidence
- Focused new+previous tests: 20 PASS, 25 subtests PASS.
- Full python -B -m pytest -p no:cacheprovider -q tests/moc3 -rs --tb=short: 204 PASS, 18 SKIP, 204 subtests PASS. One visible UI opt-in and 17 private viewer/dependency/profile SKIPs; not counted as PASS.
- cmake --build build/avatar-studio --config Release --target avatar_studio --parallel 2: PASS. Existing MSVC warnings remain.
- Native Windows: scripts/test_studio_g4_native_live.py 37/37 PASS; scripts/test_studio_g4_parent_live.py 8/8 PASS; scripts/test_studio_g4_ui_race_live.py --run-aba returns REJECTED_ABA_STALE_REVISION PASS; scripts/test_studio_g4_cross_live.py 9/9 PASS with target_child_exceeds_source=true and physical wire negative cases.
- ctest --test-dir build -C Release --output-on-failure --timeout 25: 10/10 PASS, including ABI20 C11/C++20.
- git diff --check exit 0; git diff --name-only -- include/ars/ars.h src/abi CMakeLists.txt empty.

## Boundaries still unresolved
- Impossible topology receipts injected via synthetic bridge. Physical evidence that valid Native model with zero/invalid target triangle topology can still support rig.cross is NOT_RUN; independent vertex-count opcode physical positive is proven on a model with topology.
- No fresh host GUI click-based concurrent E2E (desktop input gate previously disabled).
- Independent reviewer NOT_RUN. G4 scoped fix GREEN, G4 overall release HOLD, G5 T2 Native writer HOLD.
