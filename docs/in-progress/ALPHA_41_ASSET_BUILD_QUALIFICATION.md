# Alpha41 — public asset/build qualification contract

2026-10-23, Asia/Bangkok. User authorized starting issue #41 after the
read-only repository/GitHub reconciliation. Baseline alpha
`006f36689d1c432659925ea2f9abe865a7825831`; separate worktree/branch preserves
the Studio experiment and dirty auditor documents. Issue #41 is the requirement
source, not the Studio Beta roadmap. No main merge or shipping activation.

| ID | Required observation | Implementation/evidence plan |
| --- | --- | --- |
| A41-1 / G01 | Reproducible build and installed ABI20 consumers by platform/profile/config/linkage | Windows MSVC x64 static/shared Release/Debug; CPU/D3D11 explicit profiles and default-OFF separately; Linux GCC/Clang retained as gaps if unavailable |
| A41-2 / G02 | Valid and hostile ZIP/JSON/PNG/GLB bundles through public C ABI | Reuse immutable generated corpus oracle, verify hashes, exercise installed C11 consumer with exact per-fixture statuses |
| A41-3 / G02 | Failure zeroes output, preserves accepted state and resource/lifetime custody | Keep a live baseline model/character while every corpus admission runs; callback accounting, old-character update, repeated handle checks, exact modified-opacity pixel equality and close; nonneutral motion-pose preservation remains unverified |
| A41-4 / G02 | Rejection before GPU upload and actual render/drain integration | Installed public WARP/C11 render/drain plus separate native observation of the existing production renderer creation hook, with positive control; no production export or driver-wide telemetry claim |
| A41-5 | Honest source/build/corpus custody and gate verdicts | Per-case receipts, command logs/exit/source/binary/corpus hashes; fail on missing cases, no assertions as verifier control flow; separate G01/G02 and HOLDs |
| A41-6 | Full fuzz/ABI/security proof | Preserve existing full-duration campaigns and exact unchanged closure; no short-run substitution; #42 full87 and #40 physical faults remain separate |

Order: freeze matrix -> implement installed public hostile-input consumer and
auditable runner -> execute Windows available legs -> independent review ->
record exact outstanding platform/pose/fuzz gates. No untrusted parser or
Linux security boundary is introduced merely to remove a qualification HOLD.
Using existing validated consumers/corpus is preferable to a second importer;
new tests must traverse public `ars_model_load`, not only private validation.

Existing code observations: `ARS_ENABLE_PUBLIC_CPU_ASSETS` is Windows x64-only;
D3D11 requires that CPU option. Linux public activation is a CODE GAP, not N/A.
This host exposes only WSL installation/help rather than a usable distro; no
Linux workload has run. Do not install an OS or reset the GPU for this slice.

Private assets and Studio are excluded. Generated fixtures only. The unchanged
public ABI/header, default flags, security oracles and 3600-second fuzz durations
remain protected. Code presence or a successful local leg cannot close #41.

Execution and separate G01/G02 decisions are recorded in the
[Windows qualification report](../completed/audits/ALPHA_41_WINDOWS_ASSET_QUALIFICATION.md).
Its completed classification applies to the bounded Windows experiment, not
the whole issue. Linux needs a reviewed public admission/isolation design and
actual GCC/Clang integration evidence. The synthetic 2D baseline has zero vertex
deltas: changing layer opacity is a real configuration/pixel oracle but cannot
prove rollback of an already nonneutral motion pose. That proof stays open.
