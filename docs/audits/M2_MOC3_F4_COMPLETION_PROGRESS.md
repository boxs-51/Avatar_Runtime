# F4 completion progress and public-input decision

This is the historical checkpoint preceding the [static PNG/Physics3 package admission result](M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md). The subsequent PUB-01/PUB-03 subset passes; persistent public evaluation, SDK and qualification remain unfinished. Historical evidence and source hashes below are preserved.

**2026-10-19, Asia/Bangkok. Private continuity/installed-consumer slices PASS. Windows parser-preflight prototype PASS within its bounded claims. Whole F4 public importer and release remain HOLD.** The owner selected arbitrary public-user MOC3 input during this work. No public capability, ABI or activation was added; nothing was committed, pushed or merged. Overall task/goal verdict: **REVISE**, because public end-to-end import/SDK implementation and mandatory qualification remain unfinished. [Structured goal assessment](M2_MOC3_F4_GOAL_ASSESSMENT.json).

## Completed technical slices

| Slice | Actual evidence | Result and limit |
| --- | --- | --- |
| Persistent local controller | `scripts/moc3_verify_visible_f4b.py`, final ignored run `0d7214b29e742c72c8471309` (earlier proof `9a0980be222f2c655a5cab7b` retained) | PASS actual mapped UI/API: queued versus committed control, changed native pixels, invalid/replayed input rejection, lease expiry, exact disabled/default reset, old-run rejection, fresh owner with no held input and cancellation with empty owned Jobs. |
| Continuity WARP | `build/moc3-f4b-local/5e68bbc67708aaff688da5cc/evidence.json` | PASS five-minute warm-up plus thirty measured minutes; exact source/binary match, bounded two-slot output, recorded timings, no working-set/handle investigation trigger and unchanged 1536 MiB pair peak limit. |
| Continuity hardware | `build/moc3-f4b-local/e82ef7c245584b7497d1b3ea/evidence.json` | PASS same workload on GTX 1060 3GB, driver 581.57, concurrently with WARP. |
| Installed private consumer | `build/f4-private-bundles/be703cdd44104c5ebc50fda9`, final smoke `build/f4-installed-smoke/49ecb1d3b1f04562bb504b3f` | PASS WARP/hardware from `C:/Windows`, `python -E -B`, explicit external model and output; no source/build/Git lookup by the installed consumer. Standard-library bootstrap verifies inventory before implementation import. Native-default release/reset equality and held-input physics changes in both geometry and clean pixels pass. |
| Public parser isolation only | `build/f4-public-preflight-evidence/81c70bc988b14a3794c6d4bd/evidence.json` | PASS 23 generated/probe cases plus a separate authorized-private positive. Actual AppContainer/zero-capability identity, protected read-only worker copy, owner-only outside-file read/write denial, child-creation denial, environment canary exclusion, memory/CPU/wall limits, crash/flood containment, typed envelopes/mutations and profile/Job cleanup. |

The continuity simulation advances six fixed physics steps per presented update, requested four presentations/second. It is deliberately slowed fixed-clock continuity, not real-time catch-up or a 720p/30-FPS tier proof. Memory observations cover **one native worker plus its Python continuity owner** per run; the GUI coordinator and whole machine are excluded. Detailed model-derived timings, memory, counts, inputs, hashes and pixels remain ignored. The GUI was observed mapped with matching core/frame identity while the soak was running. **Terminal soak API/UI capture: NOT RUN successfully**: the final capture attempt occurred after the bounded window had auto-closed. Do not infer terminal UI identity from the worker PASS packet. The separate short live API/UI test supplies direct interactive identity evidence. Both long workers and the window exited normally; final checks found no product worker and no transient harness descriptor.

The private bundle contains an explicit code/binary allowlist and pinned PurismCore/asset dependency notices, with no model assets. Its manifest is local integrity detection, not signed authenticity. No root project redistribution LICENSE was supplied; the notice keeps the bundle local. Existing system Python, Windows and compatible runtime are prerequisites, not packaged dependencies. This is a private installed CLI consumer, not G10 shipping SDK acceptance.

Public parser preflight returns a fixed capped status and never routes accepted hostile bytes into the trusted local renderer. Its loopback result is **NO_CONNECTION_WITHIN_FROZEN_WINDOW**, with live host controls before/after, not universal network-denial attribution. WFP correlation and IPv6/UDP/remote-egress tests remain HOLD. The pipe-only worker avoids console startup errors without changing LowBoxConsoleEnabled. Stderr is discarded through an inherited NUL handle, separate from the 64-byte stdout cap. Native Job CPU limits are supplemented by a trusted 10-ms CPU monitor because Windows enforcement is periodic. The final environment is an explicit platform whitelist, not host-variable inheritance.

Final combined-source F4-A mapped verifier also PASS in `build/moc3-f4-local/6e59780019c8f285cabf0fa4`, including monotonic frame/core identity, late-revoke nonpublication and active cancellation. Earlier F4-A report/manifest remain historical. [Current source and binary closure](M2_MOC3_F4_COMPLETION_SOURCE.json).

## Tests and review

| Command/check | Observation |
| --- | --- |
| `python tests/moc3/test_core_controls.py` | PASS 4 generated tests, including competing callers, pending/replay distinction and huge integer rejection. |
| `python tests/moc3/test_harness_api.py` | PASS 11 tests, including typed HTTP 400 for huge integer input without queue/sequence mutation. |
| `python tests/moc3/test_private_bundle.py` | PASS 7 tests, including actual subprocess shadow-module/import sentinels, tamper/extra/path rejection and release-only/single-component false-physics negatives. |
| `python tests/moc3/test_public_preflight.py` | PASS 4 generated broker tests; explicitly not OS evidence. |
| `python scripts/moc3_verify_public_preflight.py` | PASS actual OS suite above, exit 0; run requires profile creation/deletion privileges and remains opt-in. |
| Final CTest affected core/source/public C/C++/nine publish-race families/API/F1-F4/native renderer/legacy host selection | PASS 22 selected tests, exit 0; native tests and generated tests retain their distinct proof scopes. |
| Separate MSVC AddressSanitizer native consumer CTest | PASS 1 test, ASAN DLL imports confirmed. |
| Actual ASAN WARP/hardware 12-frame integration | **FAIL unchanged combined peak-memory cap** in ignored run `build/f4-asan-private/c95fc77bc39e4c81984a4f53`. Geometry/pixel oracles ran, but memory acceptance did not pass. Integrated ASAN qualification remains HOLD; no threshold was loosened and no whole G07/G08 result is awarded. |
| Independent reviews | Controller PASS; bundle PK-01/PK-02 fixed with generated regressions; bounded parser-preflight security/source/binary/evidence review PASS. No supported unresolved P0-P2 findings reported for those scoped artifacts. Reviewer inspected recorded OS/private evidence rather than rerunning the private model. |

Earlier diagnostic failures remain ignored evidence: malformed huge integers, pre-import/shadow-module execution and a release-only physics oracle were corrected; initial AppContainer console startup failed; initial network tests wrongly assumed WSAEACCES; initial periodic-Job-only CPU enforcement exceeded the frozen witness; sanitized environment setup required the explicit platform LOCALAPPDATA location. The final security limits and pixel/memory oracles were retained. Source-pinned F1-F4-A packets remain historical states; the completion manifest records the present closure rather than rewriting earlier tested hashes.

## Remaining items

| Item | Disposition / exit condition |
| --- | --- |
| Public end-to-end import | **IMPLEMENTATION HOLD / not complete.** Isolate manifest/PNG/physics admission and retained evaluator; independently validate a typed drawable/atlas stream before trusted GPU allocation. Do not reparse public MOC in the host. [Next implementation contract](../M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md). |
| Public SDK/capability/lifecycle | HOLD separate material ABI/packaging and installed static/shared/platform qualification; current public model/character/render entries remain unsupported. |
| Sanitizer/fuzz | HOLD integrated sanitizer memory profile and coverage-guided hostile corpus/protocol qualification. Generated mutations are not coverage-guided fuzz evidence. No Clang/libFuzzer toolchain is configured on this machine. |
| G09 tiers | HOLD representative 720p/30-FPS quality/fallback and retained GPU allocation evidence. Duration-only continuity does not close G09. |
| Public fixture/reference/redistribution | HOLD rights-cleared redistributable model and independent authored reference for any fidelity claim; the private sample may never be published. |
| Network/cross-platform | HOLD stronger WFP/IPv6/UDP/remote evidence and non-Windows isolation/product build qualification. |
| D06/D08 physical | HOLD disposable reset-capable Windows GPU runner, per owner instruction. No driver reset/TDR was performed here. |
| Release | HOLD full baseline gates, reproducible shipping artifacts/notices and explicit activation decision. |

The next code work follows PUB-01 -> PUB-06 in the linked contract. These are outstanding implementation/qualification requirements, not gates closed by a successful parser or a private bundle.
