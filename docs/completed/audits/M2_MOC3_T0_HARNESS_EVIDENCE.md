# M2 MOC3 T0 synthetic harness evidence

**Phase:** implementation verification, 2026-10-18. **Base HEAD:** `472aa1624a70710b8f2498dcd6d2e20fa7fe64b4`; T0 files were uncommitted at test time. **Environment:** Windows 10 x64, Python 3.12, Tk 8.6, CMake Visual Studio 18 2026 generator. This packet concerns the synthetic harness only. It contains no model-derived data.

| Requirement | Command / observation | Result and proof tier |
| --- | --- | --- |
| T0 API auth, idempotency, invalid input, event cursor, cancellation and typed HOLD | `python -m unittest discover -s tests/moc3 -p test_harness_api.py -v` | PASS: 5 HTTP tests; `synthetic_harness`. |
| Existing P0 comparison unaffected | `python -m unittest discover -s tests/moc3 -p test_compare_snapshots.py -v` | PASS: 8 tests; existing comparison fixtures only. |
| Python syntax | `python -m py_compile scripts/moc3_test_harness.py scripts/moc3_harness_client.py tests/moc3/test_harness_api.py` | PASS. |
| CMake registration | `cmake -S . -B build/moc3-t0-verify -DARS_ENABLE_PREP_TESTS=ON` | PASS: prep-only configure. |
| CTest registration | `ctest --test-dir build/moc3-t0-verify -C Debug -R moc3_t0_harness_api --output-on-failure` | PASS: 1/1 labeled `MOC3_T0_SYNTHETIC;NON_PRODUCTION`. An initial invocation without `-C Debug` was NOT RUN because this is a multi-config generator; the corrected command passed. |
| Agent CLI against a separate running process | Bounded headless service; client `status`, `create`, `step`, `run PHYS-LINK`, `run T0-SYNTHETIC`, `result`, `events` | PASS: client observed a synthetic run and `PHYS-LINK=HOLD_F3_NOT_CONNECTED`. Local process exited and descriptor was removed. |
| Agent API while the visible window is open | Bounded `--window --duration-seconds 8` process; client `create`, `run T0-SYNTHETIC`, `result` | PASS: `VISIBLE_WINDOW_AGENT_API_PASS SYNTHETIC_ONLY`; shared session completed while Tk process was active. No visual model frame was produced or inspected. |
| Window/launcher smoke | `python scripts/moc3_test_harness.py --window --duration-seconds 2`; `cmd /c launch_moc3_test_harness.bat --duration-seconds 2` | PASS: Tk and batch launcher paths started and exited with code 0. This is process evidence, not a visual inspection of real model pixels or manual button behavior. |

**Current verdict:** T0 API contract and synthetic status window implemented. `ADMIT`/`POSE-REPLAY` remain `HOLD_F1_NOT_CONNECTED`; `PIXEL-2D` remains `HOLD_NO_HOST_RENDERER`; `PHYS-LINK` remains `HOLD_F3_NOT_CONNECTED`; actual D06/D08 loss remains HOLD. The current window displays synthetic events only. There is no real MOC3 frame, Physics3 trace or GPU readback. Windows token-descriptor ACL and all model-input security boundaries require review before F1 hookup. A green T0 test or HTTP response does not promote G00-G05 or public capability.
