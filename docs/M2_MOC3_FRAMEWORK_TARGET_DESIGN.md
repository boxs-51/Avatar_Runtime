# M2 optional MOC3 framework: target output and architecture

**Status:** authorized staged framework contract; F1 trusted-local CPU, F2 bounded host pixels and F3 bounded linked physics PASS locally; F4-A private core/host integration PASS; whole F4 public product remains HOLD. **Baseline:** private P0 comparison and partial P1 evaluator/snapshot on `m2/moc3-p1-snapshot`. **Relationship:** refines [the next-implementation plan](M2_MOC3_NEXT_IMPLEMENTATION.md); [F1 admission design](M2_MOC3_F1_ADMISSION_DESIGN.md) and the [agent-callable, visible test harness design](M2_MOC3_OBSERVABLE_TEST_HARNESS_DESIGN.md) are detailed follow-ons. The [v0.1 specification](SPECIFICATION_V0.1.md) and [acceptance gates](ACCEPTANCE_GATES_V0.1.md) remain authoritative for the core SDK. The official Live2D SDK is deferred. This optional path is default OFF and does not enlarge the v0.1 native 2D format or advertise a public capability.

## 1. Product contract and decision

The owner's current request is to design a sufficiently complete framework **before** further source implementation, so the intended output can be judged correctly. The scoped outcome is an optional adapter that evaluates an authorized, locally supplied MOC3 character over time and submits its real drawables to the Avatar Runtime host's offscreen D3D11 path. The host controls the update clock, parameter input, target/device lifetime and presentation. A changing portrait or external viewer screenshot does not fulfill this outcome.

| Candidate | What it delivers | Main limitation | Decision |
| --- | --- | --- | --- |
| A. Runtime adapter into the existing host | Dynamic parameter evaluation, bounded drawable snapshots, host-owned offscreen pixels, optional linked physics and lifecycle through the same host. | Requires manifest admission, GPU renderer and exact ownership/lifetime work. | **Recommended design target**, subject to review and evidence. |
| B. Standalone converter/player | Easier local inspection and reference capture. | Does not prove embedded host pixels, state restoration, mode transaction or SDK lifecycle; finite sampled poses are not general dynamic evaluation. | Keep only as a diagnostic or fallback tool. |
| C. New in-house binary evaluator | Full control of implementation boundary. | Highest format/deformer maintenance and parser risk; no automatic fidelity oracle. | Reconsider only after a documented failure of existing candidates and a bounded, testable subset. |

**Explicit requirement:** settle the framework and target output before the next design or code slice. **Derived requirements:** preserve host/device ownership and gate semantics, use only authorized local assets, keep the optional feature isolated, and establish separate geometry, pixel and physics evidence. **Selected public-input scope:** the owner selected arbitrary public-user MOC3 input during F4 completion. **Open fidelity choice:** whether “compatible” means a defined supported-feature subset or author-intended visual equivalence. The public target requires full isolated admission/evaluation and independent host-boundary validation; present trusted-local tests do not qualify it. Do not claim universal compatibility. Architecture approval is not implementation, merge, packaging or public-activation approval.

## 2. Target observable journey

1. The host supplies a local root and explicitly selects a `.model3.json` package. Admission resolves only declared relative references, validates the required MOC and textures and the selected optional resources, computes separate resource budgets, and returns either a complete immutable CPU package or a typed rejection. No GPU calls occur on rejection. Unreferenced files are ignored.
2. A single simulation owner creates an evaluator session and explicit parameter binding. For each host monotonic update tick it applies admitted semantic/manual/motion input under a frozen arbitration rule, advances linked physics by a bounded fixed-step rule when enabled, evaluates the MOC, and publishes one immutable drawable snapshot with a monotonically increasing generation. Disabling physics or releasing input has defined reset/decay behavior; it cannot leave an accidental perpetual force.
3. The render owner admits the snapshot and its texture set against the current host device/target generation. It draws the actual evaluated meshes, order, clipping and supported blend operations to a host-owned offscreen target. A successful readback produces testable pixels at declared size, format, color/alpha convention and viewport. The host may composite this target into a larger scene.
4. Resize, switch, reload, close and device generation change either commit a complete new state or preserve the last valid state; a lost generation is quarantined until the host's exact completion/teardown protocol permits release. A failed draw never publishes a partial target as a valid frame.

The first vertical slice should use a rights-cleared, redistributable MOC3 fixture with one atlas and a small declared feature subset if one can be obtained. It must pass through the **same evaluator-to-host path** as the intended feature. If no such MOC3 fixture exists, public manifest/renderer fixtures and an opt-in private evaluator run are separate evidence; the full path cannot be called public-CI-proven. Private licensed models may supply additional feasibility observations, but their files, images, measurements and derived metadata stay under ignored local `build/` evidence. A public fixture does not establish fidelity for every model.

## 3. Modules, contracts and ownership

```text
host source/clock -> core semantic pose -> MOC3 parameter binder -> evaluator + linked physics
                                                          |
asset admission -> immutable ModelPackage                 v
              -> immutable TextureSet           immutable DrawableSnapshot
                                                          |
                                        host render admission (device/target epoch)
                                                          |
                                         D3D11 offscreen pass -> host compositing
```

| Boundary | Input -> output | Owner and invariant |
| --- | --- | --- |
| Package admission | local root + selected manifest -> `ModelPackage` or typed error | CPU-only asset worker/control thread; canonical containment, checked file/decode limits, reference closure, no network or implicit sidecars. |
| Evaluator session | validated MOC + parameter frame -> `DrawableSnapshot` | One simulation writer; evaluator state is never read concurrently by renderer. Reject required features not represented by the snapshot contract. |
| Physics adapter | linked Physics3 + clock/input -> checked parameter deltas/state | Simulation writer; deterministic step, clamp/reset and declared parameter ownership. A physics engine from another evaluator may feed this session only after parameter semantics and update order are proven equivalent. |
| Snapshot mailbox | generation + immutable drawables/texture keys/feature flags | Atomic publication after full preparation; renderer pins the complete snapshot. Failed updates leave the preceding committed snapshot valid. |
| Texture residency | validated image metadata/bytes -> same-device atlas resources | Render owner; decoded, staging and resident bytes are accounted separately, with generation-aware cache invalidation. |
| Draw pass | snapshot + texture set + host target -> completion witness + readable target | D3D11 R thread only; preflight target format/device/epoch and feature support, restore touched host state, retain CPU/GPU pins through exact query completion. |
| Diagnostics | typed status and bounded counters -> private/public-safe evidence | No model payload, private paths, identifiers, image bytes or derived metadata in public logs/CI. |

The proposed `ModelPackage` contains a normalized reference graph, validated texture metadata, model bytes and optional linked behavior descriptors; no GPU handles. `ParameterFrame` contains typed finite values, source/pose generation and host timestamp. `DrawableSnapshot` contains stable drawable identity, full vertex/UV/index streams, texture index, order, visibility/opacity, mask membership/inversion, supported blend/color operations and generation. `RenderSubmission` pins snapshot, texture resources, target epoch and exact query until retirement. These are **private conceptual interfaces**, not changes to `include/ars/ars.h`.

The existing P1 `Session` supplies only selected `.moc3` opening, checked parameter update and an immutable drawable snapshot. It does not yet supply manifest/texture admission, linked physics or a D3D11 consumer. The existing private dual-mode host supplies a useful target/query transaction, but integration must demonstrate its ownership rules for this new representation. Neither boundary should be mistaken for a complete public SDK path.

### Frame order and failure semantics to freeze

`ingress/authz -> core arbitration -> semantic-to-MOC parameter mapping -> optional linked motion/expression policy -> physics input sampling -> bounded fixed physics steps -> physics output application -> MOC update -> immutable snapshot publication -> render admission/draw -> query retirement` is the **proposed order**, not a proven equivalent to any author's player. The design review must freeze which component owns each parameter and the priority of manual, motion, expression, physics and idle values. Explicitly handle duplicate outputs, missing IDs, out-of-range values, source expiry, clock rewind and skipped/catch-up steps. A reload resets or transfers state only by an explicit policy; never reuse stale physics state against a new model generation.

States are `UNLOADED -> VALIDATING -> READY_CPU -> READY_GPU -> ACTIVE`, with `FAILED` before commit and `QUARANTINED` for unresolved device loss. A candidate package/snapshot/GPU bundle becomes visible only after its validation and resources are complete. `CLOSING` denies new admission, then retires in-flight work; pending query means BUSY, not completion. Wrong-thread/device and stale-generation calls return typed errors before touching host state.

## 4. Fidelity and proof hierarchy

| Claim | Direct oracle and minimum evidence | What cannot substitute |
| --- | --- | --- |
| Format/admission compatibility | Declared manifest graph and MOC feature inventory; valid/invalid public fixtures and private source-pinned records; exact rejection reason. | MOC magic/header or a successful parse alone. |
| Evaluator geometry parity | Full drawable IDs, parameter values, vertices/UVs, indices, order, opacity and feature flags for bounded poses from two independent evaluators, after explicitly declared coordinate/UV/winding normalization. | Truncated exports or comparing one viewer screenshot. |
| Host renderer correctness | Numeric D3D11 WARP readbacks against generated reference images for supported features, plus named hardware repetition; exact target state before/after failure. | External viewer pixels or CI compile success. |
| Author-intended visual fidelity | Rights-cleared independent reference renderer/output for the same model, pose, viewport and color conventions, with tolerances frozen before comparison. | Evaluator agreement or a plausible-looking private image. Without a reference, this claim remains HOLD. |
| Physics3 behavior | Linked-file admission, deterministic input/on/off/release/reload traces and measured geometry/pixel response at fixed timestamps; reference trace needed for equivalence claim. | Toy spring simulation or an unrelated sidecar. |
| Lifecycle/device recovery | Host-integrated exact query/resource identities, resize/close/fault packet and separate physical-loss packet on a disposable runner. | Injected loss alone cannot close D06/D08. |
| Product release | Dependency/model redistribution review, parser trust decision, public-input isolation, exact-head gate packet and explicit activation decision. | A merged stack, green CI or local private run. |

Before any golden-image verdict, freeze viewport/camera/model transform, target format and clear color, linear/sRGB treatment, premultiplied alpha equations, UV origin/sampling/filter/address behavior, triangle winding/culling, mask creation and inversion, blend equations, draw order/tie-breaking and readback tolerance. Include public fixtures that make each convention visibly distinguishable; a blank or all-opaque image is an inadequate oracle. The manifest reference graph follows the [official `model3.json` file format](https://github.com/Live2D/CubismSpecs/blob/master/FileFormats/model3.json.md); this does not imply that the manifest specifies MOC deformation or renderer behavior. The [independent evaluator's sample viewer](https://github.com/SakuraMotion/PurismCore/blob/master/docs/VIEWER.md) explicitly documents missing physics and incomplete rendering features, so its screenshots are diagnostic, not an authoritative pixel oracle.

## 5. Budgets, security and rejection matrix

Separate limits are required for MOC input, parser/evaluator transient allocations, model state, snapshot streams, manifest/references, encoded PNG, decoded pixels, staging buffers, resident atlases, masks/offscreen intermediates, frame backlog and in-flight GPU bundles. Use checked arithmetic before allocation and record peak memory. An approved per-model policy is set after profiling representative, authorized models; **do not silently raise the global ARS2D budget** or invent an FPS/memory target here. The workload target is a bounded steady-state update/render without unbounded accumulation, measured under a named machine/build/driver and fixed scene.

Admission negative cases: absolute/drive/UNC/traversal paths, symlink or canonical-root escape, duplicate/case-aliased references, missing or extra required texture indices, corrupt/truncated/unsupported MOC, malformed JSON, oversized/decompression-bomb PNG, nonfinite or excessive counts, unsupported mask/offscreen/blend feature, invalid physics IDs/ranges/steps, and failed allocation at each boundary. Tests must assert no partial publication and no GPU upload on rejected CPU input. Public arbitrary-file acceptance remains HOLD pending parser hardening, fuzzing, process-isolation decision and dependency/license review; trusted-local P1 behavior is not evidence of public safety.

Runtime negative cases: repeated parameter application, input expiry/release, update clock rewind, frame backlog, concurrent renderer read, wrong thread/device, stale snapshot/target epoch, resize during pending work, GPU creation failure, failed query, close while in flight and injected device loss. Preserve old committed output when safe; quarantine the affected generation if completion or device state is unknown. Do not force a real TDR on the shared workstation. D06/D08 physical loss remains HOLD until a disposable reset-capable Windows GPU runner exists.

## 6. Risk-first delivery sequence

| Stage | Reviewable artifact and dependency | Positive and negative exit evidence | Stop / rollback |
| --- | --- | --- | --- |
| F0 target freeze | This contract, feature matrix, frame order, color/mask equations, reference-oracle plan and owner decision on compatibility scope. | Reviewer can map each intended output to a direct test and identify every unsupported feature/claim. | Unresolved meaning of “correct render” or parameter ownership: no renderer fidelity verdict. |
| F1 admission + evaluator completion | Builds on current P0/P1; manifest-linked package and texture metadata, full feature inventory, bounded snapshot and parser trust plan. | Public synthetic manifest/hostile corpus plus rights-cleared MOC3 or opt-in private evaluator run; full pose exports/repeat semantics; injected CPU allocation rollback. | Unsupported required feature, unsafe path or unaccounted transient allocation: retain default OFF. |
| F2 host pixel path | Requires F1; texture residency and D3D11 offscreen consumer under host epoch/query ownership. | WARP numeric atlas/alpha/order/mask/blend pixels, wrong-thread/stale target/resize/OOM/state-restore negatives; named hardware repeat where available. | Pixel mismatch or unretired resource: no renderer claim. |
| F3 dynamic behavior | Requires F1/F2 and frozen parameter order; linked Physics3 first, expressions/motions only when explicitly selected and supported. | Fixed-clock on/off/release/reload traces, changing geometry **and** host pixels, bounded soak, parameter conflict/clock fault negatives. | No linked physics trace or evaluator handoff proof: do not claim Physics3 fidelity. |
| F4 product integration decision | Requires F1-F3; core Character/capability/ABI/packaging and trust design as a separate material change. | End-to-end host consumer, relevant G01/G02/G03/G05/G06/G07/G08 evidence, rights/dependency review and explicit feature-activation decision. | Any missing gate or authorization: leave optional feature OFF; D06/D08 physical HOLD remains independent. |

Each stage records parent/HEAD SHA, exact commands/exit codes, toolchain/driver, fixtures and provenance, positive/negative results and an honest HOLD list. F0 is a **design** verdict only; F1-F3 may be private technical proofs; F4 is a separate public-product decision. Stage completion never automatically promotes a baseline gate. A failure returns to the last working default-OFF slice and keeps the prior committed host scene/pose where the state is recoverable.

## 7. Traceability and current disposition

| ID | Requirement / source | Owning stages | Acceptance evidence | Current disposition |
| --- | --- | --- | --- | --- |
| MOC-FW-01 | User: complete framework and target output before further design | F0 | Reviewed journey, interfaces, alternatives, oracles and decision list | Authorized for staged implementation; F1 local CPU implemented and verified. |
| MOC-FW-02 | Derived: dynamic MOC evaluation within host-owned offscreen path | F1-F2 | Parameter-dependent snapshot and numeric host pixels | F1 CPU and F2 bounded host path PASS locally; continuous product integration HOLD. |
| MOC-FW-03 | Derived: linked deterministic physics | F3 | Fixed-clock/release/reload traces and pixels | Bounded trusted-local independent adapter PASS; author/Cubism equivalence and continuous product controls HOLD. |
| MOC-FW-04 | v0.1: validated assets, immutable pose and bounded ownership | F1-F4 | hostile corpus, no-upload rejection, generation/query pins | F1 package/pose/replacement and F4-A private core-to-host integration PASS; public SDK qualification HOLD. |
| MOC-FW-05 | Owner: private model cannot be published; no official SDK | All | ignored private packet, public-safe generated fixtures, dependency inventory | Active constraint. |
| MOC-FW-06 | Existing D06/D08 physical evidence requirement | F4 / separate runner | actual removal/recovery and post-loss cleanup packet | HOLD; no suitable runner. |
| MOC-FW-07 | Optional future public arbitrary-file import | F4 only | isolation/fuzz/licensing and explicit activation decision | Selected by owner / implementation and qualification HOLD. |

F1 trusted-local CPU acceptance is complete on its [historical tested source](audits/M2_MOC3_F1_LOCAL_SOURCE.json) and [actual local evidence](audits/M2_MOC3_F1_LOCAL_ACCEPTANCE.md). The owner selected this machine's authorized model run as primary evidence; GitHub Actions is corroborating, and lack of a redistributable MOC3 fixture limits only public CI coverage. F2 now has its own [source-pinned local host pixel evidence](audits/M2_MOC3_F2_LOCAL_ACCEPTANCE.md), including actual WARP/hardware and visible/API readbacks. F3 now has [source-pinned local linked-physics evidence](audits/M2_MOC3_F3_LOCAL_ACCEPTANCE.md): an independent fixed-step adapter, actual on/off/release geometry and host pixels, reset/physics reload and visible API proof. F4 public integration/activation remains open. Private persistent controller and fixed-clock continuity now have direct proof; public input ownership and quality-tier performance remain outstanding; authored-reference fidelity and physical-loss evidence remain separate HOLDs.

F4 now has a [bounded private core-consumer proof](audits/M2_MOC3_F4_LOCAL_ACCEPTANCE.md), using real Character/source authority and final semantic publication after GPU completion. This is F4-A only: the original F4 product exit criteria above remain open. See the [release/gate/custody ledger](M2_MOC3_F4_RELEASE_LEDGER.md) for public ABI, import trust, package, performance and physical-loss requirements.

Current F4 completion evidence: [controller, duration, installed bundle and bounded parser-preflight results](audits/M2_MOC3_F4_COMPLETION_PROGRESS.md). The owner selected public-user MOC3 input; [remaining public pipeline implementation](M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md) is mandatory before this can be called a public importer. Whole F4 and baseline gate closure remain HOLD.
