# ARS v0.1 — Acceptance Gates and Evidence Matrix

**Document:** ARS-GATES-0001 | **Status:** PROPOSED. No gate is currently PASS merely because this document exists.

## 1. Verdict semantics

Each gate has PASS, FAIL, HOLD, or NOT_APPLICABLE. PASS requires reproducible commands, machine/toolchain metadata, exact commit SHA, raw CI logs/artifacts, results, known limitations and reviewer sign-off. FAIL is any violated MUST requirement or failing test. HOLD means evidence is absent, blocked, or capability is not yet implemented; it does not mean PASS. NOT_APPLICABLE only for an explicitly optional feature not advertised in that build. An actual security, ABI, thread or crash gate can never be waived by changing an unsupported-feature label.

Every gate report records: gate_id, PR/HEAD, tested platform, hardware, compiler/driver, command, exit code, artifact URL, conclusion, reviewer, time. Release requires ALL v0.1 baseline gates PASS, no unresolved P0/P1, and a signed release decision. A code-review approval alone never authorizes merge/release.

## 2. Stage gates

| Gate | Objective | Required objective evidence | Failure/hold triggers |
| --- | --- | --- | --- |
| G00 CONTRACT | Freeze semantics for API, model, pose, clock, thread, render, and security | reviewed spec/schema/reference example; ABI prototype and ADRs; Profile B explicit layer_bindings and signed 2D/3D layer-capability disposition | unsigned architectural decisions; incompatible sample/schema or orphan/duplicate 2D layer-to-part mapping |
| G01 BUILD | Reproducible toolchain | CMake clean configure/build C++20; Windows x64 MSVC, Linux x64 Clang/GCC; CTest | hidden setup/SDK binary deps; source build failure |
| G02 ASSET | Import validated user bundles | accepted ARSCHAR + GLB/2D fixtures; JSON Schema validator; corruption corpus | traversal, zip-bomb bypass, unbounded decode, crash |
| G03 RENDER2D | Host can render 2D layers | pixel/reference output, layer hide/show/opacity/Z tests, resize | blank output, wrong composition, leaks |
| G04 RENDER3D | Host can render skinned GLB | test mesh, skin + morph + camera + offscreen readback | incorrect joints/morphs, resource overflow |
| G05 MODE | Switch 2D/3D atomically | 100 dual-mode switches, missing-mode rejection, OOM rollback with unchanged pose | old representation destroyed before new ready; pose reset |
| G06 MOTION | Bounded deterministic real-time motion | replay fixture, layered priorities, dropout and stale/replay tests | queue unbounded, stale source locks rig, nondeterminism |
| G07 THREAD | Documented thread ownership | concurrent ingress/update/render, wrong-thread API, create/destroy stress, sanitizer run | UAF, deadlock, lock inversion, device released off-thread |
| G08 SECURITY | Fail-closed untrusted assets+inputs | fuzz corpus, malicious archive/GLB, limits, external URI denial, negative replay | crash/hang, partial GPU allocation after validation failure |
| G09 PERF | Works across defined quality tiers | 30min soak, reference hardware config, FPS+p95, memory/driver, fallback behavior | undocumented hardware, unsupported feature silently enabled |
| G10 SDK | Embedded consumer works with versioned ABI | plain-C client, C++ wrapper, DLL+static, symbols, CMake find_package, example embed | accidental STL, ABI break, private header dependency |
| G11 RELEASE | Distributable verified v0.1 | reproducible artifacts, SBOM, licenses, docs, API migration, changelog, all-gates evidence | open P0/P1, no license for sample data, undocumented limitations |

## 3. Required negative test matrix

| Test ID | Action | Expected |
| --- | --- | --- |
| SEC-001 | ZIP ../, absolute, Windows drive/UNC, Unicode/case alias | ARS_E_BAD_ASSET; no path created outside staging |
| SEC-002 | inflate >512 MiB, huge entry count, malformed ZIP central directory | early bounded rejection; no OOM |
| SEC-003 | GLB accessor overflow, negative/huge index, invalid skin joints | ARS_E_BAD_ASSET; no GPU upload |
| SEC-004 | Texture dimension exceeds cap, truncated PNG, malicious decode sizes | early reject; bounded RAM |
| SEC-005 | External http/file/data URI, plugin/shader payload | denied by default; zero outbound connections |
| SEC-006 | NaN/Inf, huge frame channel list, duplicate IDs, invalid quaternion | rejected frame; character still animates |
| SEC-007 | Old generation with plausible timestamp/sequence after reconnect | rejected; active channel ownership unaffected |
| SEC-008 | Out-of-order/replayed/future motion sample | rejected or bounded reorder; pose generation monotonic |
| SEC-009 | Flood 100x frame throughput from 16 sources | queue bounded; backpressure/drop counters increment |
| THR-001 | submit from 4 workers while update/render run | deterministic pose, no race/UAF under sanitizer |
| THR-002 | call render on wrong thread, destroy while render pending | typed WRONG_THREAD/BUSY; no unsafe free |
| THR-003 | simulated device loss during mode switch/texture upload | rollback or recover; no orphan GPU references |
| MODE-001 | request unsupported mode while active character is moving | unchanged mode, pose, resource counts |
| MODE-002 | 100+ repeated switches and 1000 create/destroy cycles | no growth of live handles and no leaks |
| API-001 | old struct_size or incompatible abi_major | clear negotiated support or rejection, no memory overwrite |
| API-002 | host callback reenters API | no lock deadlock; clearly documented BUSY/error semantics |

## 4. Render and performance acceptance protocol

Benchmark scene manifests MUST pin the exact asset hash, window/offscreen size, GPU driver, OS, CPU/GPU, selected renderer/backend, thermal/power settings, build type, vsync policy and sampling interval. Profile changes must be logged.

**Initial targets (subject to reference hardware selection at G00):**
- LOW: 1 native 2D character, typical 16 visible layers, 720p, >=30 FPS average steady, p95 update+render CPU frame cost <=33.3ms and bounded heap/VRAM.
- BALANCED: 1 3D character with 50-100 bones and a few morphs, 1080p, >=60 FPS average steady, p95 update+render CPU frame cost <=16.7ms on a selected supported mid-range GPU.
- Soak: 30min after 5min warm-up, no monotonic live-handle growth, and investigate >64 MiB additional working-set or >32 MiB new retained GPU allocations.
- Load/switch 100 times, run 10,000 synthetic motion frames, resize 100 times, drop input for 2 sec and reconnect with new generation.
- If targeted hardware cannot meet a profile, downgrade only through declared caps/quality profile; never invent achieved FPS. Hardware matrix MUST include a real low-end integrated GPU before advertising low-end support.

The frame budget is not a substitute for precise GPU-timer, camera-to-pose latency and host present timing. Record separately whenever supported.

## 5. CI baseline and commands (once source exists)

Windows:
- cmake -S . -B build/windows -A x64 -DCMAKE_BUILD_TYPE=Release
- cmake --build build/windows --config Release
- ctest --test-dir build/windows -C Release --output-on-failure

Linux:
- cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Debug -DARS_ENABLE_SANITIZERS=ON
- cmake --build build/linux -j
- ctest --test-dir build/linux --output-on-failure

CI matrix: Windows (MSVC), Ubuntu (Clang+ASan+UBSan), Ubuntu (GCC release). Native GPU jobs use a dedicated compatible runner; do not mark GPU visual tests PASS because the core compiled on a headless runner. Package and manifest validators should run on both Linux and Windows. Include a cached fuzz corpus that can run quickly in CI; longer fuzzing runs on scheduled/manual jobs.

## 6. Workstream dependencies and stage authorization

- M0 -> G00 design-only Profile B schema/reference mapping proof before production backend/public format implementation. Actual validated PNG atlas dimensions and renderer ordering/opacity require later G02/G03 evidence; these are NOT G00 runtime PASS.
- M1 -> G01, G02 and render context proof; no runtime networking.
- M2 -> G03, G04, G05; mode switch cannot be called ready before both required renderers exist.
- M3 -> G06, G07, G08; no camera adapter can bypass normalized ingestion.
- M4 -> G09, G10, G11; publish release only after empirical evidence.

Scope changes MUST declare impacted gates and rerun them on the new exact HEAD. Claims of PASS without raw logs/fixtures and reviewer are HOLD.
