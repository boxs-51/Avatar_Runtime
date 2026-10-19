# Alpha #53: historical and canonical-path custody

2026-10-20, Asia/Bangkok. **Scoped BUGFIX PASS** against
[issue53](https://github.com/boxs-51/Avatar_Runtime/issues/53), base
`4b05828d402003d560602718eec7d8f9f7d63d48`. Current order:
[linear backlog](../../in-progress/ALPHA_LINEAR_BACKLOG.md).

## Correction and preserved oracles

The historical M2-E check validates original PR44 candidate
`59f4a08006733c8e4fe61c4f899d77c150656b1c` plus ancestry. Its exact10-path checker,
moved-parent and injected public-path negatives remain intact. No historical
allowlist expands to accept later development.

Nine explicit code-owned legacy/canonical document aliases locate the same
original Git blobs after relocation. Missing, divergent duplicate or wrong blobs
fail; historical layouts still work. Original9 predecessor and8 reviewed G00
keys/expected SHA values remain unchanged. An editable catalog or path map cannot
substitute identity or waive a missing key.

A separate alpha checker verifies frozen31ce916's nine merge parent/source/order
records, exact PR44 delta from ec30b6c, current ancestry, five protected ABI/HOLD
blobs and all17 normative identities. This is integration custody, not permission
for arbitrary descendant edits.

CI's old dispatcher rejected new alpha issue branches as an old RC1 parent
mismatch. PR events with base exactly alpha now use registered branch/base/exact-
path scopes for issues52/53. Unknown routes, moved bases and extra paths reject.
The candidate source revision supplies protected blobs; legacy routes and common
checks continue without an early return or wildcard exception.

## Actual evidence and independent review

| Requirement | Observed evidence | Result |
| --- | --- | --- |
| Reproduce at base4b05828 | G00:6 tests/3 errors, exit1; M2-E:3 tests/1 failure, exit1 | RED |
| Identity and historical negative controls | G009/9, M2-E3/3; missing/divergent/both-wrong aliases, canonical-key substitution and legacy revision | PASS |
| Alpha integration/PR dispatch |15/15; order/parent/path, ABI/HOLD corruption, unknown branch/base, candidate-specific mutation, real52 source and actual legacy M1-A | PASS |
| Full local regression |178/178 contracts, exit0; complete direct checker exit0 | PASS |
| Independent process | Separate reviewer ran G009/9, M2-E3/3, alpha15/15 once, inspected exact diff/full178 log; no supported P0-P2 | PASS |

Logs: `build/alpha53-red-g00.log`, `build/alpha53-red-m2e.log`,
`build/alpha53-final-contracts.log`, `build/alpha53-direct-check.log`,
`build/alpha53-review-{g00,m2e,alpha}.log`. Commands: `python -B -m unittest
discover -s tests/contracts -p 'test_*.py'` and targeted patterns;
`python -B tests/contracts/check_m0_rc1_pins.py`.

The issue53 positive dispatcher scope fixture is explicitly synthetic. Before
push, the actual committed candidate must pass
`check_alpha_pull_request(base, committed_SHA, 'fix/alpha-53-historical-custody')`
with its nine real changed paths. Its receipt is recorded in the resulting PR
and issue; the fixture never substitutes for committed-source proof.

Architecture/project structure: **SUPPORTED**. Optimization: **NO_RECOMMENDATION**.
Agent review is not actor-distinct GitHub owner approval. No C++, CMake, workflow,
public ABI, inventory, resource cap or frozen manifest file changed. No new
native/physical/remote-CI green claim is made.

## Reconciliation and completion

PDRM: current backlog/catalog reflect the correction; historical manifests,
reports and relocation records remain untouched. Historical exact scopes remain
distinct from current integration. G1 goal fidelity, G2 artifacts, G3 actual
RED/GREEN, G4 constraints, G5 revision/commands, G6 independent review and G7
scoped closure PASS. Actual committed-scope and alpha integration receipts are
the remaining delivery gates above. Full product remains UNVERIFIED; #54..#57
render bugs and session/SDK/qualification/physical HOLDs remain open. Merge or
green tests cannot close those gates.
