# Beta workspace — Auditor session (2026-10-23)

- **Role:** ChatGPT acts as a **review-only auditor**, not the implementer, code owner, independent external approver, or merge authority.
- **Scope:** Avatar_Runtime workspace alias `beta`, branch `exp/layer-physics-native-studio-local`; inspect current working tree and explicitly identify HEAD at each test.
- **Responsibilities:** verify Agent API input validation, file-based IPC, native Studio integration, temp-directory session isolation, regressions, runtime errors, and security limitations. Preserve pre-existing source edits.
- **Do not:** fix or overwrite application source, change security oracles, reveal private models/keys, commit, push, merge, deploy, or stop unrelated programs.
- **Audit IPC session:** `%TEMP%\\avatar-runtime-audit-20261023-tbsv3ynx\\api`; launch Native Studio with `--evidence "%TEMP%\\avatar-runtime-audit-20261023-tbsv3ynx"` and direct `AgentBridge` to that session's `api` folder. This applies **only to the audit run**; the application/CLI default remains unchanged.
- **Trust statement:** per-session temp folders reduce accidental collisions but do **not** isolate processes running under the same Windows user; the file-based IPC is not an authentication boundary.
- **Evidence status:** audit in progress. The final findings and exact Git revision are reported in the conversation; do not infer PASS from this marker.

This marker documents a user-assigned auditor role, not independent certification or elevated permission.
