# ARS-M4 — Genuine stack-first development and parent-first integration

**ADOPTION_STATE=OWNER_ADOPTED_PENDING_MAIN** — Human Owner approved local governance v0.1 in chat; Draft PR not landed, no merge/M1/Wave authorization.

**Source:** https://github.com/boxs-51/assistant/issues/529 — M4 of Policy #85 v3.0.

**MAX_STACK_DEPTH=10** for dependent unmerged source/production PRs. Depth 1 is a PR directly above canonical main; depth N depends on the unmerged previous PR. Depth 11 requires a separate Human Owner T1/T2 scope exception. Ten is a limit, not a target, and unrelated scopes should be independent PRs.

## Traceability and safe re-pinning

Each PR must declare exact parent PR/full SHA, branch/base/full HEAD and merge-base, approved changed paths, rationale for dependency, CI runs, gate status, rollback and impact of parent/source changes. No claim that successful parent review implicitly approves its children. Never override security gates to make a dependent stack GREEN. **FROZEN_HEAD** history is not to be force-rewritten merely to simplify merging; material parent drift means child HOLD and focused re-audit. Re-pinning with transparent ancestry and new CI requires the appropriate owner scope.

**PARENT_FIRST** is the only default integration order. After each authorized parent merge, verify canonical main, update the next dependent PR's comparison and run affected tests before seeking its own exact-PR owner MERGE YES. Bulk autorun, child skipping, accidental duplicated commits and synthetic “all stack now ready” authorization are prohibited.

## Current scope clarification

Governance Draft PR #16 is a real child of Draft PR #11 because its policy index explicitly depends on the existing Solo Owner exception. Engineering PR #15 is a distinct main-target *integration candidate*, not a basis for a Wave. Owner G00 frozen PR #14 and its review ancestors remain unmerged; their source contents must not be silently changed by this governance stack. M4 does not itself authorize any merge.
