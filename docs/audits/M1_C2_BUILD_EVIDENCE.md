# M1-C2 private CPU GLB and dual-mode implementation

The owner explicitly authorized a child stack from unmerged PR27 and immediate C2 implementation. Parent branch is `m1/c1-native-assets`, exact source `9f5d6b8a634138e8dedae8385dcb783cd385b6c8`. This overrides the earlier merge-first sequencing prerequisite; it authorizes no merge. C2 changes exactly the approved 24 paths.

## Implemented behavior

GLB 2 framing and embedded BIN data are checked before an immutable CPU model is published. Accessors materialize actual component values, stride and padded matrix layout, sparse overlays and indices. Node forests, skin influences and inverse binds, morph targets, animation keys and cubic cardinality, embedded PNG pixels, material/texture references and semantic mappings are validated. Owned metadata retains the validated material and scene descriptions. Required disabled features are dispatched only after core and cross-file validation. No GPU, motion playback, URI resolution or public runtime activation is introduced.

Format reference: [Khronos glTF 2.0.1 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html), revision `8e798b02d254cea97659a333cfcb20875b62bdd4`. The four dependency commits and archive hashes remain unchanged. JPEG is recognized disabled after a bounded marker/frame/scan-envelope check; this is not JPEG entropy decoding. Unknown extension payload semantics are not claimed as validated.

One 64 MiB bounded arena covers the manifest, rig and GLB JSON in a transaction. Actual host allocations remain charged to the resource. Decoded RGBA bytes share one pixel budget across the 2D atlas and all 3D PNGs. Work, vertices, accessor ranges, recursion-free hierarchy traversal and allocator failures are bounded. Failure publishes no partial model; a separately held older model remains intact.

## Preserved evidence and oracle correction

PR27's original six native groups compile from a Git archive of its exact source and use its original 83-fixture corpus. Their historical positive-dual expectation remains UNSUPPORTED. C2 separately replays all 77 original C1 bundle verdicts on the changed production pipeline and accepts supported 3D/dual models. Historical custody functions are unchanged; C1 custody tests target the frozen parent snapshot.

The original six C2 fixture bytes and hashes are preserved. One original oracle, `glb-required-extension`, is strengthened from UNSUPPORTED to BAD_ASSET: its required extension is absent from `extensionsUsed`, so its core declaration is malformed. A separate valid Used+Required fixture must return UNSUPPORTED. No original C1 oracle changes.

## Native execution and final source gate

The RED commit `9714e1fb88cad813f67ecc1e79fa617dcae27712` demonstrated a real native triangle ingestion failure before the implementation (expected OK, actual UNSUPPORTED, executable exit 1). The expanded 176-fixture corpus includes exact 256/257 and cumulative joint boundaries, normal/tangent morph deltas, quaternion CUBICSPLINE, aggregate image budgets and external URI rejection. The three MSVC ASAN C2 groups passed locally, as did 130 contract and nine governance tests. An intermediate MSVC test-shadowing warning was corrected; final-source CI, preserved regression and both complete fuzz campaigns must be attested against the final source in the implementation PR. Per-case required statuses and fixture hashes are in `M1_C_CASE_EXECUTION.json`; raw test logs contain expected/actual status, cleanup checks and allocation-cut evidence.

CI runs the native OS/compiler/shared/static/sanitizer matrix, reference-schema parity and plain C consumers. It also runs two actual libFuzzer entries (ARSCHAR and GLB), each with seed 415253, 1 MiB maximum input, 2-second input timeout, 1024 MiB RSS ceiling, a 60-second smoke and a 3600-second acceptance campaign. Completion requires final summaries, successful exit codes, retained logs and no crash artifacts. A scheduled or partial campaign is not a PASS.

This is SELF_AUDIT; independent human review is not fabricated. Full G02 remains HOLD. All 87 B whole-case verdicts remain HOLD, PASS 0. PR27 and the C2 child remain unmerged pending separate owner instructions.

The shared-bufferView review finding was checked against normative section 3.6.2.4: multiple vertex attribute accessors require an explicit stride even when their ranges are sequential. Paired no-stride BAD_ASSET / explicit-stride OK native controls retain that rule. Custom vertex attributes reject unsigned-int components, and node-less animation channels are ignored as specified while their JSON metadata remains owned.
