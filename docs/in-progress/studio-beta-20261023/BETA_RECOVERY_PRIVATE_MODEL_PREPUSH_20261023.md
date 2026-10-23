# Beta recovery, model privacy and GitHub pre-push verification — 2026-10-23

Repository: boxs-51/Avatar_Runtime
Branch: exp/layer-physics-native-studio-local
Base committed HEAD before delivery: 2d2fccfaeece6ec686ca09e2e44ae4230574100c
Purpose: recover documentation after Computer Bridge tunnel loss, check model privacy, and publish the current G4/G5 local-only work to a new GitHub branch without merging main.

## Recovered documentation
- G4_SELF_REVIEW_G5_CAS_BOUNDARY_20261023.md: present, complete read.
- G5_LOCAL_CAS_ENVELOPE_PROPOSED_20261023.schema.json: present, JSON parsed and Draft 2020-12 schema checked.
- G5_CAS_TRANSACTION_GATE_REVIEW_20261023.md: present, complete read.
- ADR_PROPOSED_G5_004_LOCAL_CAS_RECOVERY_ROUTING_20261023.md: absent after tunnel interruption and reconstructed with full CAS, one-owner UI/Agent routing, safe ACK-loss status readback, undo/redo, model privacy and HOLD decisions.
All G5 artifacts remain PROPOSED. No Codex independent reviewer was invoked; separate reviewer gate remains NOT_RUN.

## Model data security audit
- Examined local Git status candidates prior to staging: 34 text-only files, none binary. A second staged-index scan must also pass before push.
- No private model binary, raw vertex/texture/indices array payload or detected secret token appears in those candidate files.
- E2E loads private fixtures through _private_manifest; private IPC logs and raw captures remain under ignored build/studio-user, not tracked Git.
- New .gitignore rules cover Cubism model inputs (*.moc3, *.model3.json, *.physics3.json, *.exp3.json, *.motion3.json, *.pose3.json, *.userdata3.json) and dedicated private capture/model directories; these protections were verified with git check-ignore.
- Existing /build/, /local-assets/, /screenshot*.png protections retained.
- Privacy conclusion: PASS for inspected intended Git inputs, not a claim of OS sandboxing or a full historical Git purge.

## Reverification
- Python default tests/moc3: 204 PASS, 18 SKIP, no FAIL (opt-in visible UI and private viewer/dependency cases intentionally skipped).
- Windows Release CTest: 10/10 PASS including C11/C++20 ABI20.
- Physical Windows G4 Native E2E: 37/37 PASS, 2 Studio sessions, raw private not exported.
- Native cross-link E2E: 9/9 PASS with target_child_exceeds_source=true.
- Native Agent ABA: REJECTED_ABA_STALE_REVISION PASS.
- git diff --check: PASS. Public ABI20 paths untouched.
- G5 T2 Native writer, GUI E2E and independent Codex review: HOLD / NOT_RUN.

## Delivery policy
Publish only reviewed relative UTF-8 source/test/design files and this report to the feature branch. No private model assets, binaries, captures, logs, credentials, model source paths or generated build products may be staged. New remote feature branch is not main; no merge, deployment or writer activation.
