# Avatar_Runtime — Modular governance v0.1 (owner adopted; main integration pending)

**ADOPTION_STATE=OWNER_ADOPTED_PENDING_MAIN** — Human Owner approved local governance v0.1 in chat; Draft PR not landed, no merge/M1/Wave authorization.

**Decisions:** ARS_POLICY85_V3_SELECTIVE_REFERENCE_ACK=YES authorized scoped reference and drafting; **ARS_POLICY_V0_1_ADOPTION_ACK=YES** was subsequently approved by the sole Human Owner in chat on 2026-10-15 Asia/Ho_Chi_Minh (the message spelled `YÉ`, interpreted as YES). **OWNER_ADOPTED_PROPOSAL_HEAD=57fbcc730cda58470deac392d8ca8dd76310ce7b** identifies the complete seven-module draft approved for adoption. **CANONICAL_IN_MAIN=NO**: these module files are still in Draft PR #16, not part of main. This approval makes the local policy choices owner-accepted for ongoing scoped coordination, subject to GitHub enforcement, while their canonical landing awaits a separate exact-PR merge decision. **NO_MERGE_FROM_ADOPTION** and no M1 activation or production release. When GPT relays the chat decision through the one connected GitHub account, it is **owner instruction relayed by GPT**, not an independently authenticated GitHub-owner review or second human actor.

**REFERENCE_VERSION=assistant-policy-85-v3.0**. Source index: https://github.com/boxs-51/assistant/issues/85; modules https://github.com/boxs-51/assistant/issues/526 through #531, owner-directed status as read on 2026-10-15. These external issues are **reference material**, not direct normative authority of this repository; only the specifically owner-adopted LOCAL clauses govern repository-authored procedure as described here. **NO_AUTOMATIC_INHERITANCE**: later external edits do not change this proposed or future adopted policy without a scoped Avatar_Runtime owner decision. Archived v2.6 / #532 is historical only. Repository-specific terms here supersede no current GitHub protection or security gate.

## Local modules and exact source mapping (one concern per document)

| Owner-adopted local module | Local source | Assistant v3.0 reference | Intended owner |
| --- | --- | --- | --- |
| ARS-M1 — Sole Owner and direct ACK | docs/governance/ARS_M1_SOLO_OWNER_ACK.md | [#526](https://github.com/boxs-51/assistant/issues/526) | Human Owner |
| ARS-M2 — Developer cycle | docs/governance/ARS_M2_DEVELOPER.md | [#527](https://github.com/boxs-51/assistant/issues/527) | GPT Developer |
| ARS-M3 — Final-only audit and exception | docs/governance/ARS_M3_FINAL_AUDIT.md | [#528](https://github.com/boxs-51/assistant/issues/528) | GPT Auditor + Human Owner |
| ARS-M4 — Real stacked PRs | docs/governance/ARS_M4_STACKED_PRS.md | [#529](https://github.com/boxs-51/assistant/issues/529) | GPT Developer |
| ARS-M5 — One-PR merge / history | docs/governance/ARS_M5_MERGE_HISTORY.md | [#530](https://github.com/boxs-51/assistant/issues/530) | Human Owner |
| ARS-M6 — Integration Wave choice | docs/governance/ARS_M6_WAVE_STATE.md | [#531](https://github.com/boxs-51/assistant/issues/531) | Human Owner |

The separately owned existing Draft PR #11, at PR11_PARENT_SHA=419a94bd567986ab5edd8ae5f4f11e6fae8ee4fb, introduces docs/GOVERNANCE_ARS_SOLO_OWNER_001.md as the **existing owner-approved procedural exception proposal**; ARS-M1/ARS-M3 refer to it, never silently replace it. This Draft PR is a child of #11 and contains only governance policies, tests and CI; the policy is owner-adopted but not landed in main. Maintain topic-specific issue(s) and module files individually after canonical landing, rather than treating Issue #6 or one omnibus issue as an operational review queue. A separate decision is needed for any additional issue creation, changes to the adopted modules or merge.

## Current owner decisions and hard safety boundaries

- OWNER_G00_FREEZE_SHA=fe3e680a44ac4e71ef57dc177e73ae31198fcbd0. G00 **DESIGN FREEZE OWNER YES** is already documented for Draft PR #14, scoped to that precise engineering source. This governance adoption does not change its bits, reopen a resolved design choice, certify production GPU or security behavior, or authorize an M1 source claim.
- PR15_MAIN_INTEGRATION=MERGE_HOLD. Draft PR #15 at 7cf306dd783edd7e155ac26517c4b99b8b449552 is only a main-target pre-merge candidate. “IW” appearing in historical PR labels does not equal an accepted Wave or a merge decision.
- **INDEPENDENCE_NOT_AVAILABLE** for the one-account situation. GPT-written GitHub comments/commits are not a second qualified reviewer; no fake APPROVED or independent PASS. The procedural gap is owner-accepted under the existing ARS-SOLO-OWNER-001 exception, while substantive review and all applicable CI/security obligations persist.
- **MERGE_NOT_AUTHORIZED** for PR #11, this proposal, PR #15 and all other Draft PRs. **M1_NOT_AUTHORIZED**; no native renderer/runtime/GPU/87 production security cases can be claimed complete from G00 design tests.
- **GITHUB_PROTECTION_VERIFICATION=HOLD**: existing access to the private repo's main protection endpoint returned HTTP 403; likewise rulesets may require plan/access changes. Never interpret 403 as “no branch rules”; authorized owner must inspect actual GitHub Settings before a merge.
- LOCAL_WAVE_STATE=NOT_ADOPTED: this is the **adopted local rule** for Wave non-authorization; assistant #531's indefinite suspension remains applicable **there only**, not automatically here. To activate, prohibit, or design an Avatar Runtime Integration Wave beyond this non-authorization rule requires a separate T2 owner directive. No automated Wave planning or batch merge.
- GitHub platform protections and law are superior to any repo-authored workflow. Latest exact-scope Human Owner choices govern eligible repo-authored process as locally adopted, with stricter issue-local technical/security gates still enforced. On conflict or ambiguous permission: HOLD rather than infer.

## Owner-adopted development lifecycle (no merge authorized)

GPT Developer: design, bounded source work on scoped branches, self-checks, real negative/regression tests and CI, repair → one stable exact-HEAD handoff → GPT Auditor: final comprehensive technical-method review with disclosed reviewer limitations → Human Owner: reserved YES/REVISE/HOLD → when authorized, one exact PR by MERGE_COMMIT (only if GitHub allows), verify landed SHA and rollback → parent-first next PR. No mandatory intermittent audit signoff; no bulk/automatic Wave authority.

## Adoption and rollback gate

**REFERENCE_ACK=YES** authorized the scoped draft; **ARS_POLICY_V0_1_ADOPTION_ACK=YES** accepts the policy text in its reviewed source state, subject to the existing Solo Owner procedure and actual GitHub protections. **CANONICAL_IN_MAIN=NO** until a separately authorized merge. A further **PER_PR_MERGE_ACK** remains mandatory for PR #11 and PR #16 individually, using exact current HEAD, base, method and a fresh readiness review. Do not treat adopting a governance document as an authorization to merge, change protected settings or start M1.

Rollback before canonical landing: leave or close this Draft PR and revert the owner direction only through a new explicit Human Owner instruction; leaving main unchanged alone does not silently cancel the owner's chat approval. After a future permitted merge: revise only affected modules through controlled, individually approved PRs; preserve original commit history and the audit trail.
