# ADR-006 — Agent-callable Studio tools, not Agent identity management

**Date:** 2026-10-24
**Status:** OWNER-CLARIFIED / LOCAL-ONLY implementation guidance
**Repo:** `boxs-51/Avatar_Runtime` — G5 experimental branch

## Owner clarification and intent

The Agent-facing API exists to **help Agents perform Studio work**: inspect the scene, discover model/rig capabilities, issue existing bounded edits, configure physics, run validation and understand operation results. It is **not** a public API consumer registry, identity directory, entitlement manager, per-Agent accounting service or feature to decide which Agent may call each public operation. "Public API" in this context means an exposed, documented *software calling interface*; it **does not** authorize disclosure of private model information.

A call is an operation plus arguments and a structured result. Clients may use a local AgentBridge, the existing CLI or a future MCP function adapter where appropriate. Do not create a new HTTP server, external service or credential system merely to make this API callable.

## Requirements / boundaries

| Requirement | Contract |
|---|---|
| AGENT-API-01 | Provide one simple callable catalog grouped by task (studio, rig, layer, XPBD, parameters, parts), reusing the existing V1 validation source |
| AGENT-API-02 | Use already-supplied local bridge for V1 actions; return existing Native acknowledgment; never auto-retry a command after TimeoutError/UNKNOWN_OUTCOME |
| AGENT-API-03 | V2 local reads remain read-only and bounded, including model generation fences; do not activate G5 draft/commit paths from a convenience wrapper |
| AGENT-API-04 | Keep synchronization needed for a correct edit (owner-thread routing, CAS revision, generation fence, recovery receipt), **without** managing Agent registrations, agent identities, accounts or per-agent permissions |
| AGENT-API-05 | Maintain ADR-005: private models and model-derived details may be used locally but must never be automatically published in GitHub, logs or SDK assets |
| AGENT-API-06 | Reuse modules and independently test them where valuable; packaging does not introduce public ABI20 symbols or imply a G5 writer is approved |

## Current implementation (scoped)

`scripts/studio_agent_tools.py` supplies `available_tools()` and `AgentStudioTools(bridge)`:

```python
from scripts.studio_agent_api import AgentBridge
from scripts.studio_agent_tools import AgentStudioTools

bridge = AgentBridge(local_studio_ipc_directory)  # already enrolled local Studio
tools = AgentStudioTools(bridge)
catalog = tools.catalog()            # operation keys and required argument names
status = tools.call("studio.status") # uses existing validated V1 bridge
# Example of an existing V1 edit; only use when the running Studio is ready:
# tools.call("rig.anchor", vertex=3, enabled=True)
capabilities = tools.read_v2("avatar.capabilities") # read-only V2 discovery
```

The examples deliberately omit any machine-specific or private model path; the local IPC directory comes from existing setup. No Agent ID is requested or stored by this wrapper. The wrapper does **not** open the bridge, bypass validation, create an IPC endpoint, perform automatic retries or export received model data. Model-dependent return values are **LOCAL PRIVATE** unless separately reviewed for publication.

The existing `scripts/studio_agent_api.py` operation-field map is exposed as the pure `operation_fields()` function. The established `validate_command()` reuses that same map, avoiding schema duplication. Supported Studio/XPBD edits still pass through the current V1 bridge and Native validation; this wrapper is a convenience surface and does not certify that V1 operations are transactionally safe under concurrent G5 edits.

## Distinguish transaction control from Agent management

- **Keep:** a single Studio Native edit owner, stale model-generation checks, document revision CAS, serialized mutation queue, bounded request/receipt parsing, unknown-outcome quarantine and recovery.
- **Do not build:** Host Agent registries, client-registration lifecycles, per-Agent permission policies, Agent identity directories, multi-tenant public API administration or security claims based on caller-supplied identity.
- **Scope of a session:** an opaque transaction/model generation context necessary for *correctness*, not a customer/Agent account.
- **Local privacy:** no external publication of model, textures, geometry, screenshots, model identifiers, raw model logs or private model bytes. Existing legitimate local Studio/Agent access is unaffected.

A prior experimental RED-first Host ingress registry target created during exploration was **withdrawn before commit** in response to this owner clarification. No Host registry feature is included in this phase.

## G5 release gates unchanged

The internal `studio_g5_txn_core` is testable/reusable, but the G5 writer in Studio/V2 remains **HOLD** until real UI/V1/Agent edits share an owner-thread command dispatcher, physical recovery IPC handles lost ACK and crash, and accepted XPBD/GPU frame generation is demonstrated. Existing G5 actor-bound prototypes are **internal correctness test scaffolding**, not an authorization management feature or mandate to expose Agent identities.

## Evidence and next step

Tests: `python -B -m pytest -p no:cacheprovider -q tests/moc3/test_studio_agent_tools.py`. They cover catalog, valid existing bridge action, invalid operation rejection, disconnected bridge, unknown ACK non-retry, V2 G5 writer disabled, malformed receipt rejection and caller attempt to override the selected `op`. The last negative oracle was observed RED before validation was corrected.

Next: broaden Agent-callable tool documentation and response schemas from the already-supported V1/V2 functions, then integrate G5 transaction operations **as task tools** only after the existing physical concurrency and frame-acceptance gates pass. No model-publication exceptions or Agent identity-management project are authorized.

## Synthetic two-process IPC progression (2026-10-24)

For RED-first integration, callable G5 tasks pass through
`FixtureIpcBackend` into a separate model-free Native DLL owner process.
The test-only mutation and read-only recovery lanes verify bounded parsing,
serialized edits, receipt readback and lost-ACK quarantine. This does not
create a public endpoint or Agent administration layer, and does not enable
writing into the real Studio. See the [G5 tool checkpoint](../in-progress/studio-beta-20261023/G5_AGENT_TASK_TOOLS_LOCAL_T2_20261024.md).

## G5 function-call progression (2026-10-24)

`scripts/studio_g5_tools.py` now provides explicit function-level schemas
and callable helper methods for ten G5 tasks: begin, anchor, validate,
commit, abort, undo, redo, document status and receipt status, plus explicit reconciliation. The
`G5DraftTools` object is test-only and refuses non-fixture backends. Its
Python/ctypes integration tests exercise the **real synthetic Native C11**
G5 core, not Studio's production model or UI. All G5 writer gates remain HOLD.

See [G5 Agent task tools checkpoint](../in-progress/studio-beta-20261023/G5_AGENT_TASK_TOOLS_LOCAL_T2_20261024.md).
This is for Agent action support, not an Agent identity registry, user-facing
permission service or public API access manager.


## Real Studio local G5 HOLD handshake

The shipping Studio now implements the read-only `studio.g5.status`
request through its existing owner-thread V1 IPC handler. The callable
`AgentStudioTools.g5_status()` helper returns bounded LOCAL_PRIVATE
metadata, explicitly reporting writer HOLD, transaction not connected and
frame not verified. Its legacy Rig revision must not be interpreted as a G5
document CAS revision. See [G5 real Studio read-only checkpoint](../in-progress/studio-beta-20261023/G5_REAL_STUDIO_READONLY_STATUS_20261024.md).
It does not expose model data, activate G5 writes or introduce Agent
identities/permissions administration.

### Explicit recovery is a function, not Agent identity management

The test-only `draft.reconcile(request_id)` tool can close a quarantine
only after matching a Native COMMITTED receipt against the exact original
command. A read-only status never automatically unquarantines, and the
lost mutation is not replayed. This is not an Agent account, permissions,
or public API access-control subsystem. Studio G5 writer remains HOLD.

## Real Studio V1 owner dispatch and recovery

UI/V1 Agent Rig edits now use a shared candidate/commit owner-thread
dispatcher with revision fences. A separate real local recovery lane
supports original-token receipt lookups and explicit client reconciliation
after a verified FOUND status. Volatile UNKNOWN outcomes never permit
blind replay. This is task support for Agents, not Agent governance,
and does not activate G5Bus in the product or establish accepted GPU frames.
See [G5 shared dispatch and product V1 recovery report](../in-progress/studio-beta-20261023/G5_LIVE_RIG_DISPATCH_V1_RECOVERY_20261024.md).



## G5 T3 native candidate and accepted-frame boundary

The internal G5Bus can now stage a full validated Native Rig candidate
through a trusted local owner call, supporting additional editing functions
without any per-Agent governance or untrusted raw-model IPC. A separate
physical Studio legacy-frame witness lets an Agent inspect whether the
CURRENT legacy Rig revision was accepted by XPBD/render. It does **not**
claim G5 APPLIED. See the
[G5 T3 checkpoint](../in-progress/studio-beta-20261023/G5_T3_NATIVE_CANDIDATE_FRAME_WITNESS_20261024.md).
The real G5Bus writer, durable recovery and document-to-frame acceptance
remain gated; private model information stays non-public under ADR-005.
