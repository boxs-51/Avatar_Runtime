# Alpha42 stage4: sustained ingress during close

2026-10-23, Asia/Bangkok. Bounded continuation from35e9734 on the unmerged
PR80/PR79 stack. Local Windows evidence is primary; Actions remain temporarily
excluded under the owner's quota instruction. No shipping runtime/ABI change,
merge, public activation, private assets or device reset is performed.

## Contract and oracle disposition

A42-1..6 remain in force. The reference87-case inventory and security regression
gate are unchanged. This stage adds one canonical local observation, SRC041:
sustained ingress while public begin_close sets writer intent must not starve
close, and later queue admissions must be denied. Existing37 observations remain
registered. SRC014's pressure diagnostic is deliberately separate: test-owned
totals cannot satisfy the original requirement that drops be visible in runtime
counters. No current runtime counter API/storage was found in public ABI,
source authority or ingress queue; that part remains UNVERIFIED, with SRC014
NOT_RUN in the canonical receipt. No counter API is invented by this test.

| Requirement | Observation |
| --- | --- |
| A42-1/5 | All87 IDs/platform/oracles retained;38 local observations,49 NOT_RUN; all complete-case qualifications HOLD |
| A42-2 extension | Public submit and begin_close with real validated model, existing native-observer tier; no private lifecycle mutation |
| A42-3 | Source/input/binary/log custody, exact flood accounting, queue/authority state, bounded drain and allocator-zero final close |
| A42-4 | Separate pressure diagnostic rejected as canonical SRC014 evidence by verifier negative test |
| A42-6 | Separate read-only review, focused custody and current-document reconciliation; no merge/activation |

## Sustained close schedule

Eight ingress workers alternate all16 sources. Every worker completes at least
100 attempts while continuing to submit; fixed security time and rate240 leave
all queues saturated at16 frames before close. An atomic clock gate then parks
one admitted worker at the actual security-clock cut; the remaining readers
block behind that clock, and the observer waits for eight active admissions.
The H owner calls public begin_close. A coordinator waits for actual admission
writer intent, then submits100 rounds across16 capabilities: all1600 calls must
return CLOSED before releasing the paused reader. Previously admitted calls
finish with QUEUE_FULL; each original worker continues until its CLOSED result.
Close must return within the bounded10-second gate, active admissions become0,
and every source is terminal closed with its queue cleared. A later public
update and submit remain CLOSED; real finish_close verifies allocator-zero.

No sleeps guess the writer-intent cut. Coordinator reads only admission state
under its lock and issues public submits; source/queue observations occur after
close and all thread joins. Queue storage addresses are captured while their
indices are valid and remain pinned by the runtime lease. Source.data is a
terminal sentinel after close and is asserted separately, never indexed.

Final Release SRC041 observed accepted256, QUEUE_FULL5750, later CLOSED1600,
with eight admitted readers drained, queues0/admissions0 and allocator-zero.
The rejection count is scheduling-dependent; the test asserts exact per-worker
accepted32, at least68 QUEUE_FULL and one CLOSED, rather than requiring5750.

## Pressure diagnostic, not a second canonical case

DIAG-PRESSURE registers16 sources at one frame/second and submits100 attempts
per source for each of eight controlled one-second refill rounds. Every round
accepts exactly one/source; subsequent99 return RATE_LIMIT without extra quota,
sequence or ordinal charge. Queue size remains1, then public update drains it.
Expected totals are accepted128 and RATE_LIMIT12672, with authorized nonneutral
pose after each update. A second real runtime fills all16 queues at rate240:
accepted256, QUEUE_FULL1344, queue16, sequence/ordinal16, credit224 units; public
update drains one per source. Both runtimes close with exact allocator-zero.
These are harness-owned counters; SRC014 stays NOT_RUN, not silently PASS/N/A.

## Preflight review correction

The initial preflight commands exited0, but independent review identified a P2
out-of-bounds observer read after terminal() replaced source.data with the
max_sources sentinel. Those preflight results are not final acceptance. The
test now captures fixed queue addresses before close, checks sentinel separately
and reads storage only after all joins while the runtime remains pinned.
Separate reviewer confirmed this correction and scoped source review PASS.
There was no production defect or production repair in this stage. A green
process exit alone was insufficient to accept the original unsafe observer.

## Exact-source evidence

Primary receipt:
`E:/Avatar_Runtime/build/alpha42-stage4-evidence/4f11d1fd76e74a07998881b0b7b06081/evidence.json`.
Result PARTIAL_LOCAL_PASS:41 commands exit0,164 source hashes unchanged,
38 canonical observations (8 linked ABI +30 native observer),49 NOT_RUN.
All four executable/worker and fixture pins remain stable throughout execution.

| Evidence | SHA256 |
| --- | --- |
| Primary38-case receipt | `78925ed740454d75e6d713c1e1c3a48d494cef8a46e388038fa1ef6efabc594c` |
| Sorted compact source-map JSON | `48c58d0b4aed473809c2b21623e39f92553705552bdb2cc0f006fa0cd696f76d` |
| Release native observer | `48d9de281b297414e654ff08566e20c63398e7e2a262fd777fadf12e71b7f475` |
| SRC041 raw040.log | `9a014efd49741a57b2423cb6f1700d1245c7289a873a2e8f702da8b33f6c378c` |

Supplemental receipt:
`E:/Avatar_Runtime/build/alpha42-stage4-supplemental/evidence.json`, SHA256
`5b7bd5239b00d3a20e2f466f3def3c88ae349b150dfb060a3e7a9ecce4d1f320`.
All three commands exit0. The same164 primary source hashes are checked before
each command and after the final command; executable pins and raw logs retained.

| Final-source execution | Result |
| --- | --- |
| Static Debug canonical38 plus3 extra diagnostics | PASS41/41,12.68 seconds |
| Static Release existing CPU/security/resource plus3 diagnostics | PASS26/26,49.00 seconds |

Debug and Release pressure diagnostics are directly executed here; they are not
included among38 canonical receipt records. Extra diagnostics are model/admin
retirement and pressure, not three additional canonical cases. Production source
and public headers are byte-for-byte unchanged from35e9734; no fresh WARP or
installed-SDK acceptance is claimed for this test-only stage.

Full Python contracts PASS190,137.408 seconds,exit0. Raw final log:
`E:/Avatar_Runtime/build/alpha42-stage4-contracts-elevated.log`, SHA256
`290627baae692c82e982b9103b3a7604e034a4d6f758523a81fd60fb03885c52`.
The first sandboxed run executed190 tests but failed with four Windows temp-path
access/cleanup errors (WinError5); its distinct log is retained at
`E:/Avatar_Runtime/build/alpha42-stage4-contracts.log`, SHA256
`f40c2891bde961392800202560c35878d49701991ef95382fb31e37ad91b94d7`.
The successful rerun used the same test assertions with native execution
permissions; the failed attempt is not counted PASS or called a runtime RED.
The new negative oracle rejects both DIAG-PRESSURE and SRC014 as eligible
canonical evidence. No original security oracle, assertion or fixture changed.

Scoped documentation checks reconcile176 catalog entries (completed54,
reference86,in_progress32,obsolete4), all four changed document hashes and242
relative links. Historical stage1/2/3 reports are unchanged. Cumulative PR scope
is15 exact paths; this stage touches10. Runtime/header/frozen-inventory bytes
are unchanged from35e9734. Commit/PR custody is recorded after publication.

## Remaining work and scope

Remaining49 canonical executions:12HDL,23RACE,14SRC. Stage3's grouped entry
criteria remain applicable except SRC041 now has this local observation.
SRC014 specifically requires production drop-counter design/implementation and
direct observation before qualification; the pressure diagnostic proves only
the stated quota/queue behaviors. GPU lifetime, callback/finalizer/copy cuts,
full named RACE schedules, actual remote boundary and smoothing/trigger lineage
remain separate work. Required Windows2022/Ubuntu GCC/Clang and sanitizers stay
NOT_RUN. All87 complete-case qualifications, G07/G08 and #42 remain HOLD/OPEN.
Issue41 gaps, #40 physical loss, #61 integrated qualification and #62 release
remain separate. Historical stage1/2/3 receipts retain their original source;
this stage does not requalify installed SDK, GPU fault paths or fuzz campaigns.

Catalog classification completed refers only to this bounded experiment, not
whole issue, platform, milestone or shipping acceptance. Raw evidence is retained
locally in ignored build storage; it is not a GitHub-hosted artifact.

Independent reviewer `/root/f2_independent_review` performed separate read-only
source and final evidence/document reconciliation without edits or native
workloads. It confirmed the P2 observer correction, primary/supplemental/current
164source equality,41+3 command/log receipts and binary/worker pins,38/49 case
disposition, exact15-path scope and176 catalog entries. It observed the final
190-test OK log; the executing parent independently recorded process exit0.
Architecture and project structure remain an appropriate test-only extension
using existing friend seams; no new supported P0-P2 findings. No optimization
recommendation is needed. Scoped stage4 task PASS; overall #42 UNVERIFIED,
G07/G08 HOLD. Review is not independent native execution or merge approval.
