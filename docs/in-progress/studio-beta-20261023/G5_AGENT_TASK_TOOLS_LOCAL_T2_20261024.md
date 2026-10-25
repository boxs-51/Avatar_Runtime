# G5 Agent Task Tools — Local T2 integration (2026-10-24)

**Repository:** `Avatar_Runtime` — `exp/g5-local-cas-transactions`
**Owner goal:** Let an Agent call practical Studio/G5 tools, not register/manage Agents or decide which Agent may call a public API.
**Delivery tier:** Python callable facade + actual Windows Native C11 **synthetic fixture**; NOT production Studio/V2 writer enablement.

## Functional G5 tool contract

| Agent task tool | Arguments (all required) | Native fixture action |
|---|---|---|
| `draft.begin` | `expected_revision` | `G5_BEGIN` |
| `draft.anchor.set` | `draft_id, vertex, enabled` | `G5_ANCHOR` on private draft |
| `draft.validate` | `draft_id` | `G5_VALIDATE` |
| `draft.commit` | `draft_id, expected_revision, request_id` | CAS `G5_COMMIT` |
| `draft.abort` | `draft_id` | `G5_ABORT` |
| `draft.undo` | `expected_revision, request_id` | `G5_UNDO` |
| `draft.redo` | `expected_revision, request_id` | `G5_REDO` |
| `draft.status` | none | Read-only revision/lease/history metadata |
| `draft.receipt.status` | `request_id` | Read-only committed/UNKNOWN receipt |
| `draft.reconcile` | `request_id` | Explicit matching-receipt recovery; no replay |

These ten operations are exposed in `scripts/studio_g5_tools.py` as `G5DraftTools` and `g5_tool_catalog()`. The caller provides a local `session_epoch` and `model_generation`, both transaction consistency fences. No Agent registration, accounts, Agent identity administration, network listener or public ABI20 additions are introduced.

### Example: test-only synthetic Native G5 workflow

```python
from scripts.studio_g5_tools import G5DraftTools
from scripts.test_studio_g5_agent_tools_native import NativeFixture

fixture = NativeFixture(epoch=32001)  # model-free Windows test DLL only
tools = G5DraftTools(fixture, session_epoch=fixture.epoch, model_generation=55)
before = tools.status()
draft_id = tools.begin(expected_revision=before["document_revision"])["draft_id"]
tools.anchor_set(draft_id=draft_id, vertex=2, enabled=True)
tools.validate(draft_id=draft_id)
result = tools.commit(draft_id=draft_id,
                      expected_revision=before["document_revision"],
                      request_id=701)
receipt = tools.receipt_status(request_id=701)
tools.undo(expected_revision=result["document_revision"], request_id=702)
```

**Important:** The example uses only synthetic Rig data. Do **not** connect the test DLL to a private model or shipping Studio. The `G5DraftTools` constructor refuses an absent or non-fixture backend; this is a test-only guard, not cryptographic authorization. The real Studio/V2 mutation capability remains disabled. The current V1 `AgentStudioTools` API continues to work separately and is not silently rewritten to call G5.

## Transaction semantics and privacy

- Every mutation passes the native revision/session/model fences. No implicit retry, no replacing a failed G5 call with direct V1 write.
- `UNKNOWN_OUTCOME` raises a dedicated exception for mutations; record the original `request_id`, query `draft.receipt.status`, and **never** assume UNKNOWN means NOT_APPLIED. Caller decides follow-up based on supported verified recovery.
- Read-only `draft.status` includes `document_revision`, `applied_revision`, draft presence, draft ID, base revision and history counters. It does **not** export geometry, private model IDs, textures, file paths or captures. COMMITTED does not imply APPLIED to an actual XPBD/GPU frame.
- Python validates exact operation fields/types, forbids `agent_id` fields, rejects unsupported commands, reserved values, malformed receipts and unexpected model-derived fields, and never retries timeout.
- The optional Native fixture exposes the actual G5 transaction core through C DLL/ctypes, not an in-memory Python stand-in. It is model-free and **not** wired into Studio. `G5_NATIVE_TOOL_FIXTURE` tests may run locally; no public SDK install/export.

## Exact verification commands

```powershell
python -B -m pytest -p no:cacheprovider -q tests/moc3/test_studio_g5_tools.py
cmake -S tools/avatar-studio -B build/avatar-studio -DSTUDIO_G5_ACTOR_RED_FIRST=ON -DSTUDIO_G5_IPC_FIXTURE=ON
cmake --build build/avatar-studio --config Release --target studio_g5_ipc_fixture
python -B -m scripts.test_studio_g5_agent_tools_native
ctest --test-dir build/avatar-studio -C Release -R "^g5_" --output-on-failure
```

## Release blockers retained

1. Native `Studio` UI, V1 Agent IPC and future G5 tools are not routed through one verified owner-thread controller; legacy bypass remains.
2. Production Host-provided session/generation binding and read-only recovery IPC are not connected to the real Studio.
3. Physical UI/Agent concurrency, same-frame ABA and XPBD/GPU accepted-frame proof are not complete.
4. `G5DraftTools` is a **local test contract**, not production write authorization; no public network API/Agent governance required.
5. SDK packages, public logs and GitHub artifacts must comply with [ADR-005](../../reference/ADR_005_MODEL_INFORMATION_NONPUBLIC_AND_SDK_PACKAGING.md): do not expose non-public model information. Local legitimate editing is unaffected.

**Gate decision:** Native G5 callable workflow can be tested; G5 Studio writer remains **HOLD**. No merge to `main` or public SDK release is implied.

## Observed Windows verification (2026-10-24, local uncommitted tree)

- RED-first Python contract: ModuleNotFoundError before the Agent G5 tool facade existed; post-implementation pure Python schema/unit tests GREEN.
- Python/Native DLL integration: G5_AGENT_TOOLS_NATIVE_PASS on Windows Release, including begin, anchor, validate, commit, read-only status/receipt, undo, redo, abort, stale session/model, CAS and COMMITTED != APPLIED.
- Native CTest: 10/10 PASS with STUDIO_G5_ACTOR_RED_FIRST=ON and STUDIO_G5_IPC_FIXTURE=ON. The tenth case is g5_agent_tools_native.
- Python regression: 253 PASS / 19 SKIP / 204 subtests PASS using tests/moc3 and tests/test_model_publication_guard.py.
- Top-level Windows CTest/ABI20: 10/10 PASS.
- git diff --check: PASS; no source change in public include/ars/ars.h, src/abi, tools/avatar-studio/studio.c or V2 read-only parser.
- Native fixture and helper are uncommitted local code until a separate publish instruction. No push/PR/merge.
- Synthetic status is a bounded metadata snapshot, NOT the live Studio UI or accepted XPBD/GPU frame.

## Two-process Agent Tools transport gate (2026-10-24)

**Goal:** Connect ten callable G5 functions to a serialized Native owner
through a separate bounded **test-only** interprocess channel, without
activating G5 writes against the real Studio or building Agent governance.

- Added `scripts/studio_g5_fixture_ipc.py`:
  `FixtureIpcBackend` implements `call_g5` using a synthetic fixture
  directory. The Native owner process loads the existing model-free G5 DLL.
- `mutation/` and `recovery/` are physically separate filesystem lanes.
  Recovery accepts only `draft.status` and `draft.receipt.status`. Sending
  `draft.undo` to recovery is rejected without changing the revision.
- Messages are bounded to 512 ASCII JSON bytes, exact tool fields, correlation
  tokens, and no Agent-ID/path/geometry fields. Duplicate JSON keys,
  non-string operations, invalid bytes and mismatched tokens fail closed.
- Mutation writes a local intent marker and obtains the exclusive file writer
  claim before publishing a command. Missing ACK leaves the mutation lane
  quarantined; the recovery lane remains read-only and accessible. Status
  readback does not silently clear quarantine.
- `scripts/test_studio_g5_agent_tools_ipc.py` starts a separate Windows
  process with the synthetic Native DLL and verifies begin, anchor, validate,
  commit, undo, redo, abort, read-only status, and dropped post-commit ACK.
- `tests/moc3/test_studio_g5_fixture_ipc.py` covers strict input/output
  validation; `g5_agent_tools_ipc` is registered in CTest only under
  `STUDIO_G5_IPC_FIXTURE=ON`. No production SDK installation or Studio
  writer linkage is created.

**RED → GREEN:** A malformed non-string operation originally caused
`TypeError: unhashable type`; parser checks its type before dictionary lookup.
The corrected parser tests and two-process G5 CTest are GREEN.

**Evidence tier:** Windows real two-process transport, **synthetic Native
Rig only**. It does not prove production Studio IPC, host authentication,
OS sandboxing, crash-durable exactly-once, live GUI/Agent ownership or
XPBD/GPU frame acceptance. The Studio G5 writer remains HOLD.


## Explicit lost-ACK reconciliation (Windows synthetic owner)

The tenth Agent tool, `draft.reconcile(request_id)`, explicitly resolves
an original operation after its matching Native COMMITTED receipt has been
verified. A read-only `draft.receipt.status` does not remove quarantine.
Wrong request ID is rejected with the marker intact. The original request
ID may be reconciled only through Native owner fingerprint/operation binding;
subsequent draft begin is then permitted at the current revision. The old
COMMIT is never retried. All these guarantees are scoped to the synthetic
Windows two-process fixture; no production Studio writer or crash-durable
exactly-once behavior is implied.
