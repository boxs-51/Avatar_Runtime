# Alpha #60 A60-1a: isolated native validation probe acceptance

2026-10-20, Asia/Bangkok. Base alpha `49eb54a88cfc30e1104bf12750852827be36aa87`.
[Design and remaining acceptance](../../in-progress/ALPHA_60_ISOLATED_ADMISSION_DESIGN.md),
[issue60](https://github.com/boxs-51/Avatar_Runtime/issues/60).

## Scoped result

The scoped A60-0 probe contract/sequence and A60-1a have a real, default-off Windows x64 CPU implementation and direct local proof. Full A60-0 typed-packet/helper-deployment planning remains coupled to A60-1b and the enabled installed profile. The probe validates generated native bundles entirely inside a zero-capability AppContainer and bounded Job, including ZIP, pinned schemas, PNG, GLB and immutable private model construction. It returns a fixed correlated diagnostic verdict, then frees the model before its allocator context. No encoded model parsing is added to public RuntimeCore.

**Issue60 remains OPEN.** A60-1b complete typed model transfer, A60-2 runtime/model/character ownership, A60-3 public authority/publication bindings and A60-4 enabled installed consumers are not implemented. A verdict cannot create a model handle. Public ABI20, capability0 and feature-off behavior stay unchanged. This is not MOC3/Physics support, release qualification or closure of G00/G01/G02/G10/physical D06/D08.

## Actual local evidence

Final direct CTest command:

```powershell
ctest --test-dir build/alpha60-isolated-native -C Release -R '^alpha60_isolated_native_admission$' --output-on-failure
```

Exited0, passed in19.43 seconds. The script creates a unique retained evidence directory each run; CTest can be rerun without overwriting earlier trials. Qualified receipt:
`build/alpha60-isolated-native/cpu-admission-evidence/4dbae72db17145b792ee79c2f5ce7bc2/evidence.json`,
SHA256 `5c99be817def337f6bd8aa9e967ec76ca773b617024bd1999fe477fa1b27e8f5`.
The receipt pins relevant source and all three executable hashes, checks their stability during the run and retains per-case binary output, launcher state, actual Job accounting and profile-deletion proof. Only generated rights-cleared fixtures were used.

| Observation | Local evidence | Result |
| --- | --- | --- |
| Complete generated native bundle oracle |90 .arschar cases, including12 positive variants (2D/3D/dual/PNG formats) and malformed/unsupported/budget negatives | PASS |
| Framing |12 request negatives: short/truncated/trailing, magic/version/checksum, both reserved halves, zero/over-cap/u32max length, zero nonce | PASS |
| Typed verdict consumer |8 independent invalid replies, exact32 bytes, allowed status and64-bit nonce including high-half-only positive | PASS |
| Real containment |112 total cases; every spawned accepted/rejected worker has observed Job active_processes0/query_error0 and profile absent | PASS |
| Helper deployment | Isolated dedicated copied executable/static CRT; direct execution refused21; missing helper refused before profile; production fault commands absent | PASS |
| Real fault worker | Test-only memory allocation768MiB denied under512MiB cap; crash0xC0000005; output overflow; userCPU20000000x100ns termination; five-second hang termination | PASS |
| Legacy native ingest | Existing SchemaClient-based c2_dual_mode full native bundle oracle, exit0,35.21s | PASS |
| Legacy broker | Rebuilt original MOC3 broker; generated malformed envelope INVALID_ENVELOPE and memory denial MEMORY_DENIED, job/profile cleanup | PASS |
| CPU ABI/security |18 existing b1/b2/b3 tests, exit0,52.53s | PASS |
| Installed feature-off compatibility | Fresh static/shared Release build/install/physical relocation/C11+C++20 consumers; source-before/after stable; cases2 | PASS |
| Contracts |178 unchanged contract tests, exit0 | PASS |

Logs: `build/alpha60-probe-verified.log`, `build/alpha60-legacy-parity.log`,
`build/alpha60-legacy-broker-regression.json`, `build/alpha60-core-regression.log`,
`build/alpha60-installed-off-final/evidence.json`, `build/alpha60-contracts.log`.
This slice reran Release installed consumers only; prior issue59 Debug evidence remains historical and is not relabelled as a new run. Windows local is the primary source. This report does not claim a Linux enabled probe or CI/AppContainer qualification on a different machine.

## Failed experiments retained

The first direct run stopped before decoding: console-subsystem worker exit0xC0000142, with cleanup observed. The worker was changed to the existing pipe-only Windows subsystem/explicit CRT entry pattern; no machine console policy was changed. The next run reached a fast malformed fixture but the old broker returned13 after worker exit0. That old branch did not log whether the accounting query failed or active count was still nonzero; the exact cause cannot be distinguished retrospectively. The broker now records actual query/counts and waits at most500ms for observed zero. The qualified run needed one accounting query per trial; it does not prove a delayed-accounting branch was exercised.

A subsequent run matched all90 native statuses, then failed the harness on the Windows CRLF spelling of an error line. The text-error oracle was corrected to exact CRLF; binary verdict checks were unchanged. Earlier receipts remain FAIL. The initial installed SDK matrix also retained FAIL/SOURCE_CHANGED because CMake was edited during worker debugging; only the fresh source-stable final matrix is accepted.

## Review and delivery boundary

Independent A60-0 design review passed before connected implementation. Final independent exact-diff review PASS: no supported P0-P2 findings, with independent source/binary receipt and compatibility evidence reconciliation. Committed custody/integration checks are recorded on the PR and issue. No public header/runtime_api.cpp/unsupported.cpp blob changed; the separate broker macro leaves the original MOC3 output allowlist in place. Shared broker cleanup diagnostics are the only cross-profile behavioral adjustment.

This dedicated probe build uses a consistent static CRT and rejects SDK installation in the same configuration. Configure the default SDK separately. No helpers, model bytes or private headers are installed by this probe. The16MiB compressed diagnostic cap is narrower than the native ceiling and is not a public admission promise. Complete typed transport and runtime ownership remain the next required work; do not reuse the verdict as an admission capability.
