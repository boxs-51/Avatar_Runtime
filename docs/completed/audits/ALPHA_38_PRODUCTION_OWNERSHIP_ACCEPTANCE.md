# Alpha #38: owned native context teardown acceptance

2026-10-21, Asia/Bangkok. Base alpha
`64c129eb1c8be3a4a12ac8b7fb807bfdbb0276eb`. The implementation and direct local
evidence satisfy the current registered native owner graph in
[the design](../../in-progress/ALPHA_38_PRODUCTION_OWNERSHIP_DESIGN.md).
Commit/alpha integration identity is recorded separately on issue38/its PR.
This is not acceptance of a public GPU API, unknown future external callers,
physical loss, or the whole G00/product gate.

## Implemented ownership graph

| Actual owner or user | Admission and lifetime enforcement |
| --- | --- |
| PacketPublication and LeasePublication | Mandatory owned-host factory; a real CoreConsumer RuntimeLease pins actual runtime work; adapter device/context participant; R-only readback operation covers staging, copy, Map/Unmap and staging destruction |
| D3D11DualModeHost and HostDevice | Canonical device/immediate-context/generation domain; backend and active/pending target owners; raw accessors require an admitted operation; close intent precedes cancel/drain; late final fences cannot publish |
| 2D, GLB3D and MOC3 renderers | Frame participants outlive all frame GPU ComPtrs; CPU snapshot pins are separate from GPU frame pins; query creation/retirement and typed rollback remain on R |
| StateScope and MOC3 ExtraScope | A nested lease covers capture, restoration and retained-reference destruction; supplied context must match the domain before any Get*; MOC3 extra restoration stays inside that outer lease |
| AtlasResidency | Cache state has its own participant and exact domain identity; retained cache references independently block receipt issuance until reset on R |
| QueryRetirement | Healthy exact completion releases GPU/CPU owners; unknown completion surrenders GPU resources on R first, preserves CPU pins, then accepts only the owning host's receipt path; legacy detach cannot bypass it |
| Registered native external owner/callback | Bounded participant plus operation/ticket; close denies new work and late tickets, active restoration/readback defers drain, every registered owner must surrender GPU references |
| CPU authority | Real runtime work remains BUSY through issued-but-unconsumed receipt; successful host consumption releases quarantined CPU and runtime pins before finish_close |

The bounded registry rejects a second live domain for the same context. No ARS
mutex is held across D3D or user callbacks. Private standalone diagnostic
constructors keep their existing oracles. Legacy CloseCoordinator cannot consume
a production-domain receipt. Process-local test observers retain their known
device/context/debug interfaces solely to compare live objects; those fixture
references are not represented as production participants.

## Direct local proof and provenance

[The source receipt](../../reference/audits/ALPHA_38_OWNERSHIP_SOURCE.json) records
raw source, binaries, commands/output and log hashes. Inputs were generated
fixtures, including validated `positive-dual-full.arschar`, not the private model.
Purism Core is pinned at `1069334965522df5d0b791e97f01e26b19c45456`; only the
owner's viewer.c/viewer.h edits remain, with samples/viewer/tests disabled.
Official Cubism SDK was neither downloaded nor linked.

| Evidence | Observation and limits |
| --- | --- |
| Full configured local native suite | 74/74 PASS, exit0, before the final cross-domain StateScope and diagnostic-receipt isolation fixes; retained at build/alpha38-regressions.log |
| Affected final production source | 38/38 PASS, exit0: core/security, original private GPU/query/receipt and 2D/3D/dual pixel oracles, actual packet/step publication and new ownership tests; build/alpha38-final-affected.log |
| Frozen Python contracts | 178/178 PASS, exit0, unchanged assertions, using native filesystem permissions and dedicated workspace TEMP/TMP; two sandbox attempts failed only TemporaryDirectory access in the F03 exponent case |
| Final owned graph/JUnit | Four tests PASS, zero skipped/disabled: WARP and named NVIDIA GTX1060 3GB, driver32.0.15.8157, plus separately debug-captured WARP/hardware executions; build/alpha38-final-owned.xml |
| Validated native2D/GLB3D | 100 owned switches/resizes per backend, exact query/pose/target epoch, staging readback pixel parity and target-allocation failure preserving prior frame/epoch |
| Actual adapters | Both packet and lease paths: healthy query/readback, pending close, injected removal, close reentry at real query-ready, cancelled late publication, closed admission/readback and diagnostic warning with completed cleanup |
| Negative ownership/receipt | Owner/operation capacity, foreign/deferred context and generation, restoration-capacity rejection without state mutation, two-domain scope identity, active scope/registered external context/cache independently blocking receipt, wrong R, default/early/wrong authority-device-generation/replayed receipt and legacy consumer rejection |
| Pin ordering | Unknown CPU snapshot survives GPU surrender; actual public-runtime finish_close returns BUSY before and after issuance; only valid owning-host consumption releases CPU/runtime and permits finish_close |
| Live resources | Exact ID3D11 live-object identity sets equal before/after (2 objects each) on WARP and hardware. Both require the debug layer; warnings/errors during the fixture graph fail the test |

Loss here means bounded QueryOps injection on healthy devices, not an actual
DXGI reset/removal. Debug live-report messages are observed separately from the
runtime warning check. An explicitly injected adapter warning reports failure
after safe close completes; a second close succeeds, so a diagnostic cannot
permanently strand runtime custody. The integrated GLB test exposed a missing
optional sampler binding; a frame-owned fallback now binds both slots. Existing
allocation/HRESULT/pixel oracles passed unchanged after that correction.

Two independent read-only agents reviewed the implementation. Review1 identified
restore-capacity handling, device/context identity and diagnostic-close cleanup;
Review2 identified foreign-context restore enrollment. All were fixed and covered
by direct regression checks. The final bounded source re-review reports PASS,
with no supported open P0-P2. Reviewers did not run native OS workloads. New
review findings have source/positive negative regression evidence; no fabricated
pre-fix RED execution is claimed for every finding.

## Gate disposition and remaining work

H38-1..H38-4 are locally verified for the graph above. H38-5 delivery requires
committed path custody, contract checks, CI cross-check and authorized alpha
integration; the issue/PR carries that final receipt.

**G00 technical-method:** locally accepted for registered native ownership,
admission, exact healthy completion and two-phase injected-loss teardown. This
replaces the previous private-host-only evidence for that bounded method. It
does not certify all future SDK hosts or an arbitrary raw native AddRef caller.
Public graphics remains OFF; any future context entrypoint must enroll before
using this method. Whole G00 remains HOLD for its separate architecture/security
obligations. D02/D03 now have direct current owned-graph resize/close evidence;
the historical case JSON remains an immutable older receipt, not a current-head
claim. Remaining committed pose/camera and full rendering matrix belongs to #39.

D06/D08 physical loss/recovery remains HOLD under #40: no disposable reset-capable
runner, no TDR/driver reset/device disable on this machine. G01/G02 full matrices
remain #41, security #42, broader qualification #61 and release #62. No public
activation, main merge, ABI capability change or private asset publication.
