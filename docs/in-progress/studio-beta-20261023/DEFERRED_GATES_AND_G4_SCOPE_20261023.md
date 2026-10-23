# Studio Beta — DEFERRED/UNVERIFIED gate ledger

Date: 2026-10-23
Workspace: Beta (local Windows), branch: exp/layer-physics-native-studio-local
Starting HEAD: fcb6eb907e24245ff1a454cbde2d01bd018786a4
Task mode: continue independent G4 Local-only API while preserving holds.
This ledger is engineering planning, not auditor permission or a release declaration.

| Case | Disposition | Why not currently verified | Resumption gate |
|---|---|---|---|
| F02-NATIVE-CRASH | DEFERRED / UNVERIFIED | Killing Native Studio during a real private-model mutation lacks a disposable, isolated owner and clean recovery procedure; synthetic child-kill evidence is not equivalent | Isolated disposable Native process + nonprivate synthetic fixture + exact commit/ACK fault injection + readback oracle |
| F02-ACK-RECONCILE | DEFERRED / UNVERIFIED | No durable owner-side mutation journal or manual reconciliation contract; client uncertain_outcome only stops cooperative replay | Typed revision and idempotent readback contract, before permitting recovery |
| F02-SAME-USER | DEFERRED / UNVERIFIED | Host reports USER_CONTEXT_NO_OS_SANDBOX; same-user hostile IPC is not isolated | OS-enforced boundary proposal and physical negative matrix |
| F05-POWER-LOSS | DEFERRED / UNVERIFIED | Crash/power-loss injection risks real workstation and private assets; no disposable disk image | Disposable Windows VM/filesystem fixture, pre/post persistence oracle |
| F03-PRIVATE-OBJECTS | HOLD (unchanged) | Local Git object/history exposure audit not independently closed | Explicit custody audit; never prune/GC/force-push |
| F04-PUBLIC-EXPORT | PARTIAL (unchanged) | Not all logging/report/export paths enforce private-data classification | Enumerated sinks and negative export-boundary suite |
| F06-CONTROLLER | OPEN (independent work) | Native dispatcher architecture still coupled | Design/diff/tests, no unsupported release assertion |
| F07-INDEPENDENT | DEFERRED / UNVERIFIED | No separate auditor and some stress/fault matrices unfinished | Independent review and environment-specific evidence |

Deferred does **not** mean PASS, waived, or permanently abandoned. Maintain baseline
RED and all existing negative/security oracles. Do not repeat private-model
mutation or OS-destructive fault injection while disposable proof is unavailable.

## Selected independent task

G4 Local-only UTF-8 ID readback: safely support bounded arbitrary UTF-8
parameter/part IDs without raw whitespace-sensitive Native IPC fields.
Read-only; preserve generation checks, fail-closed ambiguous IDs,
explicit local-private authorization, SDK ABI20 and no external export.
RED-first synthetic fixtures plus real Windows Native Release regression.
