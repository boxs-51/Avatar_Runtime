# Native Studio Beta — Finding remediation checkpoint

- **ID:** STUDIO-BETA-REMED-20261023-01
- **Date:** 2026-10-23
- **Branch:** `exp/layer-physics-native-studio-local`
- **Start:** documentation baseline `05df4d374414a6062e69f766eb0b42f25be885b8`
- **Role:** Local engineering self-check; not auditor sign-off
- **Status:** Scoped regressions GREEN; product **REVISE / release HOLD**

## Finding disposition

| Finding | Change | Test evidence | Remaining gate |
|---|---|---|---|
| F01 / P1 Native parser | Exact native EOF in 14 numeric-tail handlers and RIG_APPLY; bounded request and token | Initial 8 physical RED; after changes 48 native malformed grammar cases GREEN, 44 valid API commands PASS | Full opcode fuzz/mutation and independent review: PROVISIONAL GREEN |
| F02 / P2 IPC race | Python adapter uses Windows OS writer lock and owner-correlated cleanup | Two-process race PASS; killed holder releases OS lock; stale request rejected without false ACK | Native direct IPC writers and exactly-once restart lifecycle: PARTIAL |
| F03 / P2 Git private orphans | Preserve Git objects, no gc/prune/rewrite | Auditor found local-only orphan signature risk, no remote publication proven | Authorized offline origin/distribution review: HOLD |
| F04 / P2 report privacy | Classification-only public reporter rejects arbitrary fields and model-derived measurements | Safe and adversarial reporter unit tests PASS | Mandatory artifact promotion gate and historical report review: PARTIAL |
| F05 / P2 rig durability | Sibling exclusive temp file, fflush, OS commit, Windows replacement with write-through | RED injected failure on old source; GREEN on new save path, previous good rig intact | Crash/power-loss and sharing fault physical matrix: PROVISIONAL GREEN |
| F06 / P3 controller coupling | No broad refactor performed | Native Studio build PASS | Extract native parser and warning-baseline review: OPEN |
| F07 / P3 coverage | Expanded grammar/behavior/multi-agent and negative tests | Full local regression PASS | Fuzz, visual fidelity, cross-OS, long-run: PARTIAL |

## Evidence receipt and privacy

Fresh Windows Native Studio RelWithDebInfo build PASS; Release rig, layer and XPBD test targets PASS. Python Agent API and public reporter suites PASS. E2E grammar, Agent API, rig hierarchy, cross-drawable brush, model topology, XPBD and interactive recovery PASS. Two-process IPC race and abrupt writer termination probes PASS.

Raw traces/logs remain exclusively under the Git-ignored `build/studio-user/`. No private model identifiers, textures, screenshot bytes, model-derived receipts, hashes or geometry details are in this report. The two auditor files and their separate IPC were not altered.

## Authorization and rollback

No public SDK ABI20 change, no push/merge/deployment, no GitHub write and no object pruning. This is a development self-check, not an independent closing review. Revert only the bounded local remediation commits if regression emerges, preserve private evidence and re-run exact HEAD tests before audit closure.
