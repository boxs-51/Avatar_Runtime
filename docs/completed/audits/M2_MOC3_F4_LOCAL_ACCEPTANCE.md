# F4-A real core to host: local acceptance

**PASS for the private integration slice; whole F4 product/activation remains HOLD.** Date: 2026-10-19 (Asia/Bangkok). Parent `69eac34`, combined F3/F4 worktree remains uncommitted. [Source/binary/dependency manifest](../../reference/audits/M2_MOC3_F4_LOCAL_SOURCE.json), [frozen design](../M2_MOC3_F4_INTEGRATION_DESIGN.md), [gate and custody ledger](../../in-progress/M2_MOC3_F4_RELEASE_LEDGER.md). Local evidence is primary; no CI workflow is used to award this result.

## Direct observations

| Requirement | Direct witness | Verdict |
| --- | --- | --- |
| F4-01 actual core | RuntimeRouter creates the private runtime/Character, registers a host-scoped source using native security clock and OS entropy, and stages/publishes through existing SourceAuthority/PublishGate. No fixture clocks, fabricated snapshots or new mixer are used in this consumer. | PASS |
| F4-02 binding/input | Bridge maps only actual native catalog entries to bounded semantic channels. Complete input releases omissions to native defaults. Unknown/duplicate/out-of-range and replayed frames return typed errors and preserve current core generation. | PASS |
| F4-03 commit order | Core candidate -> linked Physics3 prepare -> actual MOC snapshot -> exact host GPU query -> final core publish -> physics commit -> advertised packet. A target completed privately after late source revocation is rejected by the final core fence and never advertised. Old accepted core/physics/image identities remain. | PASS |
| F4-04 internal copy | Candidate-copy returns only bounded scalars and next generation. Empty/foreign-router/wrong-size copies reject without publishing; valid copy does not bypass final authorization. Real native unit test also covers pending visibility, release, owner thread, replay and late revoke. | PASS |
| F4-05 bounded integration | 120 paced updates plus 16 reset/replay and four physics-toggle frames on WARP and GTX 1060 3GB. Actual release changes geometry and pixels over time; reset reproduces first-cycle readbacks. Disable changes both geometry and pixels while retaining manual input; re-enable reproduces the first rest-state update. Disabled release exactly matches a separately rendered native-default pose. Only two image slots persist; unchanged 1536 MiB combined peak acceptance passes. Timing data is recorded without an invented FPS success threshold. | PASS for this bounded workload |
| F4-06 API/window | Same accepted frame includes core/snapshot/frame/device/epoch identity, behavior and exact immutable bytes. All 140 accepted frame events are consumed with no cursor gap; rejected private target 141 does not replace the last accepted frame. Live native/core cancellation ends CANCELLED with empty owned Windows Job. | PASS |
| F4-07 decision packet | Gate/custody ledger explicitly preserves SDK/public-input/performance/physical-loss HOLDS and does not advertise a capability. | Prepared; product verdict HOLD |

The new native product bridge is a separate default-OFF target. The existing F2/F3 bridge and public `include/ars/ars.h`/ABI sources are preserved. Internal candidate-copy is not a public export. The two-slot producer is synchronous: Python copies bytes before another native render command. Self-contained image packets carry those copied bytes to the coordinator, avoiding an asynchronous file-overwrite race. Transport has a 3 MiB encoded line limit, 2 MiB decoded limit, exact target/identity checks and a two-message backpressure queue. Tokens and model-specific identifiers never appear in frame metadata.

Independent review found a malformed-JSON negative path: a deeply nested bounded header could raise RecursionError outside the coordinator's typed failure handling. The decoder now normalizes malformed/deep/invalid-encoding JSON to ValueError, with a generated regression. Review also found missing core-path physics-toggle coverage; the integrated toggle/default/reset checks now execute on both renderers. No pixel, temporal or memory thresholds were weakened. Independent final code review reports no supported unresolved P0-P2 findings; architecture and project structure assessments are supported.

## Evidence and checks

Ignored final visible packet: `build/moc3-f4-local/dbda7bbc94e0e6e64b02fc4c/`, including actual WARP/hardware trace/memory/adapter records, `evidence.json` and `api-evidence.json`. Detailed model-derived timings, inputs, checksums and pixels stay local. Compiler: MSVC 19.51.36252.0, Windows x64 Release; physical adapter GTX 1060 3GB, driver 581.57. Do not export the private packet.

| Check | Result |
| --- | --- |
| Configure with `ARS_BUILD_MOC3_F4=ON`, `ARS_BUILD_RUNTIME_CORE=ON` and existing optional MOC3/D3D11 prerequisites | PASS |
| Build `ars_moc3_product_bridge` and `ars_moc3_core_consumer_tests` | PASS / exit 0 |
| Real-core consumer CTest | PASS / 1 test, including new bounded candidate-copy controls |
| Frame transport unit suite | PASS / 4 tests, including exact roundtrip, identity/size, bomb/truncation/trailing stream and deep JSON |
| Harness API suite | PASS / 10 tests; generated wire fixture is explicitly not an actual renderer proof |
| CTest private core/public C/C++/source-authority/nine publish-race families/F4 selection | PASS / 14 selected tests, exit 0 |
| CTest affected F1/F2/F3/F4/API/native-snapshot/legacy-host/runtime-lifecycle regression selection | PASS / 10 selected tests, exit 0; overlaps some F4 checks above |
| Actual direct `python scripts/moc3_local_integration.py` | PASS / WARP and hardware, exit 0 |
| `python scripts/moc3_verify_visible_f4.py` with mapped window | PASS / exact binary/core/UI identity, complete monotonic event stream, late-revoke nonpublication and active native/core cancellation, exit 0 |
| Final harness cleanup | PASS / bounded mapped window exit 0, transient session descriptor removed, no product bridge process remains |
| Independent verification | Packet 4/4, native consumer 1/1 and API 10/10 independently PASS; full F4-A source review PASS |

Open `launch_moc3_test_harness.bat`, create a control session, then select **Run F4 core**. Agents can run suite `CORE-INTEGRATION` using the existing client or invoke the visible verifier. The global control session remains synthetic between bounded suite runs; this is not a persistent external character-control API. The native worker, core source and host are persistent within the integration run.

F4 whole-product closure still requires the explicit work listed in the ledger. G09 30-minute/tier acceptance, full SDK embedding/packaging, untrusted parser qualification and actual D06/D08 are not closed by this result. No commit/merge/push or public activation occurs in this turn. An initial launcher omitted `--window`; the verifier correctly rejected it before starting a suite. The bounded headless instance closed normally; the final packet above comes from the corrected mapped-window run.

F1-F3 source manifests remain historical tested states; the F4 manifest pins the current combined closure. Later source changes do not retroactively alter their evidence.
