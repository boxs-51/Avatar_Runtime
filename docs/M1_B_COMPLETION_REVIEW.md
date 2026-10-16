# M1-B completion review and dependency handoff

Reviewed 2026-10-16 against main `7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79`,
tree `28b8452810f57d21b0e478143fc90b2cbbb1302b`.
Owner request: recheck M1-B, continue missing B work, otherwise prepare M1-C.
Classification: SELF_AUDIT / INDEPENDENCE_NOT_AVAILABLE.

## Decision

**M1-B1/B2/B3 approved CPU implementation scope is integrated. Overall M1-B
G07/G08 security acceptance is still HOLD.** There is no approved B4 in the
phase manifest. Do not invent an additional CPU phase merely to rename missing
cross-stage evidence, and do not call the 87 whole scenarios complete.

The next useful dependency is M1-C validated CPU asset ingestion. Preparing it
does not waive B's remaining obligations. Return to B integration after actual
validated models/characters exist; complete motion/adapter/mode and physical GPU
scenarios when their respective implementations exist. No public activation is
part of this handoff.

## Scope and evidence reconciliation

| Approved increment | Landed implementation | Result and limits |
| --- | --- | --- |
| B1: typed handles, UID/generation nonreuse, CPU lifecycle, pins/finalizer, bounded allocator | PR21, merge `535663960c33c0ebe49481a8681b8625c336fafe` | Real registry/runtime components and public CPU lifecycle; no public assets/characters |
| B2: OS clock/CSPRNG, ledger, grants, ingress/queue/quota, terminal epochs, writer-priority close | PR23, merge `c76958e8398e5eefde9b77d095a320cda4aac51a` | Private runtime-owned CPU authority; final reviewed source `0b28d0c1db1bebb78f0715402b2b2bd0770a8d35` |
| B3: transitive provenance, retained-state tombstones, final authorization, immutable pose/state lifetime | PR24, merge `7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79` | Final source `c094a7aa1bdf026ce5943ddb6846e1a82f4d9565`; 21/21 exact-source checks successful |
| Two owner-deferred PR23 P2s | Included in PR24 | Runtime-specific callback exclusion and simulation timestamp regression rejection implemented |
| PR24 automated P1 and input-loss self-audit | Included in final PR24 source | Atomic callback/mutator admission, root-safe guard cleanup, transactional queue-head consumption; resolved P1 thread `PRRT_kwDOVHoZQc6s_Lfh` |

References: historical planning snapshots PR20 @
`2d37c28a03f90de551b82b66c94153554b442bf5` and PR22 @
`c56a9cd2d04dbab08aa5cc4eedf04ab4bd7efdf5`; landed B1/B2/B3 audit reports;
`tests/contracts/check_m0_rc1_pins.py`; `audits/M1_B_CASE_EXECUTION.json`.
Planning PR18/20/22 remain historical unmerged proposals, not missing production
predecessors. No permission to merge or close them is implied here.

PR24's final matrix: B3 nine workers, foundation seven, PREP three, ABI/R5 two.
Actual Linux GCC/Clang and Windows MSVC shared/static, ASAN/UBSAN and separate
TSan logs are attested in PR24. Nine B3 race families each run 1000 iterations;
another 1000 cuts exercise mutator-before-callback admission. Six native mutants
were killed. Historical aggregate TSan timeout remains a failed historical gate;
the final split-family source passed all workers. This review inherits those
results only because the landed tree exactly equals the tested source tree.
No Actions checks run on the merge SHA itself (the workflows use pull_request).

## Remaining B obligations: preserve the original scenarios

The immutable inventory contains 19 HDL, 45 SRC and 23 RACE records. All 87
records, platforms and oracles remain unchanged in the execution ledger;
whole-case PASS=0, HOLD=87. Component tests do not promote any whole-case result.

| Dependency to finish an original scenario | Representative original IDs | Required follow-up |
| --- | --- | --- |
| Validated model, asset ownership and actual character integration | HDL-001/002/003/006, SRC-006/016/021/031 | M1-C first; then separately scoped model/character lifecycle and source binding through the frozen ABI. Exercise stale/type/foreign IDs and model references, without a dummy public loader |
| Public source/update path and full motion state | RACE-A/B/F/H/I/V, SRC-008/011/012/023/025/043 | Real normalized motion, smoothing/retarget/cache integration; replay original scenario, not only the scalar B3 harness |
| Host serialized adapter and remote error boundary | SRC-002/003/007/015/031 | Validate actual hostile serialized claims/lengths; no existence leak and no authority from packet labels. Runtime performs no implicit networking |
| Complete mode/trigger effects | SRC-018/024/043 | Implement separately reviewed mode and deferred animation behavior, then verify invalidation through all future effects |
| GPU pins and safe completion | Explicit GPU markers HDL-009/015/019, RACE-D/J; also display/render portions of HDL-006/018 and SRC-032 | M1-D and real Windows D3D11 plus later rendering integration. CPU snapshot retirement is not GPU completion |
| Full platform/evidence closure | All 87 | Run each complete original scenario with its required platforms, exact source, raw log, status/oracle, sanitizer where relevant and reviewer disposition |

IDs in the dependency table are examples, not an exhaustive partition or an
allowlist for dropping dependencies on other records. Even CPU-only original
cases retain their whole-case HOLD until individually executed and recorded as
their original scenarios. The generic remaining-evidence text in the current
ledger is conservative; it is not proof that every record needs every dependency.

No source P0/P1 remains identified in the accepted private CPU scope after the
PR24 repairs. This is a scoped audit conclusion, not certification that no other
defect exists. Public model/character/source/update/render exports still return
UNSUPPORTED; capability bits remain zero. G02, full G07/G08, GPU and SDK release
are not PASS.

## Verification performed in this review

Fresh GitHub reads confirm main equals the PR24 merge and no open production
implementation PR remains; open PR18/20/22 contain historical preparation only.
Source/tree/parents were read back. At the unchanged landed baseline:

- `python -m unittest discover -s tests/contracts -p 'test_*.py'`: 113 tests OK,
  exit 0. Initial sandbox attempt had one Windows temporary-directory permission
  error, not a failing product assertion; a rerun with temp access passed all 113.
- `python -m unittest discover -s tests/governance -p 'test_*.py'`: 9 tests OK,
  exit 0.
- `cmake --build build/b2-local-static --config Debug --parallel 2`: exit 0,
  rebuilt current B3 production components with local MSVC 18.8.2.
- `ctest --test-dir build/b2-local-static -C Debug --output-on-failure`:
  25/25 passed, exit 0, 60.88 seconds. This reused directory is now rebuilt from
  current B3 source; its historical directory name is not a B2-only result.
  Raw log: `build/integration-evidence/b-completion-native.log`.

These are local Windows static results, not a fresh Linux/sanitizer matrix.
Final PR24 exact-source CI supplies the separately identified matrix evidence.
Documentation-proposal validation is recorded in the handoff PR. No frozen
schema/header/inventory/design blob,
production source, historical test or workflow is changed by this proposal.

## Continuation

1. Prepare M1-C CPU validation dependency decisions and exact staged manifests.
2. Complete C's dependency/corpus gate before authorizing parser implementation.
3. After real validated CPU assets exist, propose the exact B public-integration
   scope and execute the newly runnable original cases. Keep the other HOLDs.
4. Close overall B only after all original obligations actually pass. Production
   activation, implementation merge and release retain separate owner decisions.
