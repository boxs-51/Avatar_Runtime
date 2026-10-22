# ARS-M2 — GPT Developer lifecycle and continuous self-checks

**ADOPTION_STATE=OWNER_ADOPTED_PENDING_MAIN** — Human Owner approved local governance v0.1 in chat; Draft PR not landed, no merge/M1/Wave authorization.

**Source:** https://github.com/boxs-51/assistant/issues/527 — M2 of Policy #85 v3.0. The single GPT_DEVELOPER is the technical implementer during explicit sessions, not a 24/7 unattended worker or a distinct GitHub identity.

## End-to-end developer ownership

GPT_DEVELOPER performs issue analysis, dependency/interface design, scoped implementation on branches, incremental review of its own work, test construction (including RED-first negative controls), build/CTest, raw CI examination on Linux and Windows, graphics/platform-specific reproductions when feasible, bug fixes, docs and source pinning. These are **SELF_CHECKS** and do not become an actor-distinct audit because a role name or second chat is used.

Developer should keep source ownership per file, stale-HEAD detection, parent/full SHA, compile options, exact commands and real runner evidence. Do not label unexecuted GPU tests, actual D3D11, CSPRNG source enforcement, fuzzing, production motion mixing or sanitizer regimes “PASS” merely because a design stub compiles.

## Stable handoff to final Auditor

Only once the candidate has a stable intended HEAD, the **FINAL_HANDOFF** packet includes: issue/stage, current main, PR/base/head, source manifest, agreed owner decisions and path custody, stacked ancestry, all actual Linux/Windows or device CI, release risk, rollback, open P0/P1, test limitations and observed negative controls. An Auditor under ARS-M3 then performs the comprehensive final pass. Developer fixes issues found, supplies a new exact source and reruns affected tests; material changes invalidate old final audit. Routine development has no mandatory intermediate auditor ACK.

All work remains subject to ARS-M1 T1/T2 permission, G00/M1 separation and ARS-M4/ARS-M5 integration controls. GPT cannot promise background or autonomous continued development without an explicitly scheduled workflow.
