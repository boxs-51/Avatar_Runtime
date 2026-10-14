# Avatar Runtime SDK v0.1 — M0 Canonical Contract RC1

> **SOLO OWNER / CURRENT RC1 2026-10-14:** Owner explicitly approved Profile B layer_bindings, 87 G00 design oracles (all runtime G07/G08 HOLD), ABI20 with optional diagnostic #21 deferred, and D3D11 HOST_DEVICE/QUERY_EVENT. Historical pre-approval proposal/21-symbol/actor-distinct language below is not current authority. ARS-SOLO-OWNER-001 allows independently designed RED-first technical-method audit, not a second GitHub actor, fake approval, production certification or automatic merge. G00 design verification and owner merge decision remain separate.


**ID:** ARS-M0-CONTRACT-RC1 | **Status:** OWNER PROPOSED / INDEPENDENT RE-AUDIT REQUIRED / NOT FROZEN
**Addresses:** ARS-G00-PR3-FREEZE-REVIEW-2026-10-14-01 (three P1, one P2)
**Pinned parents:**
- Draft PR #1 design `2301aa101b46aba545c76047a0e4f7315a1a3222`
- Draft PR #2 R5 security `b80997192d14fbc7c40055300931e33727ac8674`
- Candidate PR #3 is valid ONLY while these parents remain pinned; a changed upstream HEAD requires a new material-drift review and CI.

**Normative scope:** RC1 is the **single proposed freeze-index**, not a replacement for compiler checked interfaces. The R5 > R4 > R3 > R2 > P0 security rules still apply where explicitly superseding older material, with `docs/P0_OWNER_REMEDIATION_V0.5.md` as most recent source-security proposal. Original `docs/SPECIFICATION_V0.1.md` governs product intent only; its pointer-style C sketch MUST NOT be implemented. Public header `contracts/ars_p0_contract_draft.h` is still incomplete and NOT shipping. ABI and format decisions below are **candidate owner dispositions**, NOT independent approval.

## 1. Stage gates: G00 design proof is NOT runtime proof

G00 must establish an independently reviewed **complete, internally consistent and compilable design**, with explicit C ABI declarations, format/semantic profile, host/GPU ownership rules, concurrency semantics, failure behavior, schema examples, negative design gates and exact evidence. G00 does **not** require real SDK rendering, linked production C consumer, GPU hardware tests or fuzzed parsers.

| Obligation | G00 DESIGN exit (before source) | After G00 implementation proof |
| --- | --- | --- |
| ABI | One complete `include/ars/ars.h` declaration-only RC; by-value handles; statuses; all v0.1 symbols, struct/major-minor/layout, allocator/callback, ownership and failure contracts; C11/C++20 header/symbol signature smoke; independent ABI approval | **G01** real library/toolchain build; **G10** real C host/consumer, packaging, ABI behavioral/version tests |
| D3D11 | ADR-0001 approved **design** for device ownership, immediate-context serialization, target state restoration, GPU retirement/fence and device-loss; exact GPU-spike negative matrix accepted | **M1-D / rendering gates:** real D3D11 host embed, 100x resize/shutdown, device loss, offscreen readback and resource retirement |
| Format | Selected native 2D subset + glTF capability profile, constrained ZIP and schema/semantic validator design, error taxonomy, positive/negative fixture expectations, schema parsed/validated with Draft 2020-12 | **G02/G08:** actual ZIP/GLB/PNG parser + hostile corpus/fuzz/budgets on both OS and no early GPU upload |
| Motion/security | Exact R5 authority/capability/clock/close/provenance contract independently accepted with deterministic model/negative spec | **G07/G08:** real HDL/SRC/RACE, source CSPRNG token ledger, sanitizers and native OS clock behavior |

**Dependency order:** G00 DESIGN -> M1-A real BUILD -> M1-B/C and design-approved M1-D GPU SPIKE -> stage evidence -> G10/Release. Neither a stub CI PASS nor hardware-spike proposal counts as G00 PASS. A nonshipping pre-G00 spike MAY be explicitly source scoped for feasibility, but its result may only inform the design review and never activate production SDK authority.

## 2. Proposed ABI design profile (F-02/F-08 remains HOLD)

- Preserve fixed-width `int32_t ars_status`, explicit C linkage/export convention and by-value process-local `runtime_uid, slot, generation` handles. Avoid enum-sized return ABI and public C++ object types.
- Complete **model_load/create**, **character_create/set_mode/layers**, **capabilities**, **runtime_render**, host callback/allocator and device/target descriptors alongside the 14 existing P0 operations. For each describe ownership, error zeroing, thread affinity, bounded input lifetimes, `struct_size` compatibility, major/minor semantics.
- Do NOT require a functioning engine to approve G00 ABI **design**. Require a complete compilable declaration header and signature-only consumer/stub. G01/G10 require a real linked consumer and runtime behavior.
- Keep `contracts/ars_p0_contract_draft.h` nonshipping. Naming `_p0` and legacy `ars_runtime_t*` are not aliases for the approved v0.1 surface. See `docs/V0_1_PUBLIC_ABI_FREEZE_MAP.md`.
- Error taxonomy candidate in section 4 is normative for RC1 review, not numeric reallocation of the existing `ARS_E_*` codes; numeric stability is a separate ABI decision.

## 3. Proposed ADR-0001 D3D11 DESIGN decision (F-04 HOLD pending independent acceptance)

**Primary v0.1 advertised path:** `HOST_DEVICE` only, if owner/reviewer accepts reduction from the original dual ownership promise. SDK-owned D3D11 creation (`SDK_DEVICE`) is *explicitly NOT advertised* in baseline v0.1, kept as a later separately negotiated capability. The host creates device, immediate context and render target; SDK borrows them only for a bounded render call. No implicit GPU ownership transfer.

Required design invariants:
1. Device identity verified; immediate context owned/serialized by the host's graphics R thread, or by an explicit single-owner callback policy. SDK MUST NOT drive the borrowed immediate context concurrently.
2. Validate RT format/dimensions/sample count, color space, premultiplied alpha and device generation BEFORE state mutation. Restore every D3D11 state modified by the pass to the host or return an error before mutating it. No implicit save-all if too costly; declare exact touched state set.
3. CPU assets are immutable and owned independently of device resources. GPU resources are retired on the GPU owner thread only after all CPU pins and in-flight draw/query/fence completion. A callback cannot destroy the device while resources are pinned.
4. Resize, target replacement and device loss either complete within a quiescent barrier or return BUSY/DEVICE_LOST with old logical scene preserved and no use-after-free.
5. Disallow any GPU call during hostile model/ZIP/JSON parsing. No GPU tests can be declared PASS on headless Linux.

**ADR-0001 G00 DESIGN proof:** approved state/lifetime diagrams, borrower obligations, failure/interleaving matrix and a reproducible planned hardware experiment. **M1-D physical proof:** Windows D3D11 host integration, state diff/readback, 100x resize/close, device loss and native GPU resource retirement after source implementation. These are distinct decisions.

**Detailed proposed GPU decision:** `docs/ADR_0001_D3D11_G00_DESIGN_RC1.md` provides the HOST_DEVICE ownership matrix and explicit G00 design versus M1-D physical proof. It is NOT ratified.

## 4. Proposed format, semantic validator and error taxonomy (F-05/F-07/F-09 HOLD pending independent acceptance)

**Owner selects native 2D profile B for candidate v0.1**: PNG atlas with rect, pivot, vertices, UV, triangle indices and bounded vertex-delta deform targets. Weighted deform meshes, relative-transform hierarchy, part masks and mask compositing are NOT advertised/accepted in v0.1. The original product specification's broader 2D ambition must be explicitly narrowed in capabilities/ABI and release docs before freeze. 2D visual layer ordering/visibility/opacity is allowed without mask features.

**3D baseline:** embedded glTF 2.0 GLB subset with validated buffers/accessors/morph/skin as supported by negotiated caps; no external URI, custom shader or executable extension. `vrm1` is **not** advertised or loaded in v0.1; well-formed manifest requiring VRM is rejected with UNSUPPORTED, not silently accepted because it is a GLB container.

**Validator phases, ALL CPU-only before GPU:**
- V0: native API descriptor and ZIP envelope size, canonical path & member alias checks, path traversal/symlink/external URI denial. Reject structurally malformed archive with BAD_ASSET; resource budget with OUT_OF_BUDGET.
- V1: parse JSON **with strict duplicate-object-key rejection**, strict nonfinite rejection; validate Draft 2020-12 schemas, required fields, types and version.
- V2: semantic referential integrity, uniqueness of IDs, valid `default_mode` representation, mapping targets/semantic namespace, type/unit/default bounds, DAG for layer parent/mask relationships; missing resources or conflicting embedded cross-file references are BAD_ASSET.
- V3: native 2D geometry integrity: rectangle dimensions positive and atlas bounds, vertices/UV/delta length equal, indexed triangles in bounds; no unsupported weighted hierarchy or masks. 3D GLB accessor/skin/morph size and index validation is required for actual decoder implementation.
- V4: `ValidatedCpuModel` is immutable and fully validated, with a closed set of checked paths, identities, bounds and capabilities. Only this object may be sent to the GPU upload queue. Partial failed validation MUST publish no model/GPU resource.

**Error taxonomy for a structurally valid caller input:**
- `ARS_E_INVALID_ARGUMENT`: invalid *native API call shape* (NULL mandatory ptr, inconsistent length, malformed `struct_size`, reserved bits) before parsing asset bytes.
- `ARS_E_BAD_ASSET`: malformed JSON/ZIP/GLB, unsafe path, unknown required key, ID/semantic cross-reference error, invalid geometry, bad type/range/default, forged unsupported format identifier, duplicate member/key.
- `ARS_E_UNSUPPORTED`: well-formed asset requires a **recognized but disabled** capability (`vrm1`, mask/weighted 2D features, unsupported optional glTF required extensions), or compatible ABI minor feature unavailable.
- `ARS_E_OUT_OF_BUDGET`: compressed/expanded/container/object/vertex/texture limits or memory preflight refusal, no partial model or GPU upload.
- `ARS_E_INTERNAL`: unexpected internal exception after safe cleanup, never a substitute for malformed-input classification.
- Existing security/handle errors (`REVOKED`, `SOURCE_EXPIRED`, `HANDLE_EXHAUSTED`, `CLOCK_FAILURE`, etc.) remain unchanged and MUST NOT be reused for assets.

When malformed asset and unsupported capability coexist, return BAD_ASSET first (validate basic structural/semantic validity prior to feature dispatch). Budget and envelope limits may reject sooner as OUT_OF_BUDGET. Error outputs/handles are zeroed on failure.

**Design checks:** Both schemas must pass Draft 2020-12 meta-validation and supplied JSON references must pass schema validation and defined cross-file *subset*. Test mutations: duplicate IDs; type/default misuse; rectangle width zero; out-of-bound triangle; mismatched UV/delta lengths; missing semantic mapping; VRM capability; masks/weighted 2D; canonical path; missing default representation. Tests must specify expected taxonomy and not imply that a ZIP/GLB loader was implemented.

## 5. Parent pin and consistency contract (P2 remediation)

`docs/M0_CONTRACT_PARENT_PINS.json` records **exact** ancestor HEADs PR #1/#2 and schema/ABI blob SHA fingerprints. The PR3 Actions workflow must:
- fail if PR3 pull-request synthetic merge base is not the pinned PR #2 SHA;
- verify PR #1 is an ancestor of PR #2 and supplied blob fingerprints match;
- validate schema examples/selected negative semantic cases independently of the R5 C++ contract-model;
- ensure RC1, ledger, ABI map, format map, stage plan and ADR design decision doc agree about HOLD, chosen profile, unsupported features and G00-vs-G01 separation;
- rerun for `schemas/**`, `examples/**`, security contract/ADR/spec/acceptance files; on a changed parent HEAD, require rebase and new independent exact-source review, not inherited-green approval.

## Audit-child remediation status (not accepted on parent PRs)

Proposed declaration-only `include/ars/ars.h` exists in the separate
audit Draft PR #4, with complete C ABI review inventory and fake
UNSUPPORTED symbol stub; this is not the shipping header until independent
signoff. RED-first F01/F02/F03 demonstrate why all semantic validity must be
checked before unsupported-capability resolution, why actual PNG dimensions
must be a separate witness and why all parsed numeric data must be finite.
F08 G00-P0-C predecessor clause has a separate security-owned Draft PR #5
proposal; until ACK/re-pin, this remains a material dependency and G00 HOLD.

## 6. Explicit remaining conditions for freeze

F-01 security R5 still requires an independent DESIGN verdict at the current PR #2 HEAD. F-02/F-08 full ABI declarations and ownership not yet final. F-04 ADR-0001 is proposed above, not approved. F-05/F-07/F-09 profile B and semantic taxonomy proposed above, require owner/reviewer signoff and negative fixture design ratification. F-03/F-06 R5 authority/clock semantics need acceptance. F-10 G00 signoff requires a new exact SHA and zero material P0/P1 design blocker. F-11 M1 source scopes are PREPARED only.

**FINAL RC1 OWNER STATE:** `REVIEW_READY_CANDIDATE`, `G00_HOLD`, `IMPLEMENTATION_HOLD`, `NO_MERGE`. This file does NOT confer freeze, production admission or merge authority.

## Approved owner design choices and independent gates (2026-10-14)

Sole project owner explicitly selected native Profile B mandatory one-owner `representations["2d"].layer_bindings` (PR8), staged Security R5/PR5 G00 design oracles with 87 actual tests deferred to G07/G08 (PR9), 20 public ABI declarations with optional diagnostics removed and HOST_DEVICE-only D3D11 QUERY_EVENT retirement (this PR10). This is **owner design authorization**, not G00 independent PASS or merge authorization. Source ancestry is PR8 `2301aa101b46aba545c76047a0e4f7315a1a3222` → PR9 `b80997192d14fbc7c40055300931e33727ac8674` → composed RC1; all 9 blob SHA checks must pass on its exact synthetic merge. Actual SDK, hostile parser, GPU and runtime security remain NOT IMPLEMENTED/HOLD.
