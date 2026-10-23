# SYS-001 — Native Studio system architecture (observed)

- **Version:** 0.1, 2026-10-23
- **Baseline:** local branch `exp/layer-physics-native-studio-local` at `e38eba5f4b1f21e8590520ef1aefb3a7cfa8407e`
- **Role/approval:** Development architecture review; DRAFT, Owner approval pending
- **Sources:** `studio.c`, `studio_agent_extended.inc`, `studio_rig.[ch]`, `studio_layer_analysis.[ch]`, `studio_xpbd.cpp`, `native_mesh_xpbd.hpp`, `scripts/studio_agent_api.py` and the Beta QSA audit
- **Scope:** Experimental Studio, not public SDK ABI20 or production readiness

## Observed components and data ownership

| Component | Ownership |
|---|---|
| `scripts/studio_agent_api.py` | Python command validation, request token, shared file request/response |
| `tools/avatar-studio/studio.c` | Native UI, frame loop, model, input, dispatch, private local evidence |
| `studio_agent_extended.inc` | Additional native commands, capture and local operations |
| `studio_rig.[ch]` | V4 rig: anchors, beam/parent links, sparse vertex weights, explicit cross-drawable constraints |
| `studio_layer_analysis.[ch]` | Topology/component relations and uncertain geometry classification |
| `studio_xpbd.cpp` | Conversion from rig/scene state to native physics constraints |
| `native_mesh_xpbd.hpp` | Bounded XPBD solver and accepted state |
| `build/studio-user/` | Git-ignored confidential local logs, captures and test evidence |

```text
Trusted local agent -> optional Python validator -> shared request file
  -> Native Studio dispatcher -> rig/layer/scene state
  -> XPBD adapter/solver -> renderer
                    |
                    +-> private local artifacts (ignored)
```

**Observed, not proposed:** hierarchy is implemented by XPBD parent-distance constraints; not by world/local bone matrices or inverse-bind skinning. Cross-drawable links are explicit and a two-drawable solver limit remains. Semantic classification is not verified anatomical identification.

## Trust and persistence boundary

The Native wire decoder is the final validation boundary because local writers can bypass Python. Audit F01 physically reproduced malformed suffix acceptance; readback still identifies vulnerable numeric-tail checks in `studio.c`. Audit F02 found a non-exclusive admission check followed by replacement of the shared IPC request. The Host has inherited Windows account privileges and does not supply a separate OS sandbox. Audit F05 identified direct overwrite via `fopen(path,"w")` for rig saves.

## Required invariants

- **SYS-SEC-01:** Exact native grammar, complete EOF consumption, finite/ranged values and **zero mutation on invalid input**.
- **SYS-SEC-02:** Writer ownership, exclusive admission, bounded ACK and owner-scoped cleanup across races/restarts.
- **SYS-SEC-03:** No private model material, images, derived numeric details, object hashes, full receipts or paths promoted to public artifacts.
- **SYS-SEC-04:** Crash-consistent same-directory atomic rig replacement; preserve last-good data on failure.
- **SYS-SEC-05:** Generation binding of rig/proposals/solver to the loaded model; reject stale references.
- **SYS-SEC-06:** Experimental feature gates remain separated from SDK ABI20 and production release.

**Verdict:** SOURCE BASELINE RECORDED; PRODUCT BLOCKED / REVISE by Beta audit. No source implementation, independent approval or performance benchmark is implied by this document.