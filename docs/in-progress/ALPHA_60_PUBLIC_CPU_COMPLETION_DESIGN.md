# Alpha #60: complete isolated public CPU lifecycle

Status: IMPLEMENTING; whole issue remains OPEN. Baseline: alpha `5d90997e3f1e6df0ed5bef2b77318f815aa17769`. Date: 2026-10-21. This continuation implements the remaining issue requirements; prior isolated verdict and typed 2D receipts prove only their scoped slices.

## Requirements and acceptance

- F1: owned typed native 2D, GLB and dual transfer, including default mode, original layer hierarchy, decoded accessor components and RGBA. Host independently validates metadata, graphs, materials, accessor domains, animation, skin, camera and mappings without encoded asset parsers. Preserve old 2D packet and native corpus behavior.
- F2: runtime-owned bounded context, model registry and generation-bound character/snapshot retention. Model user release must not invalidate existing characters. Reclaim outside ARS locks. Failure leaves outputs zero and previous state intact. Existing 256 MiB asset budget, 64 model, 16 character and authority limits remain.
- F3: explicit default-off Windows x64 CPU SDK through the unchanged 20-symbol C ABI. Load/release, character lifecycle, mode/layer, source administration, submit and update must work together. Render/drain remain unsupported pending the separate production rendering issues. Only actual motion ingress capability is advertised.
- F4: fixed helpers adjacent to the actual code module, owner-only fresh worker copy, bounded explicit-handle IPC and host-owned kill-on-close job. Admit only after independently observed process/job cleanup and packet validation. Unknown cleanup retains a runtime work pin in bounded quarantine and is reapable during close.
- F5: relocated installed C11/C++20 static/shared Release/Debug consumers; unchanged feature-off matrix and 20 exports; local hostile, OOM, callback, thread, lifetime, authority and real helper fault evidence. Independent exact-diff review before delivery.

## Typed GLB transfer

The full profile is separate from the frozen 2D profile. Transfer a typed metadata tree, decoded raw numeric accessor components (sparse overlays flattened by the worker), and decoded RGBA. Neither encoded BIN nor JSON nor PNG is accepted by the host. The host rebuilds GlbData using a typed-only compilation of the existing semantic validator. The Value implementation is split from parse_json to keep parser code out of the host closure.

Before traversing Value, validate a single reachable tree, unique object keys, canonical kind-specific fields, valid UTF-8, finite numbers, bounded nodes/depth/strings/work, no shared children and no cycles. Retain declared view offset/alignment/range checks against the logical buffer size. Sparse encoded index ordering is normalized away: the host validates sparse metadata bounds and independently validates the resulting dense component arrays, exact count/shape, integer domains/integrality, exact finite float32 values, min/max and every downstream semantic use. No claim is made that the host authenticates the discarded encoded sparse stream. Images must match declared count, dimensions and exact RGBA length within aggregate pixel limits. JPEG remains unsupported.

Preserve local and effective layer visibility/opacity, parent identity and default mode. Reject cyclic hierarchy; recompute effective values. Native assets permit 512 semantics, while authority supports 256: loading may retain the larger catalog, but public character creation must reject unsupported catalogs explicitly, without truncating or increasing authority caps.

## Runtime and publication invariants

Use the existing RuntimeRouter, handle registry and final authority fence. A character retention record is matched to its complete handle/generation and detached atomically before authority-slot reuse; shared model references are destroyed outside locks. Context destruction follows publication/snapshot retirement and precedes allocator/runtime destruction.

H controls and S updates exclude structural overlap; I submit stays concurrent. Bind S only at the first successful publication. Host allocation/free callbacks execute outside locks under reentry protection. Close must still invalidate pending publication. Every contributing cached or fresh source joins the final ordered authority lock set and epoch/TTL/clock/close checks.

Prepare retained partial-channel cache candidates before publication. A trusted internal no-allocation, no-lock, noexcept commit operation joins queue consumption and snapshot swap at the existing linearization point. Failed publication changes neither snapshot nor cache nor trigger queue. Confidence belongs to each retained channel. Highest priority contributors blend by confidence; quaternion hemisphere normalization and stable bool ties are deterministic. Triggers are fresh FIFO one-update pulses and are never retained.

## Helper custody and delivery

Static linkage resolves helpers beside the consumer executable; shared linkage resolves beside the runtime DLL. Ignore asset/env/PATH/cwd helper candidates. Install fixed helper targets and provide explicit copying for consumers, with matching CRT/configuration. Host outer job contains broker and worker; the inner worker keeps its existing 512 MiB, one-process, two CPU seconds and five wall seconds caps. Input/output are bounded to 16 MiB in this profile and asset jobs serialize. Broker-written cleanup reports are diagnostic only. Quarantine retains real OS handles, profile/temp identity and runtime lease until independently observed termination and cleanup.

Local evidence is authoritative; CI is a comparison. Do not close #60 from a codec test, isolated helper success, green CI or merged substep. D06/D08 physical GPU HOLD, main/public activation, SDK download and redistribution of private model assets remain outside this task.

## Implemented transaction and validation checkpoint

The public CPU path now prepares every live character into a compact bounded PublicBatch. Private Stage scratch is reused sequentially without raising the eight-stage, sixteen-character or sixty-four-snapshot caps. The final fence locks all affected poses and source records in canonical order, validates every generation/epoch/TTL/clock/close/queue head, then consumes queues, commits retained caches and swaps all snapshots in one no-fail operation. Successful S ownership and simulation time join that same operation. Batch runtime custody outlives every guard accessing runtime memory, including the last-pin close race.

Ingress and zero-time simulation updates use one exclusive host-clock sampling function and common highwater. The host callback runs outside ARS mutexes. Clock regression, callback contention and publication failure preserve snapshots, queues, caches and S/time ownership; existing path-specific exception statuses are preserved.

The public path explicitly permits a zero-channel static character; private validate_catalog defaults remain unchanged. Destroying a character closes ingress but retains caller-owned source admin records for explicit release. Source priorities use the descriptor copy supplied to authority registration. Registry type/runtime/status and retired-token SCOPE_DENIED semantics remain frozen.

Unknown helper custody is retained by runtime context/work lease or a single preallocated orphan slot with the global admission gate held. Management reaps either form only after actual process/job/profile/temp cleanup. Read-only attributes are normalized on the owned temporary worker copy. Real Windows fault and retained-work-pin trials have passed; receipts and exact scope are in the completion ledger.

The SDK exports fixed helper targets and ars_copy_cpu_helpers(target), selecting /MT or /MTd plus the matching iterator ABI. Shared consumers also copy the runtime DLL. Local installed/relocated static/shared Release/Debug C11/C++20 consumers have passed; final qualification must rebuild after any runtime correction. See [the evidence ledger](ALPHA_60_COMPLETION_PROGRESS.md) for current source attribution. This design does not itself declare integration or close any issue.
