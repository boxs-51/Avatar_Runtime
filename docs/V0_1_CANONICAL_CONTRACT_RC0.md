# Avatar Runtime SDK v0.1 — Canonical Contract Candidate (M0 RC0)

**ID:** ARS-M0-CONTRACT-RC0  
**Status:** REVIEW CANDIDATE / **NOT FROZEN** / no G00 approval  
**Parent design:** Draft PR #1, \`93858416dfdd0d6b7f0e10c8e524da4324dd9dc4\`  
**Security parent:** Draft PR #2, \`16abfb058a90f90f4ed89f6b6ced2000bff0b30f\`  
**Audience:** architecture/security reviewer, ABI implementer, asset/renderer owners  
**Rule:** until independent review and all **G00-DESIGN** requirements are met, this document is a *single reading entry point for a candidate*, NOT shipping API or a claim that underlying proposals are accepted.

## 1. Versioning and source of truth

1. Specification intent and non-goals: \`docs/SPECIFICATION_V0.1.md\`.
2. **Proposed** security and lifetime contract: \`docs/P0_OWNER_REMEDIATION_V0.5.md\`, then the unmodified provisions of R4 -> R3 -> R2 -> v0.1 in that precedence. Relevant R5 ADR proposals are in \`docs/ADR_P0_SECURITY_R2.md\`.
3. **Actual draft declarations:** \`contracts/ars_p0_contract_draft.h\`. The legacy pointer-based function sketch in SPECIFICATION section 5 is **superseded as a public-handle proposal**; it is neither compilable nor a valid reason to implement pointer handles. The draft \`_p0\` functions are **not complete**.
4. JSON-schema validation: \`schemas/character_manifest_v0.1.schema.json\` and \`schemas/ars2d_rig_v0.1.schema.json\`, PLUS a mandatory semantic/cross-file validator not yet implemented; fixture examples are illustrative, not proofs of semantic validity.
5. Evidence: \`docs/ACCEPTANCE_GATES_V0.1.md\` and \`docs/P0_SECURITY_REGRESSION_GATES_V0.1.md\`; pass/hold states in \`docs/M0_V0_1_FREEZE_READINESS.md\` are authoritative for this proposed freeze attempt.

**Conflict handling:** fail closed and record \`BLOCKED_CONTRACT_CONFLICT\`. Do not quietly choose whichever R2/R3/R4 paragraph is convenient or emit a second divergent ABI. A change to material ABI, model format, source authority, threading/clock, GPU ownership or deterministic pose rules requires matching ADR/gates and a fresh source-pinned review.

## 2. v0.1 minimal product promise

Avatar Runtime is an *embedded library* with a host-controlled event loop. It owns logical characters, validated assets, semantic motion mixing, immutable poses and rendering; it does not create cameras, network connections, AI inference, arbitrary plugins or standalone editor. Windows x64 + D3D11 is the initial real GPU target; Linux x64 must build and run CPU/security tests. Do not claim Linux rendering until a backend is implemented and tested.

One logical character may have native 2D, native 3D or both. \`ARS_MODE_AUTO\` is a runtime selection request; manifest \`default_mode\` is 2D or 3D only (its different domain is intentional). Native 2D rig is not merely orthographic 3D. Representation switch is PREPARE -> VALIDATE -> COMMIT or ROLLBACK and must preserve last valid mode/pose on failure.

Visual layers (z-order, visibility, opacity, mask, asset linkage) are separate from motion layers (source-scope/priority/blend/channel ownership/lease). Character-local semantic rig mapping is the only route for cross-representation motion control.

## 3. ABI and ownership — v0.1 **candidate only**

- Fixed-width result \`int32_t\`; C linkage and explicit import/export + calling convention. Every extensible input/output record has \`uint32_t struct_size, uint32_t abi_version\`, with major/minor version negotiation. The checked prefix must be readable before reading any optional suffix; reserved fields must be zero.
- Handles are **by-value, process-local IDs** \`(uint64_t runtime_uid, uint32_t slot, uint32_t generation)\`, never dereferenceable external pointers. A registry pins validated storage for each operation; handle invalidation and physical CPU/GPU retirement are separate. No generation, runtime UID or capability token may wrap/recycle.
- An exported \`ars_source_admin_h\` controls grants; distinct 128-bit OS-CSPRNG \`ars_ingress_cap_v1\` authorizes ingress ONLY. Untrusted source labels, source_id/generation fields, timestamps, priority or character claims confer no authority.
- Binding: runtime + one live character generation + explicit canonical ASCII semantic-channel allowlist + fixed trusted priority + finite quota + lease. A token is local to the same-process trusted adapter, not a network identity. Remote auth terminates at a trusted adapter before the C ABI.
- Public native pointers have **valid memory/lifetime/alignment preconditions** for synchronous calls; SDK rejects NULL/count/overflow, not arbitrary forged process pointers. Host adapter must decode hostile serialized inputs into bounded validated native structures. No STL/exception ownership crosses the C ABI.
- Current draft \`_p0\` surface has 14 callable symbols tested through a **nonfunctional UNSUPPORTED stub**. The complete public v0.1 surface is **NOT** defined: model load/create, character creation, mode/layer changes, capabilities, offscreen render target, device ownership and allocator/callback contracts must be reconciled with the original API sketch before ABI freeze. Do not expose \`_p0\` as finalized \`ars/ars.h\`.

**ABI freeze exit:** publish one canonical C header in \`include/ars/ars.h\` with all required v0.1 operations, explicit layout/allocators/errors and version rules; linked **real** C consumer tests on Windows/Linux; confirm ABI/ownership ADR and independent sign-off. Stub-linked smoke PASS alone is insufficient.

## 4. Runtime, source and pose linearization invariants

### 4.1 Handles and lifecycle
- Registry lookup validates runtime UID, type, slot and generation **before** returning pinned object storage. No stale-pointer dereference, double destruction or ABA on slot reuse. Generation exhaustion permanently retires affected slot.
- Logical close bars new pins/authorizations; pre-close operation pins ensure no UAF until CPU/snapshot/GPU completion. Close-management pins allow query/drain/finalize while normal work admission is closed.
- GPU owners retire device resources ONLY after snapshot releases and backend-safe completion on R thread. This is an explicit **ADR-0001 HOLD** until real D3D11 design and evidence exists.

### 4.2 Ingress and close progress
- Each source uses one ingress mutex for atomic {active epoch/cap, sequence high-water, TTL/quota, bounded queue append}. A failed append does **not** burn sequence/quota. Accepted frames do not imply future pose commitment.
- Ingress pre-parses/copies buffers before entering a bounded **admission lease**. Admission gate: mutex + condition variable + finite reader count and sticky \`CLOSE_REQUESTED\`. Closing changes OPEN->CLOSE_REQUESTED before draining prior admitted operations, blocks all new admission leases, then transitions RUNNING->QUIESCING. No successful queue append linearizes after QUIESCING.
- The admission gate mutex is never held while locking source/pose/clock/router. Closing liveness is bounded by admitted nonblocking critical sections, not an impossible unconditional guarantee against a stalled host thread.

### 4.3 Security clock and source state
- SDK-owned monotonic OS security time and host simulation time are **different**. One per-runtime \`security_clock_gate\` serializes all authorization observations; a backwards clock/failure makes runtime \`SECURITY_CLOCK_FAULT\` sticky and forbids fresh grants/ingress/new pose commits. No automatic recovery within the same runtime.
- At the first observed sample with \`now >= expiry_ns\`, source transitions terminal EXPIRED for this registration. Clear cap, rotation ticket, queue authority, advance epoch safely and release-publish invalidation tombstone. Exhausted epoch/ticket enters terminal EXHAUSTED **without wrapping**, even if increment is impossible.
- Rotation revokes old cap FIRST, reserves exact ticket/source/control/admin/character epochs, mints new CSPRNG token outside locks, then compare-and-commits only if still valid and unexpired. Explicit revoke is terminal and cancels pending rotations; failed mint never restores old cap.
- Permission/revocation guarantee is **at pose commit authorization sample**. It is not instantaneous screen/GPU-frame erasure. A privacy scrub needs a distinct present/GPU fence.

### 4.4 Cross-lock provenance and pose state
- Source expiry/revoke under source locks writes a source-owned release-ordered epoch tombstone and MUST NOT acquire pose gate or mutate simulation-owned caches. The pose/mixer single writer acquire-observes tombstones and checks **transitive lineage** across smoothing, retained filters, delayed triggers, pose history, rig retarget and mode-switch caches.
- Final publish order: runtime commit gate -> character pose gate -> source locks in stable slot order -> security clock gate. Publish either atomically commits an immutable pose + staged persistent state with valid lineage or discards them and recomputes from neutral + remaining valid sources. No post-revoke freshly committed state may retain revoked ancestry.
- Exact lock order across registries and GPU remains a required independent implementation-audit item. No callbacks, unbounded allocation or host reentry under authority/pose locks.

## 5. Model and format contract (candidate)

\`.arschar\` = constrained ZIP with root \`character.json\` and bounded relative assets. Default reject unsafe paths (absolute/UNC/device/traversal), symlinks, duplicate/alias entries, arbitrary external URIs, executable payloads, oversized archives/entries/decompressed budget and invalid GLB/PNG. Reject before GPU upload. Use CPU-only validator first.

JSON Schema Draft 2020-12 validation **is necessary but not sufficient**. A deterministic semantic validator MUST also reject:
- duplicate identities/references, unresolved layer/parameter targets and hierarchy/mask cycles;
- bool interpreted as number, nonfinite numbers, defaults outside declared ranges, noncanonical semantic identifiers and other type/unit mismatches;
- \`ars2d\` atlas rects with nonpositive dimensions, UV/vertex count mismatch, invalid triangle indices and vertex-delta count mismatch;
- unsupported required representation capabilities (including \`vrm1\` when VRM importer is not implemented), and cross-file dimension/path/index conflicts.

The complete validator input/output contract and negative fixtures require a separate design decision in the G00 register before treating JSON schema as a frozen accepted format. Third-party sample art licensing and asset hashes must be documented before release.

## 6. Stage authority and first source boundaries

**M0 contract freeze:** only after independent R5/R6 security DESIGN audit, full public ABI decision, relevant ADR-0001/0003/0004/0005/0009/0010/0011/0012 decisions and schema semantic validator contract, all on one pinned candidate HEAD.

**M1 source implementation starts with:**
1. \`M1-A BUILD ONLY\`: CMake C++20 static/shared target, public header smoke, CTest, Windows/Linux CI, no runtime security claims.
2. \`M1-B CORE CPU\`: typed by-value handle registry, pinning, close/admission gate, clock provider, source admin and token-mint/ledger behind exact ABI. Controlled negative regressions before enabling external ingress.
3. \`M1-C VALIDATOR\`: deterministic semantic/ZIP/GLB CPU parse on bounded inputs; no GPU calls.
4. \`M1-D GRAPHICS SPIKE\`: D3D11 host/device ownership, state restore and fence/retirement AFTER ADR-0001 approval; not coupled to unreviewed asset parsers.

**Source scope and gate matrix** are in \`docs/M1_SOURCE_BOOTSTRAP_PLAN.md\`. No M1 implementation PR should claim G00 PASS or production merge from test harness alone.

## 7. Explicit HOLDs at this freeze attempt

- Independent R5 review of \`16abfb058a90f90f4ed89f6b6ced2000bff0b30f\` is not recorded.
- Current 14-symbol \`_p0\` header is an **incomplete public ABI**; only stub behavior has CI evidence.
- ADR-0001 D3D11 owner/context/fence; ADR-0003 native 2D schema binding; runtime clock and thread decision ratification pending.
- Cross-file semantic schema validator and negative examples not implemented or independently accepted.
- 87 HDL/SRC/RACE cases **specified**, not a production runtime pass. GPU tests, real registry, CSPRNG ledger, sanitizer proof, fuzz corpus and performance evidence absent.
- No merge, no release and no signature claiming final v0.1 public stability.
