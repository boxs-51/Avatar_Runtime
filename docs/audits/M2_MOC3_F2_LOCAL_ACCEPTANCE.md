# F2 trusted-local host pixel acceptance

**Verdict: PASS for the bounded F2 contract**, on the [exact source and binaries](M2_MOC3_F2_LOCAL_SOURCE.json), parent HEAD `472aa1624a70710b8f2498dcd6d2e20fa7fe64b4`, with an uncommitted worktree. Actual Windows local execution is primary evidence; GitHub Actions was not used to award this result. Optional MOC3 support remains default OFF. This report contains no private model images, identifiers, hashes, counts or measurements.

## Direct evidence

| Boundary | Actual local witness | Result |
| --- | --- | --- |
| Pixel conventions | Generated numeric readbacks: multiple atlases, V flip, clamp/bilinear alpha edges, premultiplication, order/ties, multiply/screen colors, normal/additive/multiply, mask union/inversion, culling/visibility and transparent clear. Tolerance frozen at 3 color byte levels. | PASS |
| Driver repetition | Same generated fixtures on D3D11 WARP and NVIDIA GeForce GTX 1060 3GB; debug layer available on both; zero warning/error verdict. | PASS |
| Host isolation | Restores touched graphics state, including hostile geometry shader/predication and resource slots. Wrong-thread/device/generation and invalid indices/atlas/masks reject before submission. | PASS |
| Ownership | Exact query pins CPU snapshot/atlases and GPU resources; pending resize does not publish; stale target epoch rejects; close drains. Every declared GPU creation checkpoint rolls back published output. Throwing callbacks retain prepared queries, including null CPU pins. | PASS |
| Atlas residency | Warm immutable package reuses atlas textures with identical pixels; package mismatch makes zero GPU creations; destroying cache while pending retains frame resources until exact retirement. | PASS |
| Actual evaluator-to-host path | Admitted immutable package feeds native evaluator and the existing dual-mode host target. Fixed default/change/reset/repeat readbacks are nonblank, change on parameter input and repeat exactly after reset. Both WARP and hardware run. | PASS |
| Window/API identity | Visible Tk presenter displays the same completed host readback as authenticated binary routes. Metadata, clean RGBA bytes, checker PPM bytes, session/run/snapshot/frame/device/epoch identities match. Shared event sequence contains each expected committed frame. | PASS |
| Cancellation | Cancel after a real host frame while the native renderer is active; terminal CANCELLED and owned Windows Job reports no live processes. Separate real descendant test confirms job cleanup. | PASS |
| Memory | Final direct and visible runs pass the unchanged conservative combined Python/native lifetime peak commit limit of 1536 MiB. Detailed measurements remain ignored. | PASS for bounded replay |

The first visible acceptance attempt failed its hardware memory gate. It was retained as a failed packet, without raising the limit. Diagnostic runs identified near-limit native residency; the renderer previously recreated atlas textures on every frame. The final implementation reuses one session-owned immutable atlas set. Fresh direct and visible/API runs after this change pass the same limit with additional headroom. This is a measured local acceptance check, not an OS memory sandbox or continuous-soak proof. No blind retry turns a failed packet into PASS.

The final ignored packets are under `build/moc3-f2-local/adb9261f851746eb8b4d0c7d/` (direct replay) and `build/moc3-f2-local/8c55e2ffdda4ed7f4b936e4e/` (visible/API replay, including `api-evidence.json`). They retain source/binary provenance, actual backend, clean images, memory and cancellation evidence. Neither these packets nor their private hashes/metrics may be published. Local visual inspection confirms a populated character render; authored appearance equivalence remains unverified without an independent reference.

## Reproduction and regression

Configure the existing local dependency cache with `ARS_BUILD_MOC3_INDEPENDENT=ON`, `ARS_BUILD_M2_D3D11_DUAL=ON` and `ARS_BUILD_MOC3_D3D11=ON`, plus the pinned PurismCore source root. Build `ars_moc3_pixel_tests`, `ars_moc3_pixel_bridge` and `ars_m2_dual_pixels` in Release. Both new executables are under `build/m2-d-private/bin/Release/` in this configuration.

| Local command | Result |
| --- | --- |
| `cmake --build build/m2-d-private --config Release --target ars_moc3_pixel_tests ars_moc3_pixel_bridge` | PASS / exit 0 |
| `ctest --test-dir build/m2-d-private -C Release -R 'moc3_f2_warp_pixels\|moc3_f2_process_tree\|m2_warp_dual_mode' --output-on-failure` | PASS / 3 tests, exit 0 |
| `python scripts/moc3_local_pixels.py` | PASS / WARP + hardware + actual package, exit 0 |
| `python -m unittest discover -s tests/moc3 -p test_harness_api.py -v` | PASS / 9 tests, exit 0; includes binary auth, invalidation and repeated counters across runs |
| `python -m unittest discover -s tests/moc3 -p test_package_admission.py -v` | PASS / 11 tests, exit 0 |
| `python scripts/moc3_local_replay.py` | F1 regression PASS / exit 0 |
| Visible harness + local API verification | PASS / full byte identity, mapped UI identity, active-render cancellation and empty owned process tree |
| Independent read-only review | PASS after all four findings and atlas residency extension; no supported unresolved P0-P2 findings in reviewed scope |

Open `launch_moc3_test_harness.bat`, then choose **Run F2 pixels**. Agents can create a control session using `scripts/moc3_harness_client.py`, run suite `PIXEL-2D`, inspect events/results and request `frame <session-id>`. Authenticated routes are `GET /v1/sessions/{id}/frames/latest`, `/image` (checker presenter), and `/rgba` (clean raw RGBA8). Binary responses include all identity headers; clients must compare them with metadata because counters can restart in another run. No frame is available before an actual committed readback. The descriptor/token is local and removed on shutdown.

With the window open, `python scripts/moc3_verify_visible_f2.py` reproduces the final agent check: full suite, exact API/readback bytes and visible frame identity, then active-render cancellation. Its executable source is the same as the ignored verifier used for the final packet; the public source manifest pins that copy. The window must remain open for both runs. Evidence remains ignored and no token is printed.

## Explicit remaining boundaries

| Item | Disposition / evidence required |
| --- | --- |
| F3 linked Physics3 and parameter/clock arbitration | HOLD: implement linked data execution, deterministic on/off/release/reload geometry and host pixel traces. No physics claim from F2 parameter changes. |
| Authored visual fidelity | HOLD: rights-cleared independent renderer/reference at identical pose/view/color conventions. |
| Persistent interactive model session and continuous performance | HOLD: current control session is synthetic between suite runs; bounded F2 worker supplies actual frames during PIXEL-2D. Free live stepping, input leases, A/B inspector, arbitrary resize API, timing/soak and physics UI remain later hookups. |
| Public arbitrary-file import and activation | HOLD: isolation/fuzz/licensing, integration and explicit activation decision. Advanced/offscreen MOC blends remain unsupported and rejected. |
| Redistributable real MOC3 CI fixture | HOLD for full public-CI evaluator path; does not invalidate authorized local proof. Generated pixels remain public-safe. |
| D06/D08 physical device loss | HOLD: disposable reset-capable Windows GPU runner; no physical reset performed here. |
| Baseline G00/G01/G02 and other product gates | Unchanged; F2 is a private technical proof and cannot close these gates by itself. |

No commit, push or merge is part of this acceptance.
