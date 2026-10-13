# Avatar Runtime SDK — Roadmap, ADR Register, Implementation Workstreams

**Document:** ARS-PLAN-0001 | **Status:** PROPOSED | **No automatic merges or production claims**

## 1. Dependency-driven delivery

| Stage | Deliverables | Prerequisite | Gate |
| --- | --- | --- | --- |
| M0 Design Lock | reviewed spec, stable manifest/schema, sample, C ABI draft, threading/backend ADRs, CI plan | none | G00 |
| M1 Foundation | CMake library, typed error/handles, runtime lifecycle, validator + CPU asset parser, D3D11 host interop spike | M0 | G01/G02 |
| M2 Rendering | native 2D atlas/rig, GLB subset 3D, offscreen target, compositing, dual-mode switching and rollback | M1 | G03/G04/G05 |
| M3 Animation | semantic rig, motion queues, blend/masks, animation transitions, disconnect/replay hardening | M1; integration with M2 | G06/G07/G08 |
| M4 SDK RC | capability tiering, 30min benchmark/soak, C consumer packaging, docs, security/license reports | M2 + M3 | G09/G10/G11 |

M0 is specification-first and MUST NOT be counted as implemented. Each code workstream has an owner, source scope, public contract dependencies, reviewer and current HEAD; unrelated production files are not claimed implicitly.

## 2. Parallel workstreams after M0

- WS-01 Foundation/API: exported C ABI, typed statuses, version/struct_size negotiation, generation-tagged opaque handles, host callbacks and CMake package; guard interface change with compatibility tests.
- WS-02 Model Import/Security: .arschar ZIP, schema, 2D rig descriptor parser, embedded GLB parser, bounds/ratios/fuzz, hash and reference fixtures; CPU-only.
- WS-03 Graphics: D3D11 host-owned device/target path, offscreen premultiplied alpha, capability probing, state save/restore, lost-device/resize and GPU-thread deferred release.
- WS-04 Native 2D: atlas sprites/weighted mesh deformation, layering and clipping, reference-pixel screenshots, golden scenes.
- WS-05 GLB 3D: transform hierarchy, PBR base materials, skins, morphs, camera, draw ordering and import validation.
- WS-06 Motion: typed frames, timestamping, source generation/seq, queue overflow, interpolation, smoothing, semantic retarget, layer arbitration, immutable pose snapshots.
- WS-07 Verification/Performance: Windows/Linux CI, CTest, sanitizer, fuzz corpus, performance scene and metrics, SDK consumer example; independently verifies other streams.

Suggested coupling rules: importer never calls GPU; GPU may only consume validated immutable CPU assets; camera adapter only submits normalized frames; render consumes committed pose; no feature branch modifies public header without ABI reviewer approval.

## 3. Architecture Decision Records pending (ADR-0001..0008)

| ADR | Question and options | Proposed default | Required proof before lock |
| --- | --- | --- | --- |
| ADR-0001 | Host-owned vs SDK-owned D3D11 device | support both, host-owned primary | single-frame host embed, state restore, 100x resize/shutdown |
| ADR-0002 | bgfx vs custom D3D11 | small D3D11 initially, backend interface portable | bgfx external-device/render-target interop spike; dependency/license review |
| ADR-0003 | Native 2D rig descriptor | JSON mesh/atlas bounded schema | 2-3 rig samples incl deformation/masking validated |
| ADR-0004 | API control threading | serialize structural changes to H/S quiescent point | concurrent ingress and destructor race tests |
| ADR-0005 | Clock | host monotonic ns, per-adapter conversion | stale/reconnect and jitter reference traces |
| ADR-0006 | Runtime memory budgets | preflight CPU/GPU estimates + device cap | fail without scene corruption/partial GPU upload |
| ADR-0007 | VRM 1.0 timeline | capability-gated extension after GLB subset | full VRM expression/lookAt/constraints order tests |
| ADR-0008 | Third-party/native plugins | allowlisted trusted in-process; untrusted OOP only | code-execution threat model and IPC sandbox proof |

An ADR contains context, rejected alternatives, backward compatibility impact, threat impact, decision owner, reviewer and testing evidence. A missing or unresolved ADR that materially affects M1 is G00 HOLD.

## 4. Scope governance

PR templates should include: stage, issue/workstream, base SHA, exact changed paths, API/schema impact, owner, tests/CI/artifacts, fallback and rollback, unresolved risks, and explicit NOT MERGED status until authorized. Avoid treating acceptance as automatic merge authorization. Cross-repo policy does not apply by implication; choose a project-local governance file in a later separate decision.

Architecture MATERIAL changes: ABI, manifest model format, threading, rendering device ownership, motion clock/schema, source registration/generation, model trust limits, representation switch semantics. Such changes require a new ADR or ADR amendment and gate rerun. Normative spec, schema, examples and tests must remain consistent in same proposed change.

## 5. Immediate backlog (after design review)

1. Review specification, find ambiguities and finalize high-risk ADR-0001, ADR-0003, ADR-0004, ADR-0005.
2. Define compilable C ABI header from signature sketch and unit test a small plain-C host.
3. Implement model validator before file decoder or GPU upload. Include malformed ARSCHAR and GLB corpus.
4. Implement minimal D3D11 offscreen triangle, then host device adoption / state restore contract.
5. Demonstrate one 2D rig and one 3D skinned GLB without runtime motion.
6. Add deterministic motion replay and 2D/3D switch rollback.
7. Run staged security/perf evidence and package SDK for first release.

**Definition of Done:** check only stage-specific acceptance gates with raw evidence and reviewer disposition, never issue closure based on aspirational roadmap text.
