# Alpha #58: isolated logical sessions, ownership and implementation order

2026-10-20, Asia/Bangkok. R0 design dependency for
[issue58](https://github.com/boxs-51/Avatar_Runtime/issues/58), after
[issue57](https://github.com/boxs-51/Avatar_Runtime/issues/57).
Inspected source: `37ba1355f987a1d9fe14c97ab5654700e6c93b32`.
**R0 design contract. Runtime implementation is recorded separately in the
[R1-R5 acceptance](../completed/audits/ALPHA_58_SESSION_ACCEPTANCE.md); this
document and its original R0 receipt do not themselves prove execution.**
Owner instruction is to continue remaining issues under the
[alpha contract](ALPHA_LINEAR_BACKLOG.md). That authorizes staged implementation;
this file supplies no separate permission, public activation or release approval.

## R0 findings and existing proof

[Retained batch](M2_MOC3_PUB02_A_RETAINED_SESSION_DESIGN.md) and
[checkpoint bridge](M2_MOC3_PUB04_B_CHECKPOINT_DESIGN.md) are bounded inspected
methods. They establish retained evaluation within a batch and accepted solver
state across fresh jobs, respectively. Neither establishes a duplex persistent
worker. Trusted-local `scripts/moc3_core_session.py` evaluates model/physics in
a persistent local path; it is a diagnostic oracle, not public-input isolation.

`src/abi/runtime_api.cpp::ars_runtime_get_capabilities` advertises no graphics,
asset or motion capability. Source search of `src/abi` and `src/core` finds no
D3D immediate-context owner. There is no implemented general production graphics
host in those public modules. Therefore the inventory below covers current
repository private/native ownership, not unknown future SDK caller code.
[Issue38](https://github.com/boxs-51/Avatar_Runtime/issues/38) remains HOLD for
production ownership/barrier integration after public CPU/ABI work; absence of
a production host is not proof that every future context user has surrendered.

## Immediate-context and reference inventory

All calls below belong to the creating/render thread R unless a listed method
explicitly permits otherwise. Processes are separate ownership domains; a pipe
client/UI does not thereby own the native immediate context.

| Current owner/user | Source and references | Close/replacement obligation |
| --- | --- | --- |
| Publication adapter | `src/moc3/packet_publication.hpp/.cpp`, PacketPublication owns device/context ComPtrs, private host, core, fence, accepted checkpoint/bytes; accepted_texture is a borrowed identity | Gate command admission, cancel pending fence, retire exact query or quarantine unknown completion, close host on R, then surrender adapter references |
| Dual-mode renderer | `src/rendering/d3d11_dual_mode_host.hpp/.cpp`, owns HostDevice, active texture/RTV and Pending query/scene/fence | begin_close alone is atomic/off-R; poll_close/detach_lost stay R. Private detach proves only this domain, not external host users |
| Backend | `src/graphics/d3d11/host_device.hpp/.cpp`, retained device/context and shader/state objects, borrowed context getter | Owner-thread validation/loss latch; stop all users before release_owned_references; no context reference may escape an admitted R operation |
| Query retirement | `src/graphics/d3d11/query_retirement.hpp/.cpp`, retained device/context/target/query and snapshot/frame pin | Exact generation/query completion releases pins; unknown/lost completion quarantines until an established teardown barrier permits detach; never fabricate completion |
| Pipeline restore scopes | `src/graphics/d3d11/state_scope.hpp/.cpp`; MOC3 renderer ExtraScope in `src/moc3/d3d11_renderer.cpp`; borrowed context plus retained Get* resources/state | R-stack lifetimes must finish restoring/releasing before host receipt; no scope may survive a callback/close boundary |
| Render frame/cache | `src/rendering/d3d11_scene2d.cpp`, `d3d11_scene3d.cpp`, `src/moc3/d3d11_renderer.hpp/.cpp`; frame resources pinned by query; AtlasResidency is package/device-bound | No reuse across package/device generation; invalidate cache, retire or quarantine all frame pins before surrender |
| Diagnostic executable owners | `src/moc3/packet_publication_host.cpp`, `packet_render_host.cpp`, `pixel_bridge.cpp` own process-specific render loops/device or publication adapter | Pipe commands serialize on R; readback uses the same context. CLI/PPM output is not an installed public control API or mapped-window observation |
| Private teardown authority | `src/graphics/d3d11/teardown_receipt.hpp/.cpp`, PrivateTeardownHost owns device/context/RTV and tracks its admitted users | Authority/receipt is single-use generation/device/owner-bound. Current source does not automatically register every inventory row in one production barrier |
| CPU authority | `src/core/runtime_lifecycle.hpp`, RuntimeLease/FrameLease/StageLease/SnapshotLease; CoreConsumer and final PublishGate fence | CPU authority lifetime is separate from GPU completion. Final revoke/expiry check is mandatory even after a worker ACK or successful query |
| Script/UI observers | `scripts/moc3_publication_probe.py`, `moc3_core_session.py` and viewer/observer diagnostics | Pipe/process/readback owners, not native context owners. Close/reap child, bound queues and disclose whether evidence is isolated or trusted-local |
| Public/external host | Public capability is off; arbitrary installed-host context callbacks are not implemented/enumerable here | Future #59/#60/#38 implementation must enumerate/register them before issuing a production teardown receipt |

The inventory was obtained from declarations and call sites for HostDevice,
D3D11DualModeHost, QueryRetirement, PacketPublication, ID3D11DeviceContext,
D3D11CreateDevice and context getters across `src`. It is a source audit, not
runtime census, race proof or installed-host certification. Tests/examples are
separate executables, not hidden production barrier participants.

## Resource policy and chosen architecture

| Boundary | Preserved rule |
| --- | --- |
| Isolated worker process/job |512 MiB process AND job memory, one active child, user CPU2s lifetime, wall5s lifetime, kill-on-job-close, restricted handles/token/AppContainer |
| Worker transport |192 MiB aggregate output per job; exact framing/EOF; no paths/pointers/host handles or model IDs as authority |
| Current bounded formats |1..8 steps, local clock<=1s, backlog<=8 fixed16,666,667ns steps; bindings<=32; checkpoint<=128 KiB, chains<=128, particles<=2048 |
| Accepted host state | Immutable bounded checkpoint + core/frame generation; one pending candidate; no unbounded history/queue or saved model-derived evidence in public artifacts |
| Input lease | Host monotonic deadline, existing100..2000ms bounds; no renewal from a worker timestamp/ACK, package replacement or process rotation |

Native enforcement source is `src/moc3/import_sandbox.cpp` (JobObject limits,
user CPU accounting, elapsed wall checks and output cap).
`src/moc3/physics_checkpoint.cpp::walk` also enforces generation<=8 and clock<=1s;
`PacketPublication::prepare_request` has the bounded request/lease constraints.
These are hard current behavior, not adjustable defaults.

Chosen direction: a long-lived **logical owner session** with bounded isolated
worker leases. Each lease may retain one decoded evaluator and admitted atlases
across multiple commands, but must exit/reap within its unchanged cumulative
CPU/wall/output caps. Rotate at an accepted checkpoint; if the next lease cannot
start inside policy, retain last accepted frame/state and report unavailable.
Do not reset JobObject accounting, extend deadlines or run overlapping workers
to hide a cap. Decode/reload cost and original large-model admission can still
make a session unsupported; qualification must measure that directly.

Alternatives: a permanently live worker would violate the5s lifetime limit;
raising/removing that limit is outside this chosen implementation. Reparse per
step keeps existing PUB04-B behavior but does not meet retained-evaluator goals.
Logical rotation preserves isolation and acceptance atomicity at the cost of
reload latency. No performance improvement is claimed before measurement.

## Versioned protocol boundary

Do not relax ARSPHYS1/ARSCAND1 decoders or relabel accepted checkpoint bytes in
the trusted host. Their existing generation/clock bounds and negative oracles
remain intact. A new opt-in protocol must distinguish logical session/package
identity, worker epoch, local step/clock, global accepted generation/time,
control revision and accepted-predecessor digest. Checked integer arithmetic,
finite numeric/shape bounds, exact lengths/EOF and reserved-zero fields are
mandatory. Legacy decoders reject the new version; new code supports explicit
legacy conversion only inside the isolated evaluator with a validated origin.

R1 must freeze the byte layout and semantic transition oracle before transport
code. Global time is metadata for a logical session, not permission to increase
per-lease clock/backlog or the process budget. On rollover the worker must ACK
the exact accepted predecessor plus the explicit epoch transition; the host
cannot synthesize solver state, mutate checkpoint clocks or use an unaccepted
candidate as a predecessor. Cross-package predecessor, epoch reuse, global/local
clock mismatch, stale control revision and replay are rejected before GPU work.

## Owner, input and package transitions

* One trusted session owner serializes the command stream; worker epochs and
  catalog slots are admission-local identifiers, not transferable authority.
  Manual values are complete snapshots: omission/release returns defaults, no
  implicit hold-last-input. Reject duplicate slots, nonfinite/range violations,
  stale package/owner revision and unknown fields before transport.
* Expiry/cancellation/revoke wins at the final core fence. Controls are captured
  immutably for one candidate. Release changes the next candidate input, not
  published particles out of band; explicit reset changes candidate solver state
  only and becomes visible only if checkpoint/core/frame all commit.
* At most one prepared/GPU-pending candidate. Query completion alone cannot
  publish. Acceptance order is typed validation -> GPU candidate/exact query ->
  final core authority fence -> nonthrowing checkpoint/core/frame publication.
  Worker acceptance ACK and process liveness never authorize the final commit.
* Retained worker accepted and prepared solver state are distinct. It may not
  process the next command from speculative particles belonging to an unaccepted
  candidate. Reuse requires the trusted host to send ACCEPT for the exact
  session/package/epoch/candidate/checkpoint digest only after the final host
  commit. This enables the next command; a worker ACK is never a prerequisite
  or fallible operation in the already completed host publication. Missing or
  failed ACCEPT delivery/ACK forces lease termination/reap and restoration from
  the authoritative host-accepted checkpoint in a fresh epoch. On any rejected,
  cancelled, expired or revoked candidate, terminate/reap that lease rather than
  trusting a remote rollback. Late output cannot reopen it. No subsequent command
  may inherit rejected speculative state, and a worker response cannot authenticate
  host authority or substitute for the final core fence.
* Package replacement invalidates old manual bindings and any unaccepted old
  candidate, but retains the old accepted frame/state until new admission and a
  default/reset candidate commit. New package admission uses its own identity;
  predecessor/solver state cannot migrate across packages. Failure preserves the
  old accepted snapshot; late output from old epochs is ignored/rejected.
* Owner cancellation/close closes command admission first, terminates/reaps its
  isolated job, drains completed queries or quarantines unknown ones on R, then
  releases caches/host references. No new input, readback or callback enters the
  context while teardown is pending. CPU/core close and GPU teardown each need
  their own evidence. #38's production receipt integration is not implied.

## Ordered implementation and direct acceptance

| Slice | Deliverable | Required proof before moving on |
| --- | --- | --- |
| R0 | This source-grounded inventory, policy and implementation contract | Separate source/doc review, links/catalog/custody; design PASS only |
| R1 | Versioned bounded duplex lease framing + native typed session state/control validation, isolated evaluator retained within a lease | Generated codec/negative matrix, real AppContainer command sequence, one decode per admitted lease, cumulative job limits/cleanup, old modes unchanged |
| R2 | Prepare/cancel/expiry/revoke/reset/release through real query/final-core fence | Exact previous checkpoint/core/frame bytes retained on each failure; late revoke after exact query; rejected lease reaped; reject-then-continue/rotate particles and pixels match an independent accepted-only sequence at identical accepted timestamps (rejected command omitted); WARP/hardware real pixels |
| R3 | Accepted-checkpoint epoch rotation and crash recovery | Real second worker ACK; continuous independent solver comparison across rollover, process crash/timeout/reload, no unaccepted state/time relabel; lost ACCEPT/ACK after host commit restores that exact accepted checkpoint; rejection never becomes the next solver baseline; caps remain enforced |
| R4 | Package replacement and catalog/owner generation lifetime | Two packages, failed/late replacement, stale controls/ACK, release/reset/default and no cross-package state leak; child/process/COM cleanup |
| R5 | Agent observation API and mapped interactive viewer through the same isolated path | Agent calls update/status/readback while owner can observe window; stale/revoked/closed requests rejected; direct pixels/physics/current generations agree; source/binary provenance |

R1..R5 were **NOT IMPLEMENTED by R0**; see the separate current acceptance above. Begin with a failing executable scenario
where applicable; never substitute a model-authored receipt, static source pin
or CI green for an actual worker/query/window observation. Public generated
controls run in CI/local; the permitted private local model may be used only in
ignored local evidence. Unsupported full-resolution admission remains explicit.

## Gate and issue disposition

R0 design completion means goal fidelity, inventory/contract artifacts,
source/design consistency, unchanged constraints, attributable inspection and
independent review; it does not mean runtime behavior. Final R0 commit/PR records
its scoped review and custody receipt. #58 stays OPEN until all R1..R5 acceptance
is directly observed and independently reviewed. #59 public ABI stays dependent;
#38/#39 production integration, #41/#42/#61 qualification and #62 release remain
open with their own evidence obligations. Physical D06/D08 still needs a separate
disposable GPU runner; no destructive reset is authorized here. Full product
remains UNVERIFIED and public activation stays off.

## R0 verification receipt

Scoped design verdict PASS after independent source/architecture review. Review
identified a P2 omission around speculative retained solver state; the accepted/
prepared-state and reject/reap/restore rules and R2/R3 continuation controls above
resolve it. Architecture/project boundaries SUPPORTED; optimization
NO_RECOMMENDATION. No runtime execution was claimed for this design phase.
Alpha custody regressions:15/15 PASS, exit0 (`build/alpha58-r0-custody-tests.log`);
catalog151 entries and341 relative links resolve. Actual committed five-file
scope and integration receipts are recorded in the R0 PR/parent issue. G1-G7
apply to this design dependency only; full #58 runtime was NOT IMPLEMENTED at that R0 checkpoint.

## #38 ownership implementation checkpoint - 2026-10-21

The R0 inventory above is historical. [#38 design](ALPHA_38_PRODUCTION_OWNERSHIP_DESIGN.md)
and [direct acceptance](../completed/audits/ALPHA_38_PRODUCTION_OWNERSHIP_ACCEPTANCE.md)
now enroll the actual packet/lease adapters, host/backend, frames, query, restore
scopes, cache and readback into one canonical generation-bound native domain.
Actual runtime work stays pinned until reference surrender and owning-host receipt
consumption. The current native graph has direct healthy/injected WARP/hardware
proof. Public graphics remains OFF; unknown future installed callers must enroll
before receiving any production teardown guarantee. This does not close #39's
committed pose/camera matrix or physical D06/D08.
