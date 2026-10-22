# Alpha #60 completion progress

Date: 2026-10-21 Asia/Bangkok. Base: alpha `5d90997e3f1e6df0ed5bef2b77318f815aa17769`. Branch: `dev/alpha-60-cpu-lifecycle-complete`. F1-F5 local implementation and installed-source attribution have passed; committed delivery/integration are in progress; #60 remains OPEN until reviewed committed integration. [Design and immutable requirements](ALPHA_60_PUBLIC_CPU_COMPLETION_DESIGN.md).

| Requirement | Implemented and observed locally | Final delivery work |
| --- | --- | --- |
| F1 complete native2D/GLB/dual | Separate full profile, bounded typed metadata/raw accessors/RGBA, parser-free host semantic validation, original hierarchy/default mode/mappings. Full151 cases and original2D150 cases PASS, independent F1 source review PASS. | Full151/probe112/old150 and final four-library parser-free byte attribution PASS; see final acceptance. |
| F2 bounded ownership | Runtime-owned256MiB context,64model records,16characters, generation-bound model/snapshot pins, outside-lock retirement and allocator reentry guard. Corrected bind rollback, last-batch-pin128 races and all19 core tests PASS. Public140 load-allocation failures, character/publication OOM, lifetime/admin96 and budget/reuse checks PASS. | Preserve exact final-source evidence through commit/integration. |
| F3 authority/publication | Compact all-character prepare/fence/commit transaction with S/time/cache/queue atomicity. Public16-character publication, second-character failure rollback, wrong-S, typed/cache/revoke/quaternion/FIFO16/no-replay and mode/layer tests PASS. Shared host-clock exclusion/highwater correction passed public regression and independent review. | Controlled preadmitted ingress test PASS; unchanged enabled lifecycle/finalizer regressions corrected and all23 CPU core/public/custody cases PASS50.99s. Final four-config installed SDK includes these corrections and passes. |
| F4 helper custody | Fixed module-adjacent helpers, owner-only temporary worker, explicit bounded pipe, outerJob plus original inner caps. Actual missing/hang/crash/flood/malformed/readonly/hostile-path cases7 PASS. Owned quarantine and orphan recovery retain runtime work pin and block finish until real OS cleanup; local PASS. | Final fault7 and bounded-custody/API11 source/binary attribution PASS; independent review PASS. |
| F5 SDK/delivery | Installed+physically relocated C11/C++20 static/shared Release/Debug CPU matrix passed24 executions; unchanged feature-off four-config matrix and exact20exports passed. | Final CPU and feature-off matrices both PASS after the runtime corrections. All178 contracts PASS146.929s; final committed custody, source attribution, commit/push and alpha MERGE_COMMIT remain required. |

## Evidence provenance

All raw receipts remain ignored under build/ and retain their original hashes. Earlier receipts are not rewritten to represent newer sources. [Final acceptance](../completed/audits/ALPHA_60_PUBLIC_CPU_ACCEPTANCE.md) records exact qualified paths, hashes, commands and coverage limits.

- Full151: `build/alpha60-isolated-native/full-model-evidence/b50913520b2a44949f68e09fbdb68317/evidence.json`.
- Original150: `build/alpha60-isolated-native/cpu-model-evidence/f5d34fd5a3b1485d9fc62e91e55c88c0/evidence.json`.
- Enabled CPU23/23: `build/alpha60-enabled-core-qualified.log`,50.99s; includes unchanged finalizer/private-lifecycle tests and public API/custody.
- Core19/19: `build/alpha60-core-final-local.log`; corrected bind rollback and128 final-pin races included.
- Public API: `build/alpha60-public-final.log`; `build/alpha60-public-typed.log`.
- Fault7: `build/alpha60-helper-faults/98bd648164d94d3d84a43a3327086611/evidence.json`.
- Actual custody: `build/alpha60-custody-owned.log`, `build/alpha60-custody-orphan.log`.
- Earlier CPU SDK four-config receipt: `build/alpha60-installed-cpu/0e241324ef4147959ce8dcb2482aad21/evidence.json`, predates shared-clock fix and is not exact-final proof.
- Earlier unchanged feature-off four-config receipt: `build/alpha60-featureoff-installed-resume/evidence.json`.

## Review findings and scope

Resolved findings include typed GLB aggregate image/layer/ID/object-key parity, failed bind owner retention, sequential partial multi-character publication, caller-owned admin retirement, empty catalogs, orphan custody, owned read-only copy, last-pin guard lifetime and shared clock exclusion/highwater, ordinary finalizer race and legacy private allocator rollback. No scope was reduced to make a gate pass. Latest runtime review closed the shared-clock finding and found no further supported runtime P1/P2 in that delta; final whole-issue delivery remains unverified pending committed custody and integration.

The earlier usage-limit rejection is resolved: elevated local builds, OS trials and matrix executions have resumed. This is no longer an execution HOLD.

No main/public activation, private asset redistribution, Cubism SDK download or physical GPU reset. D06/D08 physical HOLD and production rendering/security/release issues remain separate. Local runtime evidence is primary; CI is corroboration.
