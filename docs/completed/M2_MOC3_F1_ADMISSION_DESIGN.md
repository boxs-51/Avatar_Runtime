# M2 MOC3 F1: package admission and evaluator session design

**Status:** F1 trusted-local CPU acceptance PASS; host pixels, linked behavior execution and public activation remain separate gates. **Parent contract:** [M2 MOC3 framework target design](../in-progress/M2_MOC3_FRAMEWORK_TARGET_DESIGN.md), stage F1. The [agent-callable test harness](../in-progress/M2_MOC3_OBSERVABLE_TEST_HARNESS_DESIGN.md) supplies `F1-LOCAL-REPLAY` as the current admission/pose test executor; persistent host model-session routes are a later hookup. **Source baseline:** `src/moc3/independent_snapshot.hpp` and `.cpp` on `m2/moc3-p1-snapshot`. This design keeps `ARS_BUILD_MOC3_INDEPENDENT` default OFF, uses no official Live2D SDK, and does not modify the public ABI, ARSCHAR schema or existing ARS2D budget.

## F1 output and definition of done

**Owner clarification, 2026-10-18:** actual runs on this Windows machine with the authorized local model are the primary F1 acceptance evidence. GitHub Actions is a corroborating regression source. A missing redistributable MOC3 fixture limits public CI coverage; it does not block the trusted-local F1 verdict. Public arbitrary-file import and F2/F3 remain separate scopes.

The frozen local policy is: manifest/linked descriptor 1 MiB each; MOC 16 MiB; at most 16 textures, encoded total 128 MiB, decoded total 512 MiB, side at most 8192; conservative simultaneous Python package/decode buffers at most 768 MiB; native model 64 MiB and snapshot payload 128 MiB, streams per `Budget`. One committed package/worker and one replacement candidate may coexist; one diagnostic snapshot bookmark may be pinned in a worker. The conservative sum of the Python process lifetime peak commit and the largest simultaneously live native-worker peak commits must be at most 1536 MiB. This is a measured local acceptance ceiling, supplemented by preallocation logical limits; it is not an OS sandbox memory guarantee. Freeze these values before the new representative run and retain actual Windows peak counters in ignored local evidence.

F1 uses raw encoded RGBA8 sample values without color conversion. Required PNG forms outside RGB8/RGBA8 noninterlaced input reject. Color-profile interpretation and host target color space belong to F2's pixel contract. Optional linked behaviors are retained/reported as disabled; requesting their execution rejects before commit.

F1 yields a **private CPU-only transaction**: given an explicitly selected local `.model3.json`, a root and a resource policy, it either returns a complete, immutable `AdmittedMocPackage` plus a ready evaluator session/default snapshot, or returns a typed error with the previously committed package and snapshot unchanged. The package lists exactly the manifest-linked MOC, ordered textures and selected linked behaviors. It contains no GPU object and causes no upload. F1 does **not** claim host pixels, Physics3 execution, author-intended fidelity or public arbitrary-file safety.

The [official manifest format](https://github.com/Live2D/CubismSpecs/blob/master/FileFormats/model3.json.md) makes `Moc` and `Textures` required references and Physics, Expressions and Motions optional references. F1 admits only MOC and textures for activation; it records a linked Physics3 descriptor for F3 without executing it. Linked expressions/motions remain explicitly unsupported until their own policy and parser exist. Unreferenced neighboring files have no effect. If an optional linked feature would change the requested behavior but is not implemented, admission either rejects that requested feature or records it as disabled in an explicit capability report; it never silently treats it as active.

## Contract sketch and phase boundaries

The names below describe **private interfaces**; implementation may adapt them to local conventions without changing semantics.

```text
AdmitRequest {root, relative_manifest, requested_features, BudgetPolicy}
  -> Result<AdmittedMocPackage, AdmitError>
AdmittedMocPackage {manifest_generation, manifest_digest, moc_blob,
                    texture_records[index -> validated image/metadata],
                    linked_resource_descriptors, feature_report, byte_accounting}
OpenEvaluator(AdmittedMocPackage) -> Result<Session + default DrawableSnapshot, EvalError>
UpdateEvaluator(Session, complete ParameterFrame) -> Result<DrawableSnapshot, EvalError>
```

1. **Reference discovery.** Parse the bounded manifest before following paths. Validate the declared version/known fields, reference type, number, UTF-8/path encoding, duplicate aliases and texture ordering. A drawable's texture index must resolve to exactly one admitted texture in this order. Do not infer textures from directory order or file names.
2. **Contained file reads.** Resolve each declared relative path beneath a canonical local root; reject absolute, drive/UNC, empty, `..`, alternate stream, unexpected extension, symlink/reparse escape and duplicate canonical targets. For a future untrusted-input mode, verify the actual opened file handle remains within the root and is not swapped between check and read; if the platform cannot make that guarantee, that mode remains disabled. The trusted-local F1 path still checks canonical containment and rejects ambiguous aliases.
3. **Resource preflight and decode.** Enforce separate encoded bytes, image dimensions/pixels, total decoded bytes, model/evaluator state, snapshot and transient-work ceilings with checked add/multiply. Decode each referenced PNG to a validated immutable image with declared color/alpha layout. Keep texture selection and ownership tied to its manifest index. Reuse `assets::decode_png` only after its limits, allocator and image lifetime are mapped to this separate policy; do not change global ARS2D limits.
4. **Evaluator preparation.** Validate MOC version/consistency and supported feature inventory before exposing the session. Check parameter IDs/defaults/min/max, drawable IDs, stream bounds, masks and referenced texture indices. Reject required offscreen or blend semantics outside the approved F2 feature subset. Construct the default snapshot using the same bounded update path as later frames. Publish package, evaluator and default snapshot together only after all steps succeed.
5. **Steady update.** The caller supplies a **complete** parameter frame or an explicit delta policy; F1 must freeze which one is chosen. This design recommends a complete frame: start from recorded defaults, apply one checked value per ID, run one model update, copy the full immutable snapshot, then publish. Repeating a frame must reproduce the same output; an invalid update or injected allocation failure must retain the previous snapshot and leave the session at a known replayable state. Generation increments only on successful publication.

The existing `Session::open(root, relative_moc, Budget, out)` may remain as a low-level private probe. A new package boundary should own manifest and image admission; passing a bare `.moc3` to that probe does not mean textures or linked resources were validated. Avoid exposing a second informal loader path to F2. The existing `Result` enum may need private typed distinctions for malformed manifest, missing reference, unsupported requested feature and budget failure; preserve typed public errors only if a later material ABI decision authorizes them.

## Memory, concurrency and rollback

The package owner retains file bytes and decoded images; the evaluator owner retains aligned MOC/model memory; a published snapshot owns copied drawable arrays; the renderer may later pin package/image/snapshot independently. Account for all four plus parser scratch, PNG decoder scratch and simultaneous old/new candidates. A per-field logical size estimate is insufficient if allocator capacity or in-flight generations exceed it; capture allocator peaks during negative tests. Failure at any allocation checkpoint must destroy the candidate on the owning thread while leaving the old committed state readable.

H/control chooses the package and serializes replacement; A/CPU worker performs read/parse/decode without D3D11 calls; S owns evaluator update and publication; R later consumes only immutable data. A handoff from A to S requires an explicit quiescent point and generation token. No callback under model/snapshot locks, and no renderer dereference into mutable evaluator memory. Wrong-thread update and stale package generation reject before model mutation. A failed evaluator rollback that cannot prove its internal state is restored marks that candidate session unusable; the previous committed session/snapshot remains active.

Private diagnostics record only typed reason, stage and bounded counters. Model bytes, path, drawable/parameter IDs, hashes, screenshots and detailed measurements from nonredistributable input stay in ignored local evidence. Public CI uses generated or redistributable fixtures only.

## Feature and test matrix to freeze before code

| Case | Expected F1 result | Required witness |
| --- | --- | --- |
| Minimal linked MOC + one texture | Accepted; texture index maps to declared slot; default snapshot immutable. | Rights-cleared redistributable MOC3 if available, otherwise opt-in private evaluator run; exact manifest graph and repeatable snapshot. Public manifest mocks alone do not prove MOC evaluation. |
| Two declared textures with reversed file-name order | Accepted in manifest order only. | Index-to-image assertions; no directory enumeration. |
| Linked Physics3 requested as data only | Descriptor admitted, execution explicitly disabled until F3. | Capability report and no physics update in F1. |
| Unreferenced expression/motion sidecar | Ignored. | Same package graph and output with/without neighbor file. |
| Requested unsupported linked feature | Typed unsupported before commit. | Old package/snapshot/generation unchanged. |
| Missing texture, duplicate/case alias, path escape or file swap | Typed reject before evaluator creation/upload. | No outside read and no GPU call; future public mode needs handle-bound proof. |
| Corrupt MOC, PNG bomb, nonfinite parameter, invalid index or unsupported drawable feature | Typed reject; no partial snapshot. | Negative fixture and budget peak. |
| Allocation failure at each parse/decode/evaluator/snapshot point | Typed failure; exact prior state retained or candidate marked unusable. | Fault checkpoint table, leak/owner checks. |
| Repeated full parameter frame, reset, invalid update, wrong-thread call | Deterministic repeat/default; invalid calls do not publish. | Full drawable comparison and generation assertions. |

Freeze the numeric policy **before** testing representative inputs: encoded file count/bytes, per-texture dimensions/pixels, total decoded and resident budgets, MOC/model bytes, peak transient bytes, drawable/stream counts and maximum simultaneous generations. Values require local profiling and owner review, not inference from one private sample. Any required feature that cannot be represented in `DrawableSnapshot` or drawn by planned F2 returns unsupported; no approximate substitution.

## Exit and handoff to F2

F1 passes only if all positive and negative rows are reproducible at an exact source SHA, memory peaks are inside the frozen separate policy, package/texture indices and snapshot ownership are explicit, and rejected inputs cause zero GPU work and no scene change. The owner-selected primary proof is the actual trusted-local run, pinned by the tested source-content manifest. No redistributable MOC3 fixture means public CI coverage is limited; GitHub Actions remains corroborating and does not veto the local F1 verdict. Public arbitrary input, host rendering, physics behavior and exact fidelity remain HOLD after this pass. F2 receives only `AdmittedMocPackage` + immutable `DrawableSnapshot` + declared feature report; it must not parse files or read mutable evaluator state on R. A reviewer should reject F2 work if F1 cannot identify every texture/mask/blend feature needed for its pixel oracle.

## Current implementation checkpoint

F1 trusted-local CPU acceptance is **PASS** on the [tested source manifest](../reference/audits/M2_MOC3_F1_LOCAL_SOURCE.json); see [local acceptance](audits/M2_MOC3_F1_LOCAL_ACCEPTANCE.md). `moc3_package_admission.py` reads manifest-relative references and verifies the opened Windows handle remains beneath the root; duplicate file identities, reparse/ambiguous references and configured budget excess reject. Ordered decoded texture bytes and the MOC blob are immutable. Optional linked behaviors are reported disabled, with Physics3 bytes retained as data only.

`AdmittedSession` prepares a separate native worker candidate from the exact admitted MOC bytes and actual atlas count. The committed worker/package is exchanged only after validation and inventory preparation. Every rejected candidate is closed while the old worker/package/snapshot stays usable. The worker has one owner thread; complete poses reset unspecified values to defaults, and full drawable equality proves repeat/reset. Native open and per-drawable fault checkpoints, parser/decode stage checkpoints, stale/wrong-thread/invalid inputs, bootstrap cleanup and measured-memory rejection are exercised locally.

The local run records Windows lifetime peak commit for Python and the native workers, including the old/new candidate overlap. Raw image samples are intentionally decoded without a color transform. These are trusted-local technical proofs, not claims of arbitrary-input sandboxing, all MOC versions, authored visual equivalence or Physics3 execution. Host texture upload, clean offscreen pixels, frame/query lifetime and a persistent host model-session API belong to the next integration stages. F2 may now start from the immutable package/native snapshot contract, with its own direct pixel gate still required.
