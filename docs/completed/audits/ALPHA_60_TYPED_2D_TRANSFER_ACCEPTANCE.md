# Alpha60 A60-1b-2D local transfer acceptance

2026-10-21, Asia/Bangkok. Base alpha `fa9a1a02afc8cf75c1867f58aff9f6e70fd0bdbb`.
[Design](../../in-progress/ALPHA_60_TYPED_2D_TRANSFER_DESIGN.md),
[issue60](https://github.com/boxs-51/Avatar_Runtime/issues/60).

## Scoped result and boundaries

A complete private immutable native2D model can now leave the isolated worker as a versioned typed packet and be reconstructed only after independent host validation. The host library/guard links no encoded ZIP/PNG/JSON/GLB decoder. All model fields, RGBA bytes, mapping defaults/bounds and deform data survive the transfer and caller-buffer destruction. No registry handle, source authority, public ABI feature or GPU operation is added.

**Full issue60 stays OPEN.** Valid3D/dual inputs return explicit UNSUPPORTED for this packet profile, after native validation; their fields are never silently omitted. GLB graph transfer, runtime-owned model/character pins, public semantic/authority/publication bindings and enabled installed consumers remain unimplemented. Resource must outlive every model allocated from it. Runtime close ownership is not proven by this private codec. MOC3/Physics, SDK activation, main, physical D06/D08 and G00/G01/G02/G10/production gates are unchanged.

## Direct local proof

Built the actual Windows x64 Release worker, separate model broker, typed host guard and test-only legacy reference with the existing locked offline dependency cache. `ARS_BUILD_CPU_ADMISSION_PROBE=ON` is a dedicated default-off static-CRT experiment, not an installed SDK. The model broker has its own16MiB output cap; the original diagnostic64-byte and MOC3 profiles retain their caps and output allowlists. Existing512MiB/one-process/2CPU-second/5wall-second worker Job and256MiB Resource/16MiB input remain unchanged.

```powershell
ctest --test-dir build/alpha60-isolated-native -C Release -R '^alpha60_typed_2d_transfer$' --output-on-failure
```

Final exit0,8.34 seconds,150 cases. Current source/binary-attributed receipt:
`build/alpha60-isolated-native/cpu-model-evidence/17d8b6c614ca4e28bb2ea0e4dee4cedc/evidence.json`,
SHA256 `4054cfb928a7e523477400dfe399c5d7f3d46e1da55d4f305110e8e148c74f06`.
This pins eleven source files and five actual executables; qualification checks before/after stability. Final closure rechecked after native regressions rebuilt the schema helper. Earlier successful development runs remain scoped to their own source/binaries and are not substituted for this receipt.

| Requirement | Actual observation | State |
| --- | --- | --- |
| REQ60-T1 complete2D transfer | Test-only legacy reference independently loads the source fixture, then compares every nativeCPU field with the decoded model: canvas/atlas, layers/effective opacity, all parameter and semantic fields, all geometry/UV/indices/deforms; prepared draw order/geometry/opacity also match | PASS |
| REQ60-T2 hostile host boundary |53 malicious packets including recomputed digests, malformed counts/UTF8/bindings/duplicates/nonfinite/defaults/indices/rects/mappings/nonce/request hash/profile/status; each keeps the prior live model pointer, Resource bytes and allocator records unchanged | PASS |
| REQ60-T2 allocation/lifetime |29 observed decode allocations and12 encode allocations individually failed; all714 baseline truncations; exact alloc/free byte/alignment matching; BUSY/lowered work/pixels/throwing allocator; model used after input buffer overwritten/freed; no leaked host records | PASS |
| REQ60-T3 actual isolation |96 worker trials, each worker exit/wait0, no overflow, actual Job query_error0/active_processes0 and profile absent | PASS |
| REQ60-T3 explicit profile/caps | Three valid native3D/dual variants return UNSUPPORTED; valid16MiB decoded atlas loads through original native ingestion but exceeds complete packet cap, encoder preserves previous bytes and returns OUT_OF_BUDGET with no partial model | PASS |
| REQ60-T4 native fixtures | All90 pinned generated native bundle statuses, with explicit typed3D/dual restriction; six new generated variants: decimal precision,128-codepoint/512-byte parameter/mapping target, rich fields, empty catalog,512-semantics catalog, packet budget | PASS |
| REQ60-T5 legacy compatibility |20 configured CTests:18 unchanged b1/b2/b3 CPU/security plus112-case diagnostic probe and original full native bundle oracle, exit0/59.98s | PASS |
| REQ60-T5 original MOC3 profile | Rebuilt original broker; generated malformed envelope and memory denial statuses unchanged; job/profile cleanup | PASS |
| REQ60-T5 installed defaultOFF | Fresh static/shared Release library install/physical relocation/C11+C++20 consumers; source-before/after stable; cases2 | PASS |

No decoder/model data from a restricted sample was used: all inputs are generated fixtures. The reference executable intentionally links encoded decoders for test comparison only; the production candidate host guard does not. This is a complete CPU-field/geometry/atlas parity check, not an offscreen GPU image, realtime physics, author fidelity or release performance claim. No visible realtime viewer is launched because this slice performs admission/CPU transfer only.

## Review, compatibility and remaining evidence

Independent design review required preserving f64 semantic to f32 parameter projection and UTF8 codepoint limits. Preliminary code review found that layer/part IDs also need the pinned manifest binding syntax; the codec was corrected and digest-recomputed hostile cases now reject those IDs. Valid Unicode parameter/mapping IDs remain supported. Final independent exact-diff/PDRM review verified the current receipt/source/binaries, all150 cases and96 cleanup proofs and found no remaining supported P0-P2 issue. Final receipt/regression and committed custody/integration outcomes are recorded on the PR/issue.

The immutable shared-pointer deleter still references its Resource; this patch documents and tests that private lifetime contract rather than pretending to implement runtime-owned pin/close semantics. Model output replacement is transactional, including rejection packets and OOM. SHA256 binds expected request/nonce and detects corruption; it does not authenticate a compromised worker. Structural and semantic checks therefore run independently before publication.

Additional evidence: `build/alpha60-t2d-final-qualified.log`, `build/alpha60-t2d-legacy-regression.log`,
`build/alpha60-t2d-installed-off/evidence.json`. Original MOC3 broker checks, final committed contracts and exact scope/custody are recorded at delivery. Public header/runtime_api.cpp/unsupported.cpp stay identical. Only local Windows enabled-transfer evidence is claimed; issue59's Debug/other-platform runs and PR72's eleven successful workflows are historical cross-checks, not evidence for this new source.
