# Native Studio Beta A��'A+�?TA��?�A��,��,�A��'A��,���A��?sA,A�A��'A+�?TA��?sA,A�A��'A,A�A�A�A��,���A,A�A��?�A,A�A��'A��,���A��?sA,A�A��'A+�?TA��?sA,A�A��'A,A�A�A�A��?sA�A.A�A��?sA,A�A��'A��,���A��?sA,A? architecture and evidence dossier

- **Artifact ID:** STUDIO-BETA-DOSSIER-20261023
- **Version / date:** 0.1 / 2026-10-23
- **Role / authority:** Development design and documentation, not an independent audit or merge approval
- **Repository baseline:** Local `exp/layer-physics-native-studio-local` at `e38eba5f4b1f21e8590520ef1aefb3a7cfa8407e`, as inspected on the Windows Host
- **Disposition:** DOCUMENTATION READY; PRODUCT BLOCKED / REVISE
- **Owner:** Repository owner; design and remediation decisions remain pending
- **Confidentiality:** This dossier is source-only and excludes model identifiers, file hashes of private assets, screenshots, geometry statistics, logs and model-derived receipts

## Document map

| ID | Document | Purpose |
|---|---|---|
| SYS-001 | `SYSTEM_BASELINE_AND_BOUNDARIES.md` | Inspected system components, state ownership, boundaries and constraints |
| ADR-001 | `ADR_PROPOSED_HIERARCHICAL_SKINNING.md` | Next architecture: transforms, skin weights, XPBD coordination and migration |
| AUD-001 | `AUDIT_TRIAGE_AND_GATE_LEDGER.md` | Severity-ranked findings and required evidence before further implementation or release |
| VER-001 | `RED_FIRST_VERIFICATION_MATRIX.md` | Adversarial and functional RED-first test definitions, status and gate ownership |
| LOG-001 | `EVIDENCE_AND_LOG_POLICY.md` | Privacy-safe report formats, private log retention, naming and recovery procedures |
| F02-UO-INTENT | F02_UNKNOWN_OUTCOME_LOCAL_INTENT_20261023.md | Windows client crash, UNKNOWN_OUTCOME quarantine and RED-first evidence |
| F02-ACK-OWNERSHIP | F02_ACK_OWNERSHIP_AND_DEFERRED_PUBLISH_20261023.md | Windows native response ownership and deferred ACK publication |
| ADR002-G4-IDS | ADR002_G4_LOCAL_MODEL_IDS_20261023.md | Native generation-fenced parameter/part ID pages and exact lookup |
| F02-NATIVE-REPLY | F02_NATIVE_REPLY_ATOMIC_20261023.md | Complete Native ACK publication and bounded F02 follow-up |
| ADR002-EPOCH-G4 | ADR002_G4_NATIVE_GENERATION_FENCE_20261023.md | Native epoch and generation-guarded local read receipts |
| ADR002-LOCAL-G4 | ADR002_LOCAL_ONLY_G4_READBACK_20261023.md | Updated Local-only scope and bounded V1 read adapter |
| ADR002-G4 | ADR002_G4_DISCOVERY_IMPLEMENTATION_20261023.md | Implemented offline-only V2 schema/capabilities prototype and remaining gates |
| REMED-001 | `FINDINGS_REMEDIATION_20261023.md` | Scoped code remediation verdicts, evidence and remaining gates |
| LOG-002 | `ENGINEERING_PROGRESS_LOG.md` | Engineering history and decision checkpoint, explicitly not a fresh test run |

## Source custody

Primary audit evidence: `docs/BETA_EXPANDED_QUALITY_SECURITY_ARCH_AUDIT_20261023.md` and `docs/BETA_AUDITOR_SESSION_20261023.md`; both are independent, untracked auditor artifacts and **are not changed, staged or adopted as approver signatures** by this dossier. Source readback on current HEAD independently confirmed the risky native EOF pattern, the file IPC admission sequence, the direct rig overwrite and the current XPBD ownership boundaries.

Older Studio design: `docs/in-progress/INTERACTIVE_STUDIO_DESIGN.md`. Runtime/API behavior: `tools/avatar-studio/AGENT_API.md`. SDK production gates remain distinct from Native Studio experiment gates.

## Documentation protocol for every subsequent engineering slice

1. Record baseline commit, proposed change, required authorization and affected trust boundaries **before edits**.
2. Add a RED-first test ID and reproduction evidence; do not weaken existing oracles.
3. Store native test output, private traces, images, model-derived receipts and workbench files under the Git-ignored `build/studio-user/` or separately authorized local custody.
4. Record only **case IDs, PASS/FAIL, non-sensitive error class, source commit, toolchain/config and evidence pointers** in repository Markdown.
5. After GREEN and scope review, update the architecture delta, test matrix and audit disposition. A developer's PASS cannot close an auditor finding or unblock release by itself.
6. Never promote raw logs, model binaries, screenshots, derived geometry counts or asset hashes to PRs/issues/CI, and never delete/prune Git object evidence before owner-authorized distribution assessment.

This is a living documentation index: update it when an approved implementation changes behavior, not to assert unverified future work.
