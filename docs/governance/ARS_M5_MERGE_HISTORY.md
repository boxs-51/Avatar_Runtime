# ARS-M5 — Single-PR merge approval and immutable history custody

**ADOPTION_STATE=PROPOSED_NOT_CANONICAL** — OWNER REFERENCE YES ONLY; no merge/policy activation.

**Source:** https://github.com/boxs-51/assistant/issues/530 — M5 of Policy #85 v3.0.

**ONE_PR_ONE_ACK:** every source or governance PR needs its own Human Owner T2 MERGE YES naming exact PR, full HEAD, current target/base, approved changed paths and **merge method**. The default suggested method is **MERGE_COMMIT** (a normal non-fast-forward merge) to retain commit objects/SHAs, author attribution, ancestry and stack evidence, but only if GitHub permits it. **NO_SQUASH_BY_DEFAULT**; no routine squash, UI rebase/merge or force-push that discards verified commit identities. Exceptions require a separate precise owner YES disclosing SHA-history impact.

## Release proposal checklist

Before any merge: read the *current* main and candidate HEAD and synthetic merge ancestry; verify file ownership, active P0/P1, every required exact-head Linux/Windows/graphics security check, final technical audit evidence, unmerged predecessors, approved risk and rollback, current protected-branch settings and required reviews. Actual Github branch protection may require linear history or a second reviewer, making merge commits incompatible; return HOLD_CONFIG_DECISION, never bypass. A REST 403 is an inability to verify, not a status of “unprotected.”

After Human Owner approval, if and only if GitHub permits, integrate **one PR**, read back landed merge/main SHA, record source PR/head, method, reviewer class, owner ACK, CI evidence and rollback plan. Update dependent children parent-first before any further requests. Automated Wave or batch merges are outside this route and **MERGE_NOT_AUTHORIZED** in all current Avatar Drafts.

G00 design-freeze YES only authorizes that design's acceptance; it is not a PR merge decision. Production activation, M1 source scope, packaging and SDK release have separate safety gates.
