# PUB-02-B: isolated bounded physics acceptance

2026-10-19, Asia/Bangkok. **Scoped PHY-01..04: PASS.** Full PUB-02/PUB-04,
shipping SDK and whole F4 remain **HOLD**. Physics now executes in the opt-in
bounded batch; static and parameter-only modes remain **VALIDATED_NOT_SIMULATED**.
This independent method makes no Cubism/author-equivalence claim.

Contract: [PUB-02-B C0](../../in-progress/M2_MOC3_PUB02_B_ISOLATED_PHYSICS_DESIGN.md),
continuing the user's instruction to implement remaining work after `89f732d`.
[Exact source closure](../../reference/audits/M2_MOC3_PUB02_B_SOURCE.json) and
[independent review](M2_MOC3_PUB02_B_REVIEW.md) accompany this report.

## Delivered code and direct proof

`retained_physics.cpp/.hpp` implement the existing independent Python Physics3
method natively inside the AppContainer worker. Typed chains are constructed only
after strict linked-data/catalog validation. A fixed 16,666,667 ns clock retains
particles/remainder, supports complete manual overrides, release, OFF and reset.
Preparation does not mutate committed state; owner/thread/generation checks guard
commit after successful evaluator update and snapshot serialization.

`--simulate` adds explicit bounded control bits. Output outer version2/inner
version3 requires the execution marker and linked-physics flags; earlier versions
forbid the marker. The host checks every record before scene allocation/GPU.
Mid-batch failure returns a nonzero worker exit; the trusted launcher discards all
partial stdout, and broker custody is empty. This is not the final product
core/GPU/physics authority transaction, which remains PUB-04.

| Task / requirements | Actual evidence | Result |
| --- | --- | --- |
| PHY-T01 / PHY-01 native solver | Single and nonsaturating weighted multi-chain generated fixtures: Python/native float32 parameters exactly match across seven control frames | PASS |
| PHY-T02 / PHY-02 transaction | Uncommitted replay, invalid prepare leaves next candidate unchanged, stale/cross-owner commit, wrong-thread, rewind, backlog, NaN, remainder, OFF and reset tests | PASS |
| PHY-T03 / PHY-03 protocol | Native sequence corpus1,373 rejection checks, previous verified output retained; actual mixed late version/missing or unknown flags/wrong outer version reject before any GPU calls | PASS |
| PHY-T04 / PHY-04 local geometry | Actual isolated ON/OFF XY differences, released temporal XY response, OFF restores default, reset equals fresh state, deterministic fresh-worker replay | PASS |
| PHY-T04 / PHY-04 local pixels | WARP and hardware: exact same-backend trusted Python-method pixels for all eight frames; actual ON/OFF and released temporal differences; OFF returns default | PASS |
| PHY-T04 / PHY-04 observation | Mapped window presents eight sequences per backend; separate authenticated API client observes all eight, verifies readback identity and401 negative; transient descriptor removed | PASS |
| PHY-T05 / PHY-01..03 failures | Missing linked Physics3, unknown control, controls in parameter-only mode and late accumulated backlog; no candidate custody and job/profile cleanup | PASS |
| PHY-T06 regression | CTest30/30; frontend/oracle6/6; actual static and parameter-only visible/API regressions; actual OS limit/cleanup preflight | PASS |

Environment: local Windows x64 MSVC Release, worker static CRT, Python3.12.
The actual model is owner-authorized for local use only. This proof explicitly
uses a 512-edge atlas derivative; original MOC/Physics3 remain unchanged. No
automatic production resizing, enlarged budget or universal compatibility claim.
Private assets, IDs, geometry, image hashes and measurements stay ignored.

## Reproduction and provenance

* `cmake --build build/moc3-public-isolation --config Release --target ars_moc3_retained_physics_tests ars_moc3_import_worker ars_moc3_sequence_tests`;
  final build log `build/pub02b-final-build.log`. Host build:
  `cmake --build build/m2-d-private --config Release --target ars_moc3_packet_render_host`,
  log `build/pub02b-host-build.log`. Worker/guard targets were also rebuilt in
  `build/pub02b-worker-build.log`.
* `ctest --test-dir build/moc3-public-isolation -C Release --output-on-failure`:
  **30/30 PASS**, `build/pub02b-ctest.log`.
* `python tests/moc3/test_native_physics_reference.py`: PASS, both generated
  fixtures; `python tests/moc3/test_retained_session.py`: **6/6 PASS**.
* `python scripts/moc3_verify_admission_observer.py --physics`:
  `build/moc3-package-evidence/f3268d95e17748f3b59920cc/`.
* `python scripts/moc3_verify_admission_observer.py --retained`:
  `build/moc3-package-evidence/9128a32938c543f29a1bd571/`.
* `python scripts/moc3_verify_admission_observer.py`:
  `build/moc3-package-evidence/4b1814a517af41f28a40d292/`.
* `python scripts/moc3_verify_public_preflight.py`:
  `build/f4-public-preflight-evidence/1d1181f32ace41e38910a538/`.

Physics, parameter and static runs each have matching unchanged child before/after
and parent-before-child/after-exit code/binary/dependency captures. All agree with
the final source closure. Local execution is primary; remote CI was not rerun.
OS preflight directly rechecked file/process/environment/limited-loopback,
memory/CPU/wall/crash/flood and cleanup. Universal network attribution remains
HOLD. Production caps remain512 MiB, two CPU seconds, five wall seconds and192 MiB
aggregate output; no driver reset/TDR was run.

## Scoped document reconciliation

Inspected inventory: README/current release ledger (CURRENT_STATE), public-input
pipeline/import design (PLANNED_PROPOSED target and effective isolation boundary),
PUB-02-A design/acceptance/source (HISTORICAL_REFERENCE bounded contract/evidence),
PUB-02-B C0 (current scoped contract), and the unchanged Python solver method.
Baseline is `89f732d`; tested worktree files are pinned separately. The current
user continuation authorizes implementation and its necessary documentation;
repository documents do not themselves grant merge/activation permissions.

| Reconciliation controls | Observation / disposition |
| --- | --- |
| PDRM-01..04 inventory/provenance/authority/time | Relevant local documents inspected; historical acceptance pins preserved; planned whole-product target separated from observed bounded implementation |
| PDRM-05..07 roadmap and bidirectional behavior | README/ledger/plan now include bounded native physics; static-only physics claims retain their original scope. PHY-01..04 map to code and direct rows above; no observed scoped contract drift |
| PDRM-08..09 architecture/migration/rollback | Intentional optional Windows path, default OFF, no DB/migration/cutover; old modes retained and directly regressed |
| PDRM-10..11 interface/security | Versioned opt-in controls/flags documented; no public ABI change; worker limits/failed-output isolation and no private redistribution preserved |
| PDRM-12..14 evidence/coexistence/linkage | Source closure, exact run IDs and separate review trace claims; old reports are historical, not contradictory current physics assertions; catalog/link inventory refreshed |
| PDRM-15..16 authorization/handoff | Necessary current-state doc updates within continuation scope; scoped code/document PASS separated from incomplete product goal and open HOLDs |

No legacy/modern shared database migration is involved (MARM NOT_APPLICABLE).
After reconciliation, touched current documents have NO_DRIFT; older static/
parameter snapshots have INTENTIONAL_DIFFERENCE. GitHub live PR/protection and
shipping deployment state are not audited here.

## Goal assessment and remaining work

Deliverable kind CODE_CHANGE; goal is continued implementation, with mandatory
PHY-01..04 frozen for this bounded stage. All four have DIRECT local evidence,
are VERIFIED, and required artifacts/tests and independent review are delivered.
Constraints preserved YES; code_task_verdict **PASS**, documentation_verdict
**PASS**, stage goal_verdict **PASS**. Whole public product goal is **UNVERIFIED**.
No merge, deployment, public activation or physical reset is implied.

Remaining: long-lived isolated session/input ownership and expiry; PUB-04 final
core authority fence, cancellation/reload and prior-frame retention transaction;
public SDK/capability/package contract; fuzz/sanitizer, representative performance,
platform/network qualification and redistributable fixtures. Physical D06/D08
still require the unavailable disposable reset-capable GPU runner. Existing two
historical M2-D custody/stack contract failures remain baseline issues, not green
or waived by this scoped30/30 CTest result. See the
[release ledger](../../in-progress/M2_MOC3_F4_RELEASE_LEDGER.md).
