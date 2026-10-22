# Alpha #56: typed D3D creation failures and lost-backend quarantine

2026-10-20, Asia/Bangkok. **Scoped BUGFIX PASS.** Base `76dc6282b2b52b015bd38bfdccb8400539a710f6`.
[Issue56](https://github.com/boxs-51/Avatar_Runtime/issues/56),
[ordered backlog](../../in-progress/ALPHA_LINEAR_BACKLOG.md).

## Cause, scope and lifecycle

Private3D resource creation previously converted native HRESULT failures to
`failed`; the deterministic allocation seam separately returned `out_of_budget`.
Event-query creation had the same generic failure. A create-time loss could
therefore bypass loss handling. The correction routes native and injected
creation HRESULTs through one classifier: removed/reset/hung/driver-internal or
failed removed-reason takes precedence, then E_OUTOFMEMORY, then generic failure.
All15 native3D create sites (18 attempts for the two-texture control) and event
query preparation use it. The native and injected branches join before status
classification; injection prevents the selected native call rather than causing
real device removal. No native device/driver reset is performed.

Loss returned from3D build/query preparation latches the private HostDevice
instance. Its target/generation stays rejected on that backend even while the
injected native device remains healthy. Post-draw loss retains the existing
query quarantine and also latches the backend. Fresh host-managed replacement
backend/generation with a fresh target can render and retire. This is not a
cross-instance target registry or full production recreation/physical proof.

Build and query-prepare failures occur before target clear/draw and retain the
caller's capture value, do not pin the target or claim query completion, and
release partial frame objects through existing RAII. OOM/generic errors permit
a healthy retry; loss blocks subsequent submission before any new create.
Successful shader/material/state restoration, private caps and public ABI remain
unchanged. Other2D/host-spike resource creation paths are outside this issue's
private3D finding; no blanket backend qualification is claimed.

## Evidence

The fixture is the existing public synthetic textured GLB; no private model
assets or fixture oracle changes. The matrix exercises18 creation attempts with
six outcomes: OOM, generic failure, removed, reset, hung, and generic failure
with removed-reason. Six event-query cases use the same outcomes. Direct checks
cover target pixels/capture, no query pins/completion, retry and loss barriers,
and a fresh target/generation that renders and exactly retires while the native
device still reports S_OK.

| Requirement | Outcome | Result |
| --- | --- | --- |
| Correct baseline RED |108 resource cases:18 original OOM PASS/90 FAIL; query generic PASS/five typed FAIL; intended CTest exit8 | RED |
| Typed3D/query failures and rollback |108 resource+six query cases PASS on WARP, including retry/quarantine and fresh-target exact retirement | PASS |
| Query/close/dual-mode regressions | Full MSVC rebuild exit0; seven related CTests PASS | PASS |
| Healthy hardware matrix |108 resource+six query cases and existing pixel/state controls PASS, native device S_OK | PASS |
| Full contracts |178/178, exit0 | PASS |
| Independent review | Separate elevated query/WARP2/2 PASS, all114 injected cases and100 healthy query cycles; no supported P0-P2 | PASS |

The first baseline run tripped the query lifetime guard because the broken
baseline unexpectedly allowed a retry submission; the harness now retires any
unexpected successful retry before asserting failure. A second harness issue
used overloaded ComPtr operator& on an existing capture, which resets it at the
call site; std::addressof preserves the intended in/out value. Earlier logs
`build/alpha56-red-before-cleanup.log`,
`build/alpha56-red-before-capture-correction.log` and
`build/alpha56-green-first-attempt.log` are retained and excluded from final
behavioral proof. Final RED is rerun on baseline production files plus only the
injection seam, with the same corrected test assertions used for GREEN.

Final local receipts: full MSVC Release `cmake --build build/m2-d-private
--config Release` at `build/alpha56-full-build.log`; CTest regex
`^(m2_private_3d_scene|m2_warp_3d_pixels|m2_warp_dual_mode|.*query.*|.*close.*)$`
at `build/alpha56-green-ctest.log`; corrected baseline at
`build/alpha56-red-ctest.log` and `build/alpha56-red-summary.log`;
`ars_m2_3d_pixels <corpus> --hardware` at `build/alpha56-hardware.log`;
`python -B -m unittest discover -s tests/contracts -p 'test_*.py'` at
`build/alpha56-contracts.log`. The pure matrix is synthetic error injection;
the rendering/query/close operations themselves use real WARP or healthy hardware.

## Reconciliation and completion

Independent receipt: `build/alpha56-review-ctest.log`. Architecture/project
structure SUPPORTED; optimization NO_RECOMMENDATION. G1 behavior, G2 delivered
source/tests/report, G3 observed corrected RED/GREEN with rollback/recovery, G4
constraints, G5 attributable local commands and G6 separate source/runtime/doc
review PASS. G7 scoped closure PASS subject to the final committed-scope and
integration receipts below. Full product remains UNVERIFIED. Agent review does
not claim GitHub actor-distinct owner approval. The actual committed candidate must pass its exact13-file alpha
custody gate; issue/PR carries source and integration SHA. Catalog/index/backlog
track the correction without rewriting historical oracles. Local proof remains
primary; no remote-CI green is inferred.

Reflected transforms #57 remains open; persistent session/public ABI and full
product qualification remain unresolved. Actual destructive device-loss D06/D08
requires the unavailable disposable runner and stays HOLD. Healthy physical GPU
query/close tests do not satisfy that physical-loss gate. No main merge, public
activation, cap increase or private-asset redistribution occurs.
