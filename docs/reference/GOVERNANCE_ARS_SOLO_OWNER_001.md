# ARS-SOLO-OWNER-001 — single-account governance exception

**Status:** OWNER-APPROVED PROCEDURAL EXCEPTION / TECHNICAL G00 REVIEW STILL HOLD
**Effective:** 2026-10-14 (owner's explicit YES instruction in project conversation)
**Repository:** `boxs-51/Avatar_Runtime`
**Decision owner:** sole repository owner/maintainer; no claim of separate GitHub identity.
**Scope:** M0/G00 design review workflow and later PR governance under a single GitHub account. The same account is NOT independent of its code authorship.

## Reason and limits

The repository owner has confirmed that they currently possess only one GitHub account. Requiring a second GitHub login as a *procedural* condition would block design iteration without adding substantive evidence. This exception permits **solo-maintainer technical decision making** supported by reproducible independent-*method* RED-first review, exact source/ancestry, pinned artifacts, negative test results, and explicit owner decisions. It must never masquerade as an actor-distinct GitHub review, manufacture an APPROVED submission, or bypass GitHub branch protections/rulesets.

This exception **does not** waive source gates, mandatory CI, threat analysis, real security/asset/GPU runtime tests, source path custody, P0/P1 blockers, regression coverage, or operator/release safety checks. A design-only PASS is not an implemented SDK PASS. It does not confer merge permission.

## Required acceptance ledger for each candidate

1. Pin `main`, each predecessor HEAD, changed-path manifest, merge-base and exact composed candidate SHA.
2. Record findings with severity (P0/P1/P2), reproduction instructions, raw pre-fix RED evidence, post-fix raw Linux/Windows CI and closeout reasoning. Negative oracles may not be weakened to obtain GREEN.
3. State a **same-account / no actor-distinct review** disclosure prominently. Separate design review from production, physical GPU and security validation. Do not use a GitHub `APPROVE` action to pretend to be a second reviewer.
4. For material architecture/security/public ABI changes obtain the owner's explicit YES/REVISE/HOLD, identifying the exact affected source and rollback. This MAY be recorded in the project's tracker as *owner attestation*; if given only in chat it is not cryptographic GitHub sign-off.
5. Where prior guidelines call for an actor-distinct reviewer, mark `EXCEPTION_ACCEPTED_BY_OWNER` only for the identity/process criterion and `INDEPENDENCE_NOT_AVAILABLE` in the audit. Preserve the substantive technical gate; do not label it `INDEPENDENT_FINAL_PASS`.
6. Verify GitHub branch-protection and ruleset compatibility before merge. Never disable protection, forge identities, or bypass required checks. If GitHub requires a second actor, the merge remains blocked pending a lawful policy/ruleset decision.
7. Run all impacted workflows at one **exact final source SHA**. Changes after GREEN invalidate that evidence.
8. **Merge is separately authorized**, with a fresh target-main diff/provenance and owner MERGE YES. Do not self-approve or merge based on this procedure alone.

## M0 owner decisions (2026-10-14)

The owner approved native 2D Profile B explicit `layer_bindings` and PR #8 adoption; Security R5/G00-P0-C `87 DESIGN` case inventory at G00 while actual 87 runtime, sanitizer and OS tests remain G07/G08 HOLD; ABI v0.1 20 symbols, optional diagnostic #21 deferred; D3D11 HOST_DEVICE + QUERY_EVENT. These are scope and design decisions, not independent source PASS or production feature certification.

Preliminary stacked source: PR #8 → PR #9 → PR #10; G00 composed review must pin its own final SHA. Exact final audit and main-targeted CI are still mandatory. No PR merge authorized by this file.

## Required status vocabulary

- `OWNER_DESIGN_YES`: owner accepts a specified design at a declared scope.
- `TECHNICAL_METHOD_PASS`: tests/findings executed by the same assistant/account with reproducible evidence.
- `INDEPENDENCE_NOT_AVAILABLE`: no distinct human/account reviewer occurred.
- `G00_HOLD`: at least one substantive design gate or evidence requirement remains.
- `MERGE_NOT_AUTHORIZED`: no separate explicit owner MERGE approval on the exact candidate.

**Rollback:** abandon or revert this governance PR; original project branch protections, main and source remain unchanged.
