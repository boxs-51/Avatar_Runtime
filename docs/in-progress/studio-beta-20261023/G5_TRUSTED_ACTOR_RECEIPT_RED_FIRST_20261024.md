# G5 T2: Trusted Actor and Receipt Identity — RED-first Gate

**Date:** 2026-10-24  
**Repository:** `boxs-51/Avatar_Runtime`  
**GitHub baseline inspected:** `f41f9a37314425b15e011fbc1cf154ffba3f1401`  
**Work branch:** `exp/g5-local-cas-transactions`  
**Stage:** TEST-ONLY / CONTRACT RED-FIRST — NOT COMPILED; Native/GUI evidence NOT_RUN.  
**Safety:** G5 V2 writer remains disabled; no edits to shipping Studio, public ABI20, real model, IPC endpoints, or `main`.

## Confirmed source-level blocker

The internal G5 command bus currently derives its entire draft owner identity from the ingress **category**, using `const uint64_t owner=(uint64_t)cmd->ingress` in `studio_g5_bus.c`. This collapses all Agent clients to the same `G5_AGENT` owner. The separate `quarantined_request[(int)ingress]` marker is likewise shared by all Agents. The recovery path `g5_bus_recover(... ingress ...)` has no distinct authenticated client identity.

While these functions are intentionally **not linked into the live Studio target**, activating an Agent writer on this contract could allow one Agent to act as another's draft owner, confuse post-effect receipt ownership, or block another Agent's recovery. This is a **P1 gate for future external write enablement**, not a claim of a currently exploitable live G5 endpoint.

The current receipt stores `owner, request_id, revision, outcome`, but not the original operation or canonical payload identity. All duplicate IDs are rejected, which is conservative; however, a post-ACK reconciliation cannot reliably prove that the returned result belongs to the same intended operation/payload. This is **G5-ACK-02**, another release blocker.

## Frozen test-first requirements

| Gate | Required behavior | Observable oracle |
|---|---|---|
| G5-ING-01 | Each trusted Agent session has an opaque identity distinct from ingress category | Agent B cannot edit/abort Agent A's draft even when both have ingress `G5_AGENT` |
| G5-ING-02 | Trusted Host binds ingress; caller JSON cannot promote Agent to UI | Forged UI command is rejected without mutating the private draft |
| G5-ING-03 | Lost-ACK quarantine belongs to authenticated owner, not whole Agent class | Agent A's uncertain operation cannot be reconciled by Agent B or block unrelated B after A's lease is released |
| G5-ACK-02 | Commit receipt records owner, operation and fingerprint of canonical original payload at the commit linearization point | Owner A reads immutable COMMITTED receipt for exactly the right request/operation/payload |
| G5-ACK-03 | Unknown request never means NOT_APPLIED and cannot authorize mutating retry | `UNKNOWN` keeps quarantine; no second edit/undo occurs |
| G5-SEC-04 | Session epoch/model generation remain authoritative | Old session, wrong model or unauthorized owner cannot read or mutate |
| G5-COMPAT-05 | Legacy API cannot provide an alternate unbound bypass | Only single Native owner-thread dispatcher can authorize state-changing edits |

Suggested internal API shape under review: `G5TrustedActor { ingress, owner }` with `g5_bus_submit_trusted`, `g5_bus_recover_trusted`, `g5_bus_mark_unknown_trusted` and `g5_bus_reconcile_trusted`. The `owner` member must be derived from a verified Host-owned session/connection, not accepted as an arbitrary request field. A cryptographic `payload_sha256[32]` must be computed from a **versioned, canonical semantic representation**, not raw C struct bytes (padding, float representation, unspecified members) and not a caller-asserted hash. Its digest is private internal metadata; no raw geometry, model paths, or secrets in GitHub logs.

The old entry points must not remain as a separately exposed unbound mutation/recovery path after integration. An authenticated owner identity is a logical authorization control within the local process; inherited Windows user privileges are not an OS sandbox.

## RED-first artifact and result

`tools/avatar-studio/tests/studio_g5_actor_receipt_redfirst.c` is opt-in and deliberately **not registered in default CMake/CTest**. It tests independent Agent draft ownership, forged ingress denial, owner-scoped post-effect ACK reconciliation, nonzero operation/payload-bound receipt, and UNKNOWN quarantine.

**Evidence level:** Source inspection of actual pushed GitHub HEAD, and file creation on the feature branch. **Native compilation: NOT_RUN** because the Windows Host Computer Bridge tunnel was unavailable and GitHub Actions were previously quota-blocked. The expected compiler failures from missing proposed symbols have **not** been observed in a real build here; do not label them observed RED.

Existing Windows synthetic Native G5 5/5 PASS and Python 211 PASS/18 SKIP refer to the **prior** local revision and do not certify this new test or a repaired actor-identity implementation.

## Next authorized development gates

1. Review/freeze the trusted actor contract against Local-only ADR-004 and Studio owner-thread requirements.
2. Compile this opt-in test on the same Windows Native toolchain to capture the actual RED; add healthy controls and denial matrix.
3. Implement actor-bound dispatch, actor-scoped quarantine, canonical payload/operation receipt and strict recovery; ensure no old caller bypass.
4. Run RED → GREEN, retained G5 five Native tests, Python regression, ABI20 C11/C++20, and physical lost-ACK test (separate read-only IPC with real Host).
5. Validate GUI/V1/Agent routing and accepted FrameCoordinator; keep writer disabled and no merge until gate and independent review are complete.

**Delivery decision:** G5 trusted ingress/receipt identity **REVISE / tests-only**. G5 Native external writer **HOLD**. No `main` merge, deployment, or public ABI20 change.