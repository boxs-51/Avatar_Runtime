# PUB-04-A: core-authorized packet publication

2026-10-19, Asia/Bangkok. **PUBA-01..04 scoped PASS.** The user's next-goal
continuation after `9a9ccd6` is implemented against the
[frozen contract](../../in-progress/M2_MOC3_PUB04_A_PUBLICATION_DESIGN.md).
[Source closure](../../reference/audits/M2_MOC3_PUB04_A_SOURCE.json) and
[independent review](M2_MOC3_PUB04_A_REVIEW.md) accompany this report.
Full PUB-04/PUB-02 and shipping activation remain **HOLD**.

## Delivered transaction

The private packet publication owner prepares a real core candidate before
receiving worker output. A generic normalized request channel binds sequence,
fresh nonce, selected record/clock and a100..2000 ms monotonic input lease. This
authorizes a trusted request, not model-controlled parameter IDs or worker fidelity.
Only the independent typed batch parser runs in the renderer; MOC/PNG/Physics3
parsing stays in the isolated worker.

The pending D3D11 target owns a nonthrowing publication fence. Exact query
completion precedes the final existing core authorization check. Success then
accepts the physics batch cursor and swaps the active texture/epoch using only
nonthrowing assignments. No-argument polling cannot omit that fence; close drains
and rejects it. Rejection retains the prior core generation/value, target identity,
epoch, accepted cursor and readback. A failed prepared session is terminal; there
is no silent source rotation or authority remint.

The accepted cursor is host bookkeeping for a speculative bounded batch. It does
**not** acknowledge/commit live worker particle state. That protocol and long-lived
sessions remain unfinished. No public C ABI/capability, dependency, activation,
merge, driver reset or resource-cap increase is part of this change.

## Direct acceptance matrix

| Task / requirement | Actual observation | Result |
| --- | --- | --- |
| PUBA-T01 / PUBA-01 | Real core grant exists before actual AppContainer physics admission; selected record nonce/index/clock checked after full batch validation | PASS |
| PUBA-T02 / PUBA-02 | Controlled BUSY keeps prior state; real S_OK query completion then final core fence advances core/image/cursor identities together | PASS |
| PUBA-T02 / PUBA-02 owned fence | Native lifetime test verifies pin retention; no-argument poll invokes rejection fence; close rejects without invoking authorization or advancing epoch | PASS |
| PUBA-T03 / PUBA-03 | Actual isolated crash cancels an already prepared grant; cancellation before/after GPU submission, actual elapsed input-lease expiry before/after submission, invalid batch reload, stale nonce and wrong clock retain prior state and pixels | PASS |
| PUBA-T03 / PUBA-03 late revoke | Trusted query observer calls real source revoke only after actual GetData S_OK; final core publish rejects, old target/core/cursor/pixels remain | PASS |
| PUBA-T03 / PUBA-03 allocation failure | Injected target-create rejection retains prior state and creates no new drawable resources; not a claim of physical GPU exhaustion | PASS |
| PUBA-T03 / PUBA-03 failed session | Later BEGIN is denied; no authority rotation/remint occurs | PASS |
| PUBA-T03 transport quarantine | Malformed BEGIN after STAGE exits2 without unwinding pending GPU objects; prior image bytes preserved | PASS |
| PUBA-T04 / PUBA-04 local pixels | WARP and hardware: eight accepted physics batch frames exactly match the qualified same-backend packet renderer | PASS |
| PUBA-T04 / PUBA-04 observation | Mapped window and separate authenticated API client observe all eight accepted sequences per backend, correct image/sample identities and401 negative; transient descriptor removed | PASS |
| PUBA-T05 / regression | Complete integrated CTest63/63; generated native core/GPU publication7/7; prior physics method visible/API proof matches Python reference | PASS |

Local Windows x64 MSVC Release, static-CRT AppContainer worker and Python3.12.
The model is authorized for local use only. An explicit512-edge atlas derivative
is used, with original MOC/Physics3 unchanged. This is not original-resolution,
universal feature, performance or Cubism/author-equivalence qualification. Private
assets, IDs, images, hashes and model-derived measurements remain ignored.

## Commands and exact run custody

* `cmake --build build/m2-d-private --config Release`: `build/pub04a-full-build.log`.
* `ctest --test-dir build/m2-d-private -C Release --output-on-failure`:
  **63/63 PASS**, `build/pub04a-ctest.log`.
* `python tests/moc3/test_packet_publication.py`: **7/7 PASS**; owned-fence and
  actual CoreConsumer executable checks also passed independently.
* `python scripts/moc3_verify_admission_observer.py --publication`:
  `build/moc3-package-evidence/960a1f344fdc4b909e92f7e0/`.
* `python scripts/moc3_verify_admission_observer.py --physics`:
  `build/moc3-package-evidence/9e757e2177cf472089dcc362/`.

Both visible proofs have identical unchanged child before/after and separate
parent-before-child/after-exit code/binary/dependency captures, including actual
core/security/motion sources. They match the exported closure. Local execution is
primary; remote CI was not rerun and no live GitHub gate/PR claim is made.

Earlier integrated discovery ran45 tests successfully but18 were NOT_RUN because
executables had not been built. That log is preserved at
`build/pub04a-ctest-before-full-build.log`; the complete build/rerun above resolves
this verification gap without changing test assertions.

First live run `e8f10b12a208405d86343860` is **FAIL**: the first input lease
expired while admission competed with the integrated test suite. The same packet
passed independent guard/publication with a fresh grant; an idle timing probe in
that ignored folder confirmed admission could fit the unchanged lease. The final
controlled live run then passed on an idle machine. No deadline, test assertion,
fixture resolution or production limit was relaxed. Input lease expiry under
contention is a valid rejection, not a throughput guarantee or accepted frame.

## Review, reconciliation and completion

The independent reviewer found a P2 in malformed diagnostic commands: returning
through a live pending query caused destructor termination instead of the
explicit quarantine path. Every malformed live transport exit now uses
non-unwinding process exit and the line reader is bounded. The generated pending-
query regression verifies the correction; see the separate review receipt.

PDRM-01..04: inspected baseline README/ledger as CURRENT_STATE, public pipeline as
the PLANNED target, PUB-02-B design/evidence as historical bounded proof, and the
new C0 as the current scoped contract. No document is treated as merge permission.
PDRM-05..07: current README/ledger/plan now include bounded host authorization;
full worker acknowledgement remains explicitly absent. PUBA-01..04 each map to
the direct matrix above, without rewriting historical acceptance pins.
PDRM-08..11: intentional optional Windows integration, no migration/cutover or
public ABI change; old unfenced host callers remain supported and regressed.
PDRM-12..16: exact run/source linkage, independent review and catalog/link refresh
support this scoped delivery; code/document PASS is separated from the incomplete
product goal. Shared-DB migration MARM is NOT_APPLICABLE. Current touched docs:
NO_DRIFT after reconciliation; older bounded reports: INTENTIONAL_DIFFERENCE.

Completion: CODE_CHANGE; mandatory PUBA-01..04 VERIFIED with DIRECT integration
evidence. Constraints preserved YES; artifacts/tests/review delivered YES.
Code task and documentation verdicts **PASS**; bounded stage goal **PASS**.
Whole product goal **UNVERIFIED**. Existing historical M2-D custody/stack contract
failures are not waived by this separate CTest suite.

Next: live worker prepare/commit acknowledgement and cancellation/reload state
ownership, then long-lived input expiry/release and session qualification within
an explicit resource policy. Public SDK/packaging, sanitizer/fuzz/performance/
platform qualification and rights-cleared fixtures remain HOLD. Physical D06/D08
still need the unavailable disposable reset-capable runner; healthy GPU tests
above do not supply device-loss/recovery evidence. See the
[release ledger](../../in-progress/M2_MOC3_F4_RELEASE_LEDGER.md).
