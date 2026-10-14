# M1 Official Source Kickoff Plan — v0.1 (PREPARED, NOT AUTHORIZED)

**ID:** ARS-M1-BOOTSTRAP-PLAN-RC0  
**Stack:** this prep branch -> Draft PR #2 @ 16abfb058a90f90f4ed89f6b6ced2000bff0b30f -> Draft PR #1 -> main  
**Status:** \`PRE_CLAIM\` / independent G00 and owner signoff required; no production code, no merge.  
**Gate dependency:** \`G00 CONTRACT\` before **M1 production implementation**; G01 BUILD/G02 ASSET and real security gates assessed only after executable code exists.

## 1. Proposed implementation branch topology

| Stage | Proposed child branch / separate Draft PR | Exact prospective source ownership | Dependency | Required evidence |
| --- | --- | --- | --- | --- |
| M1-A BUILD | \`m1/a-build-foundation\` | root CMakeLists.txt; \`cmake/**\`; \`include/ars/**\` only after ABI ratified; \`tests/abi/**\`; \`.github/workflows/m1-build.yml\`; \`examples/plain_c/**\` | G00 ABI contract + build toolchain agreement | Linux GCC/Clang C++20, Windows MSVC x64, plain C linked consumer, CTest G01 |
| M1-B REGISTRY/AUTH | \`m1/b-runtime-authority\` | \`src/core/handle_registry.*\`, \`src/core/runtime_lifecycle.*\`, \`src/core/security_clock.*\`, \`src/motion/source_registry.*\`, \`src/motion/admission_gate.*\`, \`src/motion/pose_snapshot.*\`, \`tests/security/**\` | M1-A + accepted R5 + ABI ID/ownership decisions | real production HDL-001..019, SRC authorization cases and RACE-L..W before opening native source grants |
| M1-C VALIDATOR | \`m1/c-asset-validator\` | \`src/assets/**\`, \`src/validation/**\`, \`tests/assets/**\`, \`fuzz/**\`, fixture manifests/schema edits only with format review | G00 schema/semantic format decision + M1-A | schema Draft 2020-12 + semantic validation, ZIP budgets/path safety, GLB index bounds, corrupted fixture corpus, zero GPU side effects |
| M1-D GPU SPIKE | \`m1/d-d3d11-interop-spike\` | \`src/graphics/d3d11/**\`, \`src/graphics/backend.*\`, \`tests/gpu/**\` | G00 ADR-0001 + M1-A, no conflict with M1-B GPU pins | real Windows D3D11 device, target/format/state restore, resize/device-loss, R-thread retire behind query/fence, raw GPU evidence |

**Scope rule:** Paths are a **proposed manifest**, not a reservation/claim or permission to edit all of \`src/**\`. Each owner must pin main/base HEAD, actual exact paths and PR review, including test files. No shared file edits across concurrent owners without explicit dependency/serialization.

**Merge and authorization:** Stacking PRs is for review; no stage automatically merges when CI turns green. A written authorizer decision is required. Owner self-audit does not count as independent reviewer signoff.

## 2. M1-A (first actual coding) — concrete start procedure

1. Resolve G00 blockers in the freeze ledger. Pin the exact accepted candidate HEAD and exact compiled public ABI header. Do not take \`_p0\` proposal for a stable shipping header.
2. Open a dedicated **M1-A Draft PR** with an exact source manifest. Build a CMake C++20 project with a static and optionally shared library, hidden symbols by default and explicit \`ARS_API\` exports. Do not export engine code from the test stub.
3. Export at least ABI version/capabilities negotiated in the accepted public header. Add a **real linked** plain-C consumer with startup/shutdown errors, exact struct size/version checks, C++20 compile and module visibility checks. Fail unsupported functionality explicitly.
4. Build and run: Linux Ubuntu x64 GCC/Clang (warnings-as-errors in CI), Windows MSVC x64; CTest with clean build; ASan/UBSan at least CPU foundations if implementable, separate TSan runner when multithreaded code arrives.
5. Cross-check all symbols on Linux/Windows (nm/dumpbin) and user-defined struct layouts. A stub symbol list only proves linkage, not runtime semantics.
6. Declare G01 BUILD only with actual code evidence at the M1-A HEAD and independent review.

**Development commands once M1-A is implemented:**

~~~sh
cmake -S . -B build/linux -DARS_ENABLE_PREP_TESTS=ON -DARS_PREP_ONLY=ON
cmake --build build/linux --parallel
ctest --test-dir build/linux --output-on-failure
~~~

~~~powershell
cmake -S . -B build/windows -A x64 -DARS_ENABLE_PREP_TESTS=ON -DARS_PREP_ONLY=ON
cmake --build build/windows --config Debug
ctest --test-dir build/windows -C Debug --output-on-failure
~~~

Those commands in this prep PR run **nonfunctional contract smoke/model tests only**. Actual M1-A will replace this constrained bootstrap after reviewed public API/ABI contract.

## 3. M1-B security architecture implementation sequence

1. Implement process-local nonreused runtime UID router; typed by-value 16-byte handles and slot generation; pin/unpin RAII; never dereference supplied handle as raw address.
2. Implement runtime structural state, short commit gate, close-management leases, source/character retirement; no GPU cleanup off R.
3. Implement writer-closing admission barrier with sticky CLOSE_REQUESTED and bounded readers; check that admitted operation always releases reader lease even after exception paths.
4. Implement security clock provider with runtime-wide high-water/poison; Linux monotonic and Windows QPC or appropriate OS monotonic source. Inject mock clock to test regression, fail and wrap.
5. Implement trusted source registration, fixed scope, real OS CSPRNG 128-bit token and lifetime nonreuse ledger. No remote packet identity directly used as capability.
6. Per-source atomic epoch/sequence/quota/queue ingress; full parsing/validation outside short critical section; source grant checks repeated before queue commit.
7. Implement terminal EXPIRED/EXPLICIT_REVOKED/EXHAUSTED, rotation ticket compare-and-commit and release tombstones; no privileged fallback/re-registration from an expired registration.
8. Simulation single-writer retained-state lineage, acquire-checked tombstones, staged mix + publish gate commit, no revoked/expired source contributing through cached smoothing/trigger/retarget.
9. Add and **actually run** the 19 HDL + 45 SRC + 23 RACE production-negative gates, or explicitly track not-yet-runnable cases as HOLD with exact reasons. Run sanitizers and cross-platform raw evidence.
10. If security/clock/lock rules change, update ADR and obtain fresh independent re-audit before proceeding.

## 4. M1-C / model ingestion

- A hostile ZIP/GLB/JSON must never reach GPU parser/allocation before validated CPU structures are produced.
- Keep optional capability flags distinct from accepted/implemented formats; \`vrm1\` stays disabled until its importer is validated.
- Required negative corpus: traversal, alias/duplicate paths, decompression caps, unsafe URI, out-of-bounds triangles/indices, bad UV/vertex/delta lengths, duplicate semantics, bool-default misuse, bad defaults, unsupported required extension.
- Record reproducible corpora, hashes, decoder dependency licenses, max bytes/entries/image dimensions and allocation ceilings in test data.

## 5. M1-D graphics contract prerequisites

- Resolve ADR-0001: HOST_DEVICE vs SDK_DEVICE, immediate-context rules, D3D11 state isolation, target lifetimes, resource retirement/completion, reset/resize, lost device, render thread.
- If a dedicated GPU runner is not available, mark that evidence HOLD instead of claiming compile-only GPU PASS.

## 6. Planned integration/release gates

\`M0 G00\` DESIGN -> \`M1-A G01\` BUILD -> \`M1-B G07/G08 source security\` (partial, non-GPU) + \`M1-C G02\` asset validation -> \`M1-D GPU ADR proof\` -> M2 2D/3D rendering -> M3 motion -> M4 G09/G10/G11 and release.

**No stage may claim SDK v0.1 shipped merely because an implementation branch has been created.**

## 7. Next authorized actions

- **Now without G00:** review this plan, compile the prep-only CMake model/stub and examine outstanding ADR/schema/ABI decisions.
- **Only after independent G00 DESIGN PASS:** take exact M1-A source manifest and begin real C++20 engine implementation.
- **Only after per-stage evidence:** authorize integration; no automatic merge.
