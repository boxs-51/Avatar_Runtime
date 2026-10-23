# ADR-002 local-only G4 checkpoint, 2026-10-23

- Branch: exp/layer-physics-native-studio-local
- Baseline: 38e7dde8e52bd450ab3a2dcdff5362601744c152
- Authority: development self-review only; ADR approval pending
- Disposition: G4 PARTIAL GREEN, Studio RELEASE HOLD

## Architecture reconciliation

Latest updates to ADR_PROPOSED_AGENT_TOOLKIT_API_V2.md and
ADR_003_RIG_EDITOR_V5_DETAILED_DESIGN.md at 11:08 local time explicitly
supersede mandatory named pipes, broker, distributed journal, H5 plugin
integration and blanket prohibition of authorized local geometry reads.
Studio must remain single model/frame owner on the Windows Host.
Native grammar, bounded IPC, response ownership, model privacy and
accepted-frame invariants still apply. The proposed ADR originals and
auditor artifacts are unchanged and not in our commit.

## Implemented

- V2 accepts a caller-injected trusted-local V1 bridge ONLY when
  allow_local_private=True; otherwise remains offline discovery only.
- studio.local.status: finite, typed status and aggregate counts.
- catalog.parameters.page and catalog.parts.page: index-based, at
  most four values per call. Native receipt schemas strictly checked.
- Count drift during paging rejects as STALE_MODEL. Same-count model
  replacement remains undetectable without native generation fence.
- Bound capabilities/schema truthfully advertise additional read
  operations, never Native V2 mutation or H5 registration.
- LOCAL_PRIVATE classification; best_effort_unfenced_v1 consistency;
  original model bytes and asset paths not returned.

## RED-first and current evidence

Synthetic negative tests cover absent bridge, absent permission,
invalid pages, malformed native receipt, bridge timeout, count drift,
and truthful catalog discovery. Windows real-model E2E validated status,
two paged read classes and denied access, printing only case verdicts.
Regression suites for V2/CLI, legacy Agent, public reporter, Native
grammar, IPC race and rig test returned zero errors.

## Outstanding

F01 provisional GREEN; F02 partial; F03 HOLD; F04 partial; F05
provisional GREEN; F06 open; F07 partial. Missing model lifecycle,
ID lookup, full rig/geometry readback, frame/generation/revision
fences, undo/redo and V5 bone/skin mutation. G4 is NOT completed and
no live Native V2 service is claimed. SDK ABI20 unchanged.

Next: local owner model generation/revision readback, then safe local
IDs and bounded geometry, with exact RED-first reload/race oracles.
