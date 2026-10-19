# F3 bounded manifest-linked physics: local acceptance

2026-10-19 (Asia/Bangkok). **PASS for the first private F3 slice** on the [source/binary manifest](../../reference/audits/M2_MOC3_F3_LOCAL_SOURCE.json), parent commit `69eac34`. F3 changes remain uncommitted for review. User authorized F2 recheck/commit followed by F3 implementation. Local Windows execution is primary; no GitHub Actions result is used to award this verdict.

The independently implemented adapter consumes the actual manifest-linked immutable Physics3 bytes, validates bindings against the native evaluator's actual parameter catalog, and feeds complete poses into the same evaluator and D3D11 host used by F2. It implements the [frozen bounded policy](../M2_MOC3_F3_LINKED_PHYSICS_DESIGN.md), not a claim of Cubism algorithm or author-intended animation equivalence. External viewer/synthetic spring tests are excluded from this acceptance.

| Requirement | Direct evidence | Verdict |
| --- | --- | --- |
| F3-01 linked admission | Physics3 comes from admitted package bytes, never an unrelated discovered sidecar. Checked version/counts/numerics/normalization, particle properties, parameter targets, actual IDs/ranges/defaults and output vertex references. | PASS |
| F3-02 bounded clock | Generated rewind/backlog/zero-delta/fractional accumulator tests; fixed 60 Hz and bounded catch-up. Actual private negative timestamps preserve accepted state. | PASS |
| F3-03 ordering | Generated nonsaturating exact output-weight oracle; two-chain repeated-destination file order and source isolation test; actual checked native catalog and complete-pose handoff. | PASS |
| F3-04 transactions and reset | Prepare does not publish; rejected/stale/cross-engine candidates and discarded downstream candidate preserve accepted state. Commit follows evaluator and exact host query completion. Disable returns defaults; reset and recreation of physics state replay exactly. | PASS for bounded worker |
| F3-05 generated dynamics | Numeric first step, length constraint, normalize/reflect/weight/clamp, held-input chunk equivalence, temporal release, disable/reset/reload, hostile descriptors/inputs/clock, owner thread and fixed-size 10,000-step CPU soak. | PASS / 9 tests |
| F3-06 actual integrated proof | WARP and GTX 1060 3GB: on/off differs in both geometry checksum and clean pixels; release changes both over time; reset and physics reload repeat actual readbacks. Named adapters/debug and unchanged memory policy recorded privately. | PASS |
| API and visible output | PHYS-LINK run through loopback API; same committed frame bytes and session/run/snapshot/frame/device/epoch IDs; mapped UI shows same frame. Fixed behavior label distinguishes PHYSICS_ON/OFF, replay/reload and DISABLED. | PASS |
| Cancel/lifetime | Cancel after actual native host frame; CANCELLED, owned Windows Job empty; descriptor removed on normal window exit. | PASS |

Final ignored evidence is `build/moc3-f3-local/a5fe540e93f860ed437a28cd/`: `evidence.json`, `api-evidence.json`, and per-backend `trace.json`, adapter/memory records and clean frame files. Trace includes fixed timestamps, accepted manual values, actual physics outputs/particle state, geometry checksums and corresponding frame generations. Input hashes and all model-derived data stay ignored; this public report contains none. The final verifier source matches the executed script and the manifest pins it.

The first private descriptor admission rejected normalization with its default at an endpoint. The official format does not mandate a strict positive span around the default. The documented refinement admits ordered endpoint/constant ranges and adds explicit generated tests; inverted normalization still rejects. The failed packet is retained. Numeric weighting, pixel response, temporal response, determinism and memory thresholds were not relaxed.

## Local checks

| Command / check | Observed result |
| --- | --- |
| `cmake --build build/m2-d-private --config Release --target ars_moc3_pixel_bridge ars_moc3_pixel_tests ars_moc3_snapshot_tests ars_moc3_harness_bridge` (targets built in scoped invocations) | PASS / exit 0 |
| `python -m unittest discover -s tests/moc3 -p test_linked_physics.py -v` | PASS / 9 tests, exit 0 |
| CTest F3 generated, T0 API, F1 CPU, F2 process-tree/WARP and legacy dual-mode selection | PASS / 6 tests, exit 0 |
| `ctest --test-dir build/m2-d-private -C Release -R '^moc3_independent_snapshot$' --output-on-failure` | PASS / 1 test, exit 0 |
| `python scripts/moc3_local_physics.py` | PASS / actual WARP + hardware linked physics, exit 0 |
| `python scripts/moc3_local_pixels.py` with extended native bridge | F2 full WARP/hardware regression PASS / exit 0 |
| `python scripts/moc3_verify_visible_f3.py` with visible harness | PASS / exact binary and mapped UI identity, PHYS-LINK terminal PASS, active-render cancellation and empty job, exit 0 |
| Independent read-only review | PASS; generated physics 9/9 and API 9/9 independently rerun; no supported unresolved P0-P2 findings |

Launch `launch_moc3_test_harness.bat`, create a control session, and select **Run F3 physics**. Agents can run `python scripts/moc3_harness_client.py run <session-id> PHYS-LINK`, inspect `result`, `events` and `frame`, or execute `python scripts/moc3_verify_visible_f3.py` while the window stays open. The suite exposes coarse stage/verdict/behavior without private paths or model identifiers. Authorized local binary routes return the actual private clean RGBA or checker presenter, so do not publish their output.

## Remaining boundaries

* Author/Cubism physics or visual equivalence: HOLD for an independent authorized reference trace/render. This adapter uses a declared independent solver.
* Persistent model controls, real-time input leases, motions/expressions/idle arbitration, whole-package interactive reload and long-running GPU performance/soak: HOLD for the next product integration slice. Tested reload recreates the physics engine and repeats the same admitted package; it does not claim whole-package runtime replacement. F1 replacement remains separate evidence.
* Public arbitrary-file import, parser isolation/fuzzing, dependency/redistribution and public activation: HOLD. Optional native targets remain default OFF; no public ABI/capability change.
* Full public CI evaluator path: HOLD until a redistributable MOC3 fixture exists; generated CPU and pixel tests remain public-safe.
* Physical D06/D08 and baseline G00/G01/G02 or other release gates: unchanged. No physical GPU reset or merge/push was performed.

The next integration plan must freeze persistent session/input ownership and GPU timing/soak acceptance before expanding this proof into a continuously interactive product path.
