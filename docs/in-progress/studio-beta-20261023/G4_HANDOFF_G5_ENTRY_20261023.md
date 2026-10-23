# G4 checkpoint to G5 handoff (2026-10-23)

## Exact state
- Repo: boxs-51/Avatar_Runtime; Windows beta workspace, branch exp/layer-physics-native-studio-local.
- Committed HEAD: 2d2fccfaeece6ec686ca09e2e44ae4230574100c.
- G4 source is UNCOMMITTED; not released, merged or deployed.
- Local snapshot: build/studio-user/g4-checkpoint-20261023/manifest.json, 13 named files with SHA-256 and safe backups.
- Checkpoint manifest hash SHA-256: ce060bfe4ca232f034fd37a0d6c81702bee1b451d5c1c2b0e9da5873de5d4461.
- Preserve all unrelated changes and untracked files. No reset, clean, force push or GC.

## G4 verified scope
- Native revision-fenced Rig V4 readback rejects Agent/Agent mixed snapshot (STALE_REVISION).
- Physical Native ABA: identical before/after anchor configuration, monotonically higher revision, rejection of in-flight read. Two PASS runs.
- Five Native G4 endpoints in two process sessions: 37/37 PASS.
- Updated legacy revision assertion without removing behavior and privacy checks.
- Extended Python regression: 57 tests, 85 subtests PASS twice.
- Windows Release CTest 10/10 PASS; no public ABI20 diff; git diff --check PASS.

## Outstanding gates
- Independent G4 code/security review NOT_RUN; same-agent self-review does not count.
- Native UI-click vs Agent test BLOCKED (Computer Bridge DESKTOP_INPUT_DISABLED).
- Model hot reload, F02 lost-ACK recovery, F03 private Git object custody and F05 disk durability remain open.
- F02 ACK synthetic timing test once failed amid contention, isolated and two broad reruns PASS: investigate flakiness, no oracle weakening.
- G4 SCOPED VERIFIED, G4 RELEASE HOLD.

## G5 authorization boundary
- User has directed marking G4 and switching work to G5.
- Effective local design is ADR_PROPOSED_AGENT_TOOLKIT_API_V2.md: G5 staged CAS editing, one-writer lease, editor commit versus accepted physics/frame state, undo, readback and conflict rejection.
- Older G4 checkpoint says G5 staged Native editor commands depend on independent G4 review. This prerequisite remains unmet; source document is PROPOSED, not proof of Owner approval.
- G5 T0/T1: scope and RED-first tests-only are permitted here; tests must be opt-in and preserve default regression.
- Activating a G5 Native writer, stage/commit/undo runtime or direct UI change is ON HOLD until independent G4 review and resolved command schema. Do not route around this through V1.
- No new public SDK ABI, network service, arbitrary filesystem path or model asset export.
