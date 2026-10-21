# Alpha #39: committed public render acceptance

Local qualification on 2026-10-21 through 2026-10-22, Asia/Bangkok, against alpha base
`83938df825d8a1618d907fc15d72904fb0d3b39f`. The owner selected the existing
C ABI20 render/drain entrypoints in an explicit, default-OFF D3D11 profile.
[The design](../../in-progress/ALPHA_39_COMMITTED_RENDER_DESIGN.md) defines the
bounded contract; [the source receipt](../../reference/audits/ALPHA_39_COMMITTED_RENDER_SOURCE.json)
pins tested source, binaries and local raw evidence. Commit/PR/integration
identity is recorded separately on issue39. Local Windows evidence is primary;
hosted CI corroborates it.

## Implemented outcome

Actual RuntimeRouter snapshots, full runtime/character/model identities and
configuration revisions are captured coherently for every live character.
Immutable frame leases retain the original committed CPU pose and runtime work
through GPU completion. Rendering does not synthesize a pose counter or mutate
the validated model. Native2D neutral-relative deformation and hierarchical
visibility/opacity/Z, and GLB TRS/quaternion, skin, morph and camera projection,
use those committed values. Static preparation remains the empty-overlay case.

The existing `ars_runtime_render` and `ars_runtime_drain_gpu` operate only when
`ARS_ENABLE_PUBLIC_D3D11_HOST=ON`. The public header and twenty-symbol ABI are
unchanged. The CPU-only profile retains UNSUPPORTED render/drain, and the default
profile remains feature-off. This profile handles validated native2D/GLB/dual
bundles, not public MOC3 rendering.

The R-owned host builds a private candidate, waits for its exact draw query,
validates the final CPU census/configuration fence, then copies to the borrowed
host target and waits for a separate exact query. Accepted identities are
published together. Failed preparation, stale configuration, closing admission,
OOM or loss does not relabel the previous frame. Runtime-wide 3D uses shared
depth/opaque-transparent ordering, followed by native2D composition; camera
selection, target format and resource limits are explicit in the design.

All native context access uses the #38 canonical ownership domain. State capture
and restoration surround uploads, drawing, copying and query End. Host geometry,
hull and domain stages and predication cannot suppress SDK output, and their
identities are restored. Conflicting host SRV/UAV/SO bindings are refused before
mutation. Closing drain retires on R; actual GPU references surrender before
unknown-query CPU/runtime pins can be released by the owning receipt.

## Direct local evidence

| Check | Observation |
| --- | --- |
| Final configured public-profile native suite | 53/53 PASS, exit0, zero skipped/disabled, after the selected-scene morph fix; includes100-close/live, Debug consumer correction and two new public multiscene pixel cases; build/alpha39-morph-final53.log and full stdout/JUnit pinned in the receipt |
| Final affected owned/live suite | 4/4 PASS, exit0, after adding100 actual-runtime H-close/R-publication races per backend; WARP/hardware ordinary and debug-captured runs, exact CLOSED result, R drain and unchanged live-object identities |
| Affected private/feature-off regression suite | 38/38 PASS, exit0, zero skipped/disabled; original C2/M2/MOC3/ownership pixel and failure oracles retained |
| CPU-only compatibility | Freshly rebuilt profile37/37 PASS, exit0, zero skipped/disabled, including original UNSUPPORTED render/drain assertions |
| Debug direct C++ smoke consumer | CI exposed a static Debug iterator-ABI mismatch; the same local build failed before a three-line test-target CMake fix. The unchanged1000-finalizer race then passed. Runtime/export flags and feature-off targets are unchanged; installed4-config qualification was repeated on the corrected CMake, all4 PASS. Full51/private38/CPU37 precede this test-build-only delta. |
| Frozen contracts | 178/178 PASS, exit0, native filesystem execution with dedicated workspace TEMP/TMP |
| Numeric producer/projection | Actual semantic ingress/update/capture; neutral/static parity, independent Euler/quaternion calculations, hierarchy, skin/morph/camera and retained historical snapshots |
| Selected-scene morph regression | Observed local RED: valid static scene succeeds but committed projection returns UNSUPPORTED with a mapped mesh instance in another scene. GREEN3: actual-runtime numeric controls for scene0/1, selected descendant/multiple instances and outside-only rejection preserving prior output; public C ABI dynamic pixels on WARP/hardware. Full53 and installed4 are repeated after the source fix. |
| Public pixels | Visibility and half-opacity controls, independently expected colored-quad displacement/Z, committed skin/morph/camera changes; WARP and NVIDIA GTX1060 3GB, driver32.0.15.8157 |
| Fractional LOD and mips | LOD0.25/0.75/1.25/1.75, mip0/1/2 and three sampler filters; independent expected pixel values on both backends |
| Publication and resize | 100 mode switches/resizes per backend; live drain, pending-target replacement refusal, stale configuration, wrong R, invalid device generation and mixed-character census |
| Actual H/S/R races | Rendezvous at draw-ready and copy-ready; configuration/destroy return STALE_HANDLE, close returns CLOSED, already-permitted S update cannot relabel the accepted old pose |
| Allocation and loss rollback | Every observed 18 GLB and 16 native2D GPU creation site fails in turn; accepted full pixels and metadata preserved; injected unknown completion retains actual runtime custody until ordered teardown |
| Borrowed pipeline state | Host geometry shader and both predication values, distinct sentinel target before each trial, exact pixels and identity restoration |
| Live objects | Before/after exact D3D11 live-object identity sets equal on WARP and hardware; debug-captured owned-host runs included, runtime warnings/errors fail the owned fixture |
| Installed SDK | All four static/shared Release/Debug configurations PASS, exit0; physically relocated/deployed C11 and C++20 render/drain consumers, WARP/hardware switches/dynamic/mip pixels, exact DLL20 exports and unchanged source-before/after hashes |

Inputs are generated permitted fixtures, not the private `ulvm2_0002` model.
No official Cubism SDK or physical driver reset was used. Existing owner edits
in ignored Purism viewer.c/viewer.h were preserved. The earlier sandbox F03
TemporaryDirectory failure was rerun successfully with native permissions;
the failed attempt remains historical evidence, not a passing test.

Two independent read-only agents reviewed production source and test/package
oracles separately. They did not execute native workloads. Findings in drain
retry, aggregate budgeting, accepted metadata, typed 2D HRESULT, host shader/
predication isolation, C11 coverage, race observation and pixel rollback were
corrected. Both final bounded reviews report PASS, with no open supported P0-P2.
Typed HRESULT and hostile geometry shader each have observed RED-to-GREEN
receipts. The later GitHub P2 for inactive-scene morph expansion also has a
source-pinned RED-to-GREEN receipt and independent bounded source/oracle review.
No pre-fix execution is invented for the other findings. Historical native51,
affected4 and CPU-only37 receipts retain their original source boundaries;
final53 supersedes the earlier public-profile runs. The installed design
snapshot precedes the final selected-scene wording clarification; runtime and
consumer source hashes are stable throughout the refreshed installed matrix.

## Separate gate decisions

| Gate | Current bounded assessment |
| --- | --- |
| D02 stale RTV / resize | LOCAL PASS for actual public borrowed-target custody, exact-query retirement, pending replacement and stale generation refusal, plus retained #38 registered ownership checks. Historical case JSON stays unchanged; it is not rewritten as a current receipt. |
| D03 host concurrency | LOCAL PASS for the registered production host: R admission/serialization, actual H/S/R fence races, close intent and ordered drain. No promise that an external raw context caller can bypass ownership safely. |
| G03 native2D | LOCAL PASS within the explicit profile: actual committed deform/layers, independent pixels, opacity/Z/visibility, resize, rollback and live-object evidence. |
| G04 GLB3D | LOCAL PASS within the supported validated GLB contract: actual TRS/skin/morph/camera, offscreen pixels, fractional LOD/mips, bounded allocation and teardown. Unsupported asset features remain typed rejections. |
| G05 mode transaction | LOCAL PASS within the explicit profile: 100 switches/resizes on each backend, exact pose/config/target publication, missing-mode rejection, allocation/loss rollback and preserved committed pose. |

These are scoped technical-method observations. Final source custody and alpha
integration are separate delivery requirements, not inferred from green CI.
Whole G00, G01/G02 #41, security #42, broader qualification #61 and release #62
remain separate. Physical D06/D08 remains HOLD #40 until a disposable,
reset-capable GPU runner exists. No public activation or main merge is approved
by this receipt.
