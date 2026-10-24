# ADR-005 — Non-public model information and reusable SDK packaging

**Status:** OWNER REQUIREMENT / documentation baseline (2026-10-24)
**Repository:** `Avatar_Runtime`
**Scope:** GitHub, PR/CI artifacts, documents, published SDK packages, binaries, logs, captures, and example bundles.
**Interpretation:** This is a **non-disclosure/publication rule**, **not** a new local model-access permission regime.

## Decision and hard invariant

**Model information classified as private must not be publicly published, accidentally embedded in distributable SDKs, or copied into Git history, GitHub issues/PRs, workflows, logs or evidence archives.** Developers, Studio UI, and local Agent tooling may still read and edit a model *within their existing legitimate local workflow*. This ADR does not add a blanket read-deny, create UI consent dialogs, or turn a model path into an authorization token.

A model is **not public merely because its local runtime/API supports it**. "Public CPU asset profile" and "public SDK API" describe the software distribution interface, **not** a blanket license to redistribute model assets or model-derived evidence.

### Confidential classes: never export by default

| Class | Non-public examples | Disposition |
|---|---|---|
| Model sources | Cubism `.moc3`, `.model3.json`, physics/motion/expressions/pose/userdata, GLB/GLTF or other privately owned models | Local workspace only |
| Derivatives | Vertex coordinates, topology, mesh/rig weights, unique parameter/drawable IDs, texture atlases, identifying model-specific statistics/hashes | Do not put into public docs, assertions, API examples or test output |
| Visuals | Rendered model screenshots, video, captures, thumbnails and previews | Save privately; never attach automatically |
| Identifiers/locations | Model filenames, directory paths, proprietary resource IDs, customer/project identifying labels | Sanitize public outputs, not merely remove content bytes |
| Runtime/evidence | Raw IPC payloads, debug traces, full JSON responses, crash dumps and failure logs containing model data | Retain locally under ignored/private evidence roots; publish status and broad error class only |
| SDK/install | Example corpora, cached models, copied texture/resource directories, private tests and build artifacts | Exclude from install/export/archive manifests |

**Safe public evidence:** test case ID, generalized scenario, PASS/FAIL/SKIP/BLOCKED, non-identifying error class, source code file/line, feature-gate status and aggregate test outcomes **only when they are not model-derived or identifying**. Never publish private geometry counts, coordinates, resource names, screenshots, derived hashes or detailed raw model receipts as "anonymized" evidence.

## Publication gates (GitHub and SDK release)

1. **Default deny for model artifacts:** The tracked tree, staged index, diff attachments, release archives and installed SDK manifest must not contain private model file types, model binaries under renamed extensions, private captures or known private directories. `.gitignore` is a convenience, **not** a security control against `git add -f` or past commits.
2. **Content and evidence review:** Inspect proposed PR/issue comments, CI logs, screenshot attachments, verbose failure output and user-facing examples for model-specific identifiers and derived content. Do not post suspicious raw evidence in public while investigating; report categorical findings only.
3. **Automated pre-publication scanner:** Use `python scripts/check_model_publication.py --staged` before a commit and `--ref HEAD` before publication. The script checks known artifact formats and raw model JSON signatures and intentionally does **not** print disallowed filenames, paths or payload snippets. A PASS does not certify that arbitrary prose, logs, Git history, or a compiled binary has no embedded private content.
4. **SDK install/export validation:** Scan the actual SDK install/staging directory with
   python scripts/check_model_publication.py --package <sdk-directory>, then inspect
   its manifest and any compressed release archive. This guard blocks model/media
   extensions, renamed known binary formats, private directories, and symlinks.
   It is not a replacement for checking raw logs, exported symbols, or private
   byte sequences embedded inside native objects.
5. **Explicitly distributable fixtures only:** Generated/public-domain/licensed model fixtures are allowed **only after a separate provenance and publication review**. Prefer generated synthetic meshes/rigs with no real assets. Keep exceptions in an approved release manifest external to the default scanner rather than silently ignoring matches.
6. **History and remote scope:** A clean current Git tree does not prove old commits, orphaned local blobs, forks, caches or remote releases are clean. Suspected earlier exposure requires a separate forensic/source audit and controlled remediation; never rewrite shared Git history unilaterally.
7. **Hard stop:** Missing origin/license approval, an unexplained scanner finding, a questionable upload artifact, or an unreviewed model-derived log **blocks publication of that artifact**. It does not block legitimate local model editing.

## Reusable component / SDK packaging policy

Reusable functionality should be isolated from the Studio application **when it has a stable contract, genuinely reusable API, bounded dependencies and independently testable behavior**. Do not create new public ABI symbols just for convenience.

| Candidate | Packaging direction | Gate |
|---|---|---|
| G5 CAS, undo/redo, receipt/recovery core | Independent **internal C11 library** with synthetic test fixtures; separate from Studio GUI; no direct model source or texture dependency | Native cross-target tests, stable owner semantics, no writer enabled before real Studio gates |
| Canonical request hashing / strict bounded wire parser | Reusable internal utility library/module; no raw private payload telemetry | Test vectors, malformed input and privacy checks |
| Native Rig V4 deterministic transformations | Internal rig library with private runtime model references supplied by caller, never bundled assets | Independent tests, validation of model-generation/fences |
| Public RuntimeCore and installed C/C++ SDK | Keep existing ABI20 and packaging profiles; expose only documented stable public APIs | Installed consumer tests and *public-safe installation manifest* |
| Viewer/Studio integrations, private model adapters | Stay Host/Studio-private rather than published as model data or implied SDK contracts | Separate explicit API and confidentiality review |

Rules for a reusable package: (a) source/library/API documented without embedding model samples; (b) data ownership stays with the caller/local workspace; (c) test fixtures are synthetic or separately licensed and approved; (d) examples, install rules, release archives and error messages pass the non-publication gate; (e) feature flags default OFF until verified; (f) backwards compatibility and independent consumer tests; (g) security/privacy review before changing a private implementation into a public SDK.

**Packaging is not a bypass for G5:** G5 Native writer remains **HOLD** pending Host-bound actors, shared UI/V1/Agent command authority, recovery IPC, physical GUI concurrency and XPBD/GPU accepted-frame evidence. Packaging an internal component does not automatically permit Studio/V2 mutation or exporting privately owned models.

## Implementation and test acceptance

- Current `.gitignore` excludes known private Cubism formats and ignored build/local-assets directories; retain them, but do not claim they cover arbitrary renamed or embedded data.
- The publication scanner rejects known private artifact extensions, private directory scopes and renamed obvious model/image signatures; its output contains only category labels, not private paths or asset counts.
- Negative synthetic fixtures cover forbidden filename formats, hidden/private directory paths, renamed model/PNG bytes, private model JSON signatures, and a harmless source-code control.
- Run scanner on staged tree and on the exact Git revision to be released. Review SDK installation manifests independently for additional bundled files.
- Maintain a public-safe log/report generator rule; never copy raw model-specific logs into Markdown/PR comments automatically.

## Explicitly out of scope

This ADR **does not** restrict model read/edit rights, alter Host/user permission modes, add network permissions, authorize new exported SDK functions, delete private local assets, purge Git history, or certify historical/public absence. It governs **what can be published publicly**, not whether an approved local user/Agent can work on their own model.
