# PUB-04-A independent review

2026-10-19, Asia/Bangkok. Separate read-only reviewer agent
`/root/f2_independent_review`, delta from `9a9ccd6` against
[PUBA-01..04 C0](../../in-progress/M2_MOC3_PUB04_A_PUBLICATION_DESIGN.md).
The [source closure](../../reference/audits/M2_MOC3_PUB04_A_SOURCE.json) pins the
reviewed/tested worktree; reports are subsequent handoff artifacts.

**Final scoped verdict: PASS. No open supported P0/P1/P2 findings.**

## P2: malformed transport with a pending GPU query

Initial diagnostic-loop `return 2` branches could unwind the publication owner
while a query was pending. Its one-attempt destructor close could terminate on
BUSY instead of using the deliberate non-unwinding quarantine path. Initial
subject verdict: REVISE.

All malformed live-loop paths now use explicit process exit; a fixed-size line
reader also bounds diagnostic transport allocation. The generated regression
prepares/submits a candidate, proves prior state/pixels and pending status, sends
a malformed BEGIN, observes exit2 and verifies old image bytes. Disposition:
**FIXED_WITH_PROOF**. Unknown completion is never labelled completed/device-lost.

## Independent observations

Reviewer inspected whole-batch validation before GPU, request identities/lease,
actual core prepare, owner-bound final fence and core-to-cursor-to-target order,
close/poll alternate paths, failed prepared-session termination, diagnostic pipe
and verifier oracles. No additional supported P0-P2 findings.

Reviewer independently executed generated publication tests **7/7 PASS exit0**,
the owned-fence executable PASS and actual CoreConsumer executable PASS. The first
restricted generated run failed on readback/temp-folder access and teardown
WinError5. One elevated retry passed without source, assertion or limit changes;
the initial environment failure is not reported as a passing execution.

Reviewer reconciled final publication `960a1f344fdc4b909e92f7e0` and physics
regression `9e757e2177cf472089dcc362` native/API/UI evidence. Current selected
source/binary/core/security/motion/dependency pins match unchanged parent/child
captures. Both mapped backends presented/observed all eight accepted identities;
image/sample checks,401 rejection and descriptor removal agreed. The integrated
CTest log records63/63 PASS after all registered executables were built.

The first contended live run remains FAIL for expired request lease. Keeping that
rejection and qualifying a later idle controlled run under the same limits is
correct; no gate relaxation was found.

Architecture assessment: **SUPPORTED**. Project structure assessment:
**SUPPORTED**. Optimization proposals: **NO_RECOMMENDATION**.
Full PUB-04/PUB-02, live worker state ACK, long-lived control and activation remain
HOLD. This is a bounded reviewed stage, not release/merge authority.
