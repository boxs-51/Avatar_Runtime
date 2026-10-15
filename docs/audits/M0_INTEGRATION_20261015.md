# M0 consolidation and M1-A scope decision packet

Prepared 2026-10-15, Asia/Bangkok. Same-account technical-method verification;
INDEPENDENCE_NOT_AVAILABLE. Owner requested integration resolution and a defined
M1-A scope in the active conversation. This packet is a reviewable candidate,
not an exact-SHA owner merge attestation or M1 implementation kickoff.

## Exact custody

- Target main: `637bdae7373aecbf50b18834f3118421d921fe3a`.
- Frozen G00 PR14: `fe3e680a44ac4e71ef57dc177e73ae31198fcbd0`.
- Main-target PR15: `7cf306dd783edd7e155ac26517c4b99b8b449552`.
- Governance PR11: `419a94bd567986ab5edd8ae5f4f11e6fae8ee4fb`.
- Governance PR16: `19069dc85a0e07fe58b9ce8aad6cbfd5f0b1b55a` (contains PR11).
- Local maintenance: `3d72b9c2cfaa77c3c0495143780f9a947b4c39c8`.
- Candidate: [PR17](https://github.com/boxs-51/Avatar_Runtime/pull/17),
  `integration/m0-consolidated-20261015`, based directly on current main.

PR17 preserves all these commit objects. Governance is composed by a normal
local merge, not squash/rebase; original PR heads and main are unchanged.
The source checker requires their ancestry and exact ten governance file blobs,
three maintenance document/config blobs, the immutable 9+8 design blobs, and an
exact 19-path delta from frozen PR14. Existing PR15's two-file guard is unchanged.

## Historical PR disposition

| PR | Source disposition in PR17 | Action after approved integration |
| --- | --- | --- |
| 1 | Exact head is an ancestor | GitHub may mark merged by ancestry; otherwise close as integrated |
| 8, 9, 10, 12, 13, 14 | Exact composed/owner-reviewed heads preserved as ancestors | Same ancestry-based disposition; preserve branches and RED evidence |
| 11, 16 | Exact governance ancestry and ten source blobs preserved | Superseded by consolidated landing; no claim of separate per-PR merge ACK |
| 15 | Exact main-target candidate preserved as ancestor | Superseded by PR17's broader reviewed integration |
| 2 | Original head not an ancestor; Security R5 adopted via PR9, not blind merge of legacy branch | Close as superseded only after landing; keep source/evidence links |
| 3 | Historical RC1 prep; replaced by PR10 and final G00 composition | Close as superseded only after landing |
| 4 | RED-first findings adopted and extended in final mask/finite/oracle tests | Close as superseded only after landing |
| 5 | G00 design / G07-G08 implementation split adopted through PR9 | Close as superseded only after landing |
| 7 | Finite-tree fix and focused regression adopted/extended in composed schema checks | Close as superseded only after landing |

This table records source and intended disposition. It does not assert the five
divergent heads were merged, or authorize deleting branches. The 87 inventory
cases remain design specifications; actual runtime execution is still HOLD.

## Rules observed live

The repository is public. Ruleset [BV, 25099333](https://github.com/boxs-51/Avatar_Runtime/rules/25099333)
is active for `refs/heads/main` and `refs/heads/master`, has no bypass actors,
and contains deletion and non-fast-forward restrictions. It has no required
review, status-check or linear-history rule. The legacy main/protection endpoint
still returns 403 to the app; its state remains UNKNOWN. Do not claim required
reviews/CI are physically enforced or disable/bypass a protection.

Direct Git fetch/push works with command-local proxy overrides; no global Git
configuration or repository rules were modified.

## Technical acceptance and decisions

Initial six new consolidation tests fail on missing `check_consolidation` before
repair (local RED, exit 1). Tests exercise missing source ancestry, unexpected
runtime paths, missing required scope/evidence, changed main, and governance
blob rewriting. The new branch-specific transition is exact, not `integration/*`.
PR17 CI runs these tests before the original provenance gate.

Acceptance requires fresh PREP Linux GCC/Clang + Windows MSVC, Profile B, F03,
R5, and governance workflows on the exact final PR17 head. Record completed run
IDs and final SHA in the PR description after all checks finish; existing PR15
GREEN is historical and cannot attest PR17. Local verification is supplemental.

The accepted scope for the next increment is in `docs/M1_A_SCOPE_V0.1.md`:
bounded C++20 build/ABI foundation with honest UNSUPPORTED placeholders, real
consumer/export checks, shared/static build-tree linkage, and a separately
reviewed M1 custody transition. No registry, renderer, parser, GPU work or M1
activation is included in PR17.

Before main landing, the owner must review one exact PR17 HEAD/current main,
this source manifest, CI and rollback, and explicitly decide on this consolidated
candidate rather than approving a batch of old PRs. Governance modules retain
their original pending-main snapshot; this packet does not silently amend their
owner-adopted policy text. After a permitted landing, update current-status
records through a separately scoped follow-up and close only superseded drafts.

Rollback after landing: a normal PR reverting the consolidation merge while
preserving all original commits and evidence. No force-push, branch deletion,
fake review, automatic Wave activation or claim of production SDK readiness.
