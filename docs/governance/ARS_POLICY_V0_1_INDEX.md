# Avatar_Runtime — Modular governance proposal v0.1 (selective reference)

**ADOPTION_STATE=PROPOSED_NOT_CANONICAL** — OWNER REFERENCE YES ONLY; no merge/policy activation.

**Decision:** ARS_POLICY85_V3_SELECTIVE_REFERENCE_ACK=YES, received from the sole Human Owner in chat on 2026-10-15 Asia/Ho_Chi_Minh. This explicitly authorizes drafting a local governance proposal, **not** adoption, activation, release or merge. When relayed by GPT through the same connected GitHub account, the decision is **owner instruction relayed by GPT**, not an independently authenticated GitHub-owner comment or a second human actor.

**REFERENCE_VERSION=assistant-policy-85-v3.0**. Source index: https://github.com/boxs-51/assistant/issues/85; modules https://github.com/boxs-51/assistant/issues/526 through #531, owner-directed status as read on 2026-10-15. These external issues are **reference material**, not active rules of this repository. **NO_AUTOMATIC_INHERITANCE**: later external edits do not change this proposed or future adopted policy without a scoped Avatar_Runtime owner decision. Archived v2.6 / #532 is historical only. Repository-specific terms here supersede no current GitHub protection or security gate.

## Local modules and exact source mapping (one concern per document)

| Local proposed module | Local source | Assistant v3.0 reference | Intended owner |
| --- | --- | --- | --- |
| ARS-M1 — Sole Owner and direct ACK | docs/governance/ARS_M1_SOLO_OWNER_ACK.md | [#526](https://github.com/boxs-51/assistant/issues/526) | Human Owner |
| ARS-M2 — Developer cycle | docs/governance/ARS_M2_DEVELOPER.md | [#527](https://github.com/boxs-51/assistant/issues/527) | GPT Developer |
| ARS-M3 — Final-only audit and exception | docs/governance/ARS_M3_FINAL_AUDIT.md | [#528](https://github.com/boxs-51/assistant/issues/528) | GPT Auditor + Human Owner |
| ARS-M4 — Real stacked PRs | docs/governance/ARS_M4_STACKED_PRS.md | [#529](https://github.com/boxs-51/assistant/issues/529) | GPT Developer |
| ARS-M5 — One-PR merge / history | docs/governance/ARS_M5_MERGE_HISTORY.md | [#530](https://github.com/boxs-51/assistant/issues/530) | Human Owner |
| ARS-M6 — Integration Wave choice | docs/governance/ARS_M6_WAVE_STATE.md | [#531](https://github.com/boxs-51/assistant/issues/531) | Human Owner |

The separately owned existing Draft PR #11, at PR11_PARENT_SHA=419a94bd567986ab5edd8ae5f4f11e6fae8ee4fb, introduces docs/GOVERNANCE_ARS_SOLO_OWNER_001.md as the **existing owner-approved procedural exception proposal**; ARS-M1/ARS-M3 refer to it, never silently replace it. This PR is a child of #11 and contains only governance proposals, tests and CI. Keep each module separate; if/when the owner adopts this structure, maintain topic-specific issue(s) and module files individually rather than treating Issue #6 or one master policy issue as an operational review queue. Separate issue creation and canonical adoption need their own documented owner decision.

## Current owner decisions and hard safety boundaries

- OWNER_G00_FREEZE_SHA=fe3e680a44ac4e71ef57dc177e73ae31198fcbd0. G00 **DESIGN FREEZE OWNER YES** is already documented for Draft PR #14, scoped to that precise engineering source. This proposal does not change its bits, reopen a resolved design choice, certify production GPU or security behavior, or authorize an M1 source claim.
- PR15_MAIN_INTEGRATION=MERGE_HOLD. Draft PR #15 at 7cf306dd783edd7e155ac26517c4b99b8b449552 is only a main-target pre-merge candidate. “IW” appearing in historical PR labels does not equal an accepted Wave or a merge decision.
- **INDEPENDENCE_NOT_AVAILABLE** for the one-account situation. GPT-written GitHub comments/commits are not a second qualified reviewer; no fake APPROVED or independent PASS. The procedural gap is owner-accepted under the existing ARS-SOLO-OWNER-001 exception, while substantive review and all applicable CI/security obligations persist.
- **MERGE_NOT_AUTHORIZED** for PR #11, this proposal, PR #15 and all other Draft PRs. **M1_NOT_AUTHORIZED**; no native renderer/runtime/GPU/87 production security cases can be claimed complete from G00 design tests.
- **GITHUB_PROTECTION_VERIFICATION=HOLD**: existing access to the private repo's main protection endpoint returned HTTP 403; likewise rulesets may require plan/access changes. Never interpret 403 as “no branch rules”; authorized owner must inspect actual GitHub Settings before a merge.
- LOCAL_WAVE_STATE=NOT_ADOPTED: assistant #531's suspension applies **there**, not automatically here. The local Wave decision is proposed and awaiting explicit owner disposition, with no automated Wave planning or batch merge.
- GitHub platform protections and law are superior to any repo-authored workflow. Latest exact-scope Human Owner choices govern eligible repo-authored process after adoption, with stricter issue-local technical/security gates still enforced. On conflict or ambiguous permission: HOLD rather than infer.

## Proposed lifecycle if separately adopted

GPT Developer: design, bounded source work on scoped branches, self-checks, real negative/regression tests and CI, repair → one stable exact-HEAD handoff → GPT Auditor: final comprehensive technical-method review with disclosed reviewer limitations → Human Owner: reserved YES/REVISE/HOLD → when authorized, one exact PR by MERGE_COMMIT (only if GitHub allows), verify landed SHA and rollback → parent-first next PR. No mandatory intermittent audit signoff; no bulk/automatic Wave authority.

## Adoption and rollback gate

**REFERENCE_ACK=YES** permits maintaining this PR only. Separate local **ARS_POLICY_V0_1_ADOPTION_ACK=YES/REVISE/HOLD** should identify modules, PR/full HEAD, effective date, any Wave state, compatibility with PR #11 and branch protections. A further **PER_PR_MERGE_ACK** remains mandatory. Do not treat opening, testing or approving a module's design as an authorization to merge or start M1.

Rollback before adoption: close this Draft PR or abandon its branch; the parent PR #11, owner G00 freeze, main, source CI and protection remain unchanged. After future adoption: controlled change through the affected small module(s) and a separately approved PR, preserving commit history and audit trail.
