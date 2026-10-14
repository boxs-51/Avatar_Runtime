# Avatar Runtime v0.1 — Format & Semantic Validation Freeze Map

**ID:** ARS-FORMAT-M0-01 | **Status:** REVIEW CANDIDATE, NOT LOCKED  
**Source:** \`schemas/character_manifest_v0.1.schema.json\`, \`schemas/ars2d_rig_v0.1.schema.json\`, \`examples/character_dual_mode.json\`, \`examples/character2d.ars2d.json\` and SPECIFICATION section 4.

## 1. Mandatory validation layers (in order)

1. **Bounded source acquisition:** no network fetching; require caller-trusted local file or memory buffer. No OS path escape. Copy/protect input lifetime.
2. **ZIP envelope first:** reject duplicate entries, absolute/device/UNC/drive paths, \`..\` traversal, symlink entries, invalid path encodings, Unicode/case-fold/normalization alias collisions, entry count or inflate/expanded size over ceiling and nested bombs. Never extract into an arbitrary host path.
3. **JSON Schema Draft 2020-12:** check required keys, declared types, \`enum\`, nested arrays, min/max, unknown required extensions. Treat JSON booleans separately from integers.
4. **Semantic/cross-file validation:** validated names, referential integrity, cycles, actual atlas/image dimensions, mesh/UV/triangles, deformer arrays and default ranges; never rely on schema alone for cross-array index and bound checks.
5. **Decoder/CPU asset validation:** GLB 2.0 embedded buffer views/accessors/index arithmetic checked against actual byte lengths; PNG/decode pixel budgets validated before allocation. External URI and custom shader/embedded code denied by default.
6. **Immutable CPU asset publication:** only fully validated assets enter model registry. GPU allocation/upload waits for backend-supported caps, budget and approved GPU source scope.

## 2. Proposed semantic invariants and negative fixtures

| Fixture ID | Intentionally invalid construction | Required result |
| --- | --- | --- |
| FMT-001 | duplicate semantic IDs, duplicate representation IDs, duplicate layer IDs | BAD_ASSET; no partially loaded model |
| FMT-002 | a boolean used as numeric parameter default/range without declared conversion | BAD_ASSET |
| FMT-003 | parameter default outside declared min/max | BAD_ASSET |
| FMT-004 | atlas texture rectangle with zero or negative dimensions, or out of image bounds | BAD_ASSET |
| FMT-005 | UV array count != vertex count | BAD_ASSET |
| FMT-006 | mesh triangle index >= number of vertices, or arithmetic overflow | BAD_ASSET |
| FMT-007 | vertex-delta deformation array length mismatches mesh vertices | BAD_ASSET |
| FMT-008 | \`vrm1\` representation/required extension when runtime does not advertise importer | BAD_ASSET / UNSUPPORTED as formally chosen; must NOT accept as supported |
| FMT-009 | visual layer parent cycle or mask cycle | BAD_ASSET |
| FMT-010 | rig mapping refers to unknown semantic channel/part | BAD_ASSET |
| FMT-011 | duplicate ZIP entries under case-folding/Unicode normalization alias | BAD_ASSET |
| FMT-012 | absolute/UNC/device/traversal asset path or symlink | BAD_ASSET before extraction |
| FMT-013 | embedded GLB external http/data/file URI or oversized buffer | BAD_ASSET, zero network accesses |
| FMT-014 | mode default 2D but representation absent, or 3D unavailable | BAD_ASSET |
| FMT-015 | malformed JSON schema version, unknown required capability | BAD_ASSET or formal UNSUPPORTED result |
| FMT-016 | malformed PNG dimensions/GLB accessor offsets causing integer overflow | BAD_ASSET with bounded allocations |
| FMT-017 | duplicate parameter values in different files with conflicting type/unit | BAD_ASSET |
| FMT-018 | safe reference \`examples/character_dual_mode.json\` + \`character2d.ars2d.json\` with available referenced assets | accepted only after complete cross-file validation and runtime caps |

**All above are *proposed* fixtures until implemented.** Accepted JSON shape alone does not prove assets are renderable. Fixtures must be source-controlled with exact expected error codes, no absolute machine paths, and resource ceilings.

## 3. Two design choices that must be explicitly ratified

**F-05 native 2D design:** baseline spec calls for relative part transforms, weighted deform mesh, masks and hierarchy, while the current native-2D schema centers on vertices/UV/triangles and vertex deltas. Either:
- **A — extend schema/validator** for transforms/weights/masks/hierarchy with explicit finite bounds; OR
- **B — narrow v0.1 advertised capabilities** to vertex-delta deformation only and postpone advanced 2D features.

Do **not** advertise weighted 2D/masking if neither schema nor renderer enforces it. Proposed preference for M0: **A if reference fixtures+validator can be delivered at G00; otherwise B explicitly with reduced v0.1 feature flags**. No implicit choice.

**Format-capability registration:** GLB 2.0 subset is baseline; VRM 1.0 importer is deferred and must be gated by negotiated capabilities. The manifest \`default_mode\` (2d/3d) is not the runtime AUTO selection mode. Mandatory unsupported representation must be rejected, not silently accepted.

## 4. Validation API and proof boundary

\`validate_asset(const bytes, limits, capability_set) -> expected<ValidatedCpuModel, ValidationError>\` is a **conceptual internal contract**, not a public shipping C++ signature. It returns an immutable parsed CPU model with validated identity tables, bounded resources, verified references and no GPU effect. The renderer must not consume unvalidated source bytes.

Required evidence before FORMAT G00 design PASS:
- Schema v0.1 checked with an actual Draft 2020-12 implementation and reference examples.
- Semantic-validator interface, accepted data model and exact corpus of negative fixtures ratified by asset/graphics owners.
- No untrusted URI fetch; no path escaping or decompression budget bypass; typed errors and bounded failure.
- Native 2D choice A/B and VRM importer capabilities accepted at exact HEAD.

Required evidence for implementation \`G02\`: negative fixtures actually executed across Linux/Windows with real parser, fuzzing and CPU memory bounds. G00 format design review is not a substitute for production security tests.

**Current verdict:** \`HOLD_FORMAT_SEMANTIC_CONTRACT\`. Do not freeze format or approve M1-C importer until above design obligations are closed.
