# PUB-02-A bounded retained sequence: local acceptance

2026-10-19, Asia/Bangkok. **PASS for RET-01..04 only.** Full PUB-02/PUB-04 and
whole F4/public release remain **HOLD**. Physics3 is **VALIDATED_NOT_SIMULATED**.
The [frozen contract](../../in-progress/M2_MOC3_PUB02_A_RETAINED_SESSION_DESIGN.md),
[exact tested source closure](../../reference/audits/M2_MOC3_PUB02_A_SOURCE.json)
and [independent review](M2_MOC3_PUB02_A_REVIEW.md) define this result.

## Delivered behavior

One AppContainer worker decodes a package once, retains one evaluator and applies
1..8 complete parameter frames. Omitted bindings reset to defaults. Every command
is checked and bound against the native catalog before any frame update/output.
Sequence, clock, binding count, range and finite-value limits are explicit.

The new opt-in `--evaluate` mode emits a bounded versioned batch. The independent
host checks the entire outer sequence and every inner drawable/atlas packet before
decoded scene allocation or GPU creation. Failed decoding leaves the previous
verified sequence unchanged. The packet-only render host selects a record only
after complete validation. Static v1 remains distinct and rejects sequence/v2 data.

No public ABI/capability change, new dependency, driver reset, merge or activation.
Worker limits remain 512 MiB, two CPU seconds and five wall seconds; total output
remains 192 MiB. This is a short retained batch, not a persistent public service.

## Direct local evidence

Windows x64 MSVC Release, static worker CRT, authorized local fixture. Private
model data, geometry, image hashes and measurements stay ignored under `build/`.
The test explicitly creates a 1024-edge derivative atlas fixture; original MOC
and Physics3 bytes are retained. This does not qualify original full-resolution
inputs or universal model compatibility, and the broker never silently resizes.

| Requirement/check | Observation | Result |
| --- | --- | --- |
| RET-01 retained complete overrides | Actual isolated evaluator: default, normalized drive, omitted/released; deterministic fresh-session repeat | PASS |
| RET-02 native preflight | Replay/backwards clock/nonfinite drive/unknown catalog slot/trailing commands reject with no candidate bytes; job empty and profile removed | PASS |
| RET-03 all-record boundary | Native generated corpus: 694 rejection checks and previous-sequence retention; actual late malformed sequence/clock/generation/NaN and cancelled nonce reject before any GPU calls | PASS |
| RET-04 geometry | XY-only packet witness: default differs from driven and exactly equals release; appearance/UV/atlas-only mutations cannot satisfy this oracle | PASS |
| RET-04 pixels | WARP and hardware: exact same-backend trusted-local pixel comparison for all three poses; driven differs, release equals default | PASS |
| RET-04 visible/API | Separate authenticated API client observes sequences 1,2,3 on both backends; mapped Tk image sample checks agree, unauthorized call returns 401, descriptor removed | PASS |
| Static v1 regression | Actual static admission, malformed output/GPU rejection, WARP/hardware, mapped window/API and unchanged execution pins | PASS |
| OS regression | Actual file/process/environment/limited-loopback, memory/CPU/wall/crash/flood containment and cleanup | PASS |
| Build/regression | `cmake --build build/moc3-public-isolation --config Release`; `ctest --test-dir build/moc3-public-isolation -C Release --output-on-failure`: 28/28 | PASS |
| Frontend/oracle | `python tests/moc3/test_retained_session.py`: 5/5; static packet379 and Physics3 admission18 also independently passed | PASS |

Actual commands and ignored evidence:

* `python scripts/moc3_verify_admission_observer.py --retained`:
  `build/moc3-package-evidence/ff54dbf09fda42c9b25a3c65/`.
* `python scripts/moc3_verify_admission_observer.py`:
  `build/moc3-package-evidence/6add4159f2d84787af4d2c13/`.
* `python scripts/moc3_verify_public_preflight.py`:
  `build/f4-public-preflight-evidence/42775af44ce14aa2840a7879/`.
* Build log `build/pub02-final-build.log`; CTest detailed log
  `build/moc3-public-isolation/Testing/Temporary/LastTest.log`.

Both retained and static acceptance captures pin code, binaries and dependencies
before and after execution; the separate parent client pins before child creation
and after child exit. All four captures agree with the exported source closure.
Local evidence is primary; GitHub CI is corroboration and was not rerun remotely.

## Failed/ineligible observations preserved

Run `4d80145bb4584c6081ed99d2` predates the geometry fix and cannot close RET-04.
Strengthened run `142117afe12f42348d5d5323` correctly failed because the first
catalog entry changed appearance without XY movement. The final fixture selects
a conventional motion binding but still requires measured XY change and release.
Static run `19f0ddef42794dc8ac4a86a4` was invalidated by a concurrent build changing
a pinned test binary. It is not acceptance evidence; the final static run occurred
after the build completed with all pinned files frozen.

DOC-MOVE separately preserved the existing contract-suite baseline: 155/157 PASS,
two preexisting historical M2-D scope/stack drift failures (`test_exact_scope` and
`test_complete_stack_against_merged_m2_b`). They were not weakened or claimed green.
The [relocation record](../../DOCUMENTATION_PATHS.json) records before/after proof.

## Completion assessment

Contract: user-requested document separation followed by implementation; DOC-MOVE
and the frozen RET-01..04 slice. Deliverable kind: REFACTOR + CODE_CHANGE.

| Mandatory task | Direct evidence | State |
| --- | --- | --- |
| DOC-01 physical categories and exhaustive inventory | `completed/`, `reference/`, `obsolete/`, `in-progress/`; catalog/index | VERIFIED |
| DOC-02 custody and navigability | Original121 mapping; JSON/raw bytes preserved; relative link and workflow match checks | VERIFIED |
| DOC-03 active readers | Live scripts/tests/workflows updated; governance9/9 and pin/stage contracts PASS; baseline contract failures preserved | VERIFIED |
| RET-01..04 bounded continuation | Direct local rows above; exact code/binary/dependency closure; independent review | VERIFIED |

Constraints preserved: YES. Required artifacts delivered: YES. Necessary scoped
checks satisfied: YES. Independent review satisfied: YES. Scoped task/goal verdict:
**PASS**. Broader product verdict: **UNVERIFIED**, with explicit remaining HOLDs.
No merge, deployment, public release or physical reset is authorized by this report.

Next implementation: persistent isolated physics/state lifetime plus host
authority/expiry/cancellation and final publication fence. Public ABI/installed SDK,
fuzz/sanitizer/performance/platform qualification, redistributable fixtures and
physical D06/D08 are still outstanding; see the [release ledger](../../in-progress/M2_MOC3_F4_RELEASE_LEDGER.md).
