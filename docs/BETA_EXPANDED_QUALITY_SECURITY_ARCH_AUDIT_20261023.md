# Beta — Expanded Quality, Security, Architecture & Performance Audit

> **Artifact:** BETA-QSA-20261023-v1 · **Date:** 2026-10-23 · **Status:** REVIEW DELIVERED / SUBJECT BLOCKED
>
> **Classification:** PUBLIC-SAFE AUDIT ONLY. This report intentionally excludes private model files, filenames, renderings, image bytes, private paths, model-derived checksums, geometry counts and measurements. Do not append raw test receipts or private evidence.

## 0. Contract, authority and reproducibility

- **Goal / C0:** Broad technical audit of Avatar Runtime `beta`: code quality, model/data confidentiality, Agent API/IPC, Native Studio, architecture, project structure, performance and maintainability; deliver report **inside this repository**.
- **Scope:** Registered Computer Bridge workspace `beta`, branch `exp/layer-physics-native-studio-local`, pinned inspected HEAD `e38eba5f4b1f21e8590520ef1aefb3a7cfa8407e`; files in the working tree were read with that baseline. Git refs/object history is local-only evidence.
- **Constraints:** Auditor-only, no source/oracle edits, no commit/push/merge/deploy, no deletion/pruning/rewriting of Git history, no private model material published. This audit report is the sole authorized new repository artifact.
- **Provenance:** Direct Windows physical Computer Bridge runs, local Git/pytest/CTest and read-only source review on 2026-10-23. Results are **SELF_REVIEW_ONLY**, not an independently authenticated third-party review.
- **Freshness:** Another development session may move HEAD/working tree. Check HEAD and working tree again before acting on individual findings.
- **Requirements:** REQ-01 secure storage/privacy; REQ-02 IPC and native input trust boundary; REQ-03 code correctness/test depth; REQ-04 architecture and project organization; REQ-05 performance opportunities/validation; REQ-06 evidence and actionable P0–P3 remediation; REQ-07 deliver privacy-safe Markdown into repo.
- **Report meaning:** A completed audit deliverable is not a product PASS. P0/P1/P2 below refer to the **reviewed subject**.

## 1. Executive verdict

**SUBJECT: BLOCKED / REVISE.** Tests are green for their tested paths, but there is one *physically reproduced* native-parser boundary defect and an IPC concurrency admission flaw. Local orphaned model/image Git objects and privacy-sensitive evidence files also require custody controls. **No publicly confirmed model exfiltration was demonstrated.**

| Domain | Verdict | Evidence |
|---|---|---|
| Python API contract tests | PASS, scoped | 17 tests + 57 subtests |
| Root CTest installed Release artifacts | PASS, scoped | 10/10 CTest; artifacts not all rebuilt at audit HEAD |
| Installed native test executables | PASS, scoped | Rig, Layer Analysis, XPBD parameter executables exit 0 |
| Native Studio build | PASS with warnings | RelWithDebInfo build on inspected source; C4996/C4701/C4244 among warnings |
| Native parser fail-closed | **FAIL / P1** | Current RelWithDebInfo physical negative E2E |
| Multi-agent file IPC | **FAIL / P2** | Deterministic two-caller race in isolated temp directory |
| Private model data custody | **UNVERIFIED publicly / P2 local risk** | Git fsck/orphan signature inspection; ignored evidence endpoints |
| Runtime physical rendering fidelity | UNVERIFIED | Receipt/API tests do not attest frame correctness |
| Complete cross-OS/long-run/crash tests | NOT_RUN | No full test matrix asserted |
| Host permission boundary | NOT AN OS SANDBOX | Host reports inherited Windows account, permission mode NOT_ENABLED |

## 2. Findings (reproducible, severity ordered)

### BETA-QSA-F01 — P1 — Native IPC parser accepts extra malformed tokens

- **Source:** `tools/avatar-studio/studio.c:669,735,836,850,858-870,880-881,905-906,920,927`. Several native handlers use `fscanf(f,"%d",&tail)!=1` to mean “no more input”.
- **Root cause:** When the next token is nonnumeric, `fscanf("%d",...)` returns 0, satisfying `!=1` while leaving trailing data unread. This is not an EOF check. `tools/avatar-studio/studio_agent_extended.inc:2-3` already uses a safer `" %c" == EOF` idiom for different handlers.
- **Current physical RED:** On freshly built RelWithDebInfo Studio, valid `RIG_SELECT 0` returned OK, while malformed `RIG_SELECT 0 invalid`, `RIG_WEIGHT 0 .25 invalid` and `RIG_PARENT 0 1 .8 1 invalid` incorrectly returned OK. The control `STUDIO_STATUS invalid` correctly returned ERR. All operations ran in a disposable local private session; no private payload was published.
- **Impact/preconditions:** Any local actor able to write native IPC requests can bypass Python validation and trigger state changes with invalid wire grammar; not evidence of remote unauthenticated access.
- **Remediation acceptance:** Consolidate exact-input/EOF validation for *every* native command before mutation. RED-first table: whitespace only, alpha suffix, extra integer, float NaN/Inf, negative/out-of-range indices, truncated/overlong request. Assert **ERR and no state mutation**. Rerun Windows physical E2E and synthetic controls without weakening expected failures.
- **Disposition:** OPEN; blocking trustworthy command interpretation.

### BETA-QSA-F02 — P2 — Single-file IPC lacks atomic admission / multi-agent lifecycle

- **Source:** `scripts/studio_agent_api.py:190-211`; `tools/avatar-studio/studio.c:661-665`; documented limitation `tools/avatar-studio/AGENT_API.md:62-65`.
- **Mechanism:** `request.exists()` check precedes `os.replace()` without an exclusive claim. A caller can overwrite another pending request; a caller's `finally` unlinks the shared request path. Response-token matching reduces false ACK, but does **not** provide mutual exclusion or exactly-once processing.
- **Reproduction:** Two caller threads were synchronized immediately before `os.replace` against a disposable temp queue; both passed admission check, both timed out without a native consumer, and shared request disappeared. This is a synthetic collision/admission proof, not a real-model two-agent correctness test.
- **Remediation acceptance:** Add exclusive writer admission/owner-scoped session request IDs, durable ACK lifecycle and bounded cleanup keyed to identity; test two callers, abrupt exit, stale reply, Windows sharing errors and process restart. Preserve legacy compatibility explicitly.

### BETA-QSA-F03 — P2 — Private binary assets persist in unreachable local Git objects

- **Source/evidence:** `git fsck --full --no-reflogs --unreachable --no-progress` plus *local, non-exported* blob magic classification. There are unreachable local blobs with model-format and PNG signatures, in addition to code/history objects. The inspected ref/reflog path inventory did not show tracked model/texture assets under the known private directories.
- **Interpretation:** These blobs are locally recoverable from Git's object database despite being absent from live tracked tree. **Unreachable is not proof of push, GitHub publication, unauthorized access or exploitation.** No external or remote-host object comparison was performed.
- **Remediation acceptance:** Treat `.git` backups/clones as sensitive; use an authorized offline object-origin and distribution audit before deleting, pruning or rewriting. Do not print object bytes/hashes, model-specific statistics or image pixels. Do not run `git gc`/`git prune` while evidence custody is unresolved.
- **Privacy policy:** `docs/in-progress/M2_MOC3_F4_RELEASE_LEDGER.md:50` prohibits public redistribution of models, images, derived hashes/counts/measurements.

### BETA-QSA-F04 — P2 — Model-derived evidence can escape through report/log promotion

- **Source:** `tools/avatar-studio/studio.c:638-650` writes local state and physics metrics; `tools/avatar-studio/studio_agent_extended.inc:131-141` saves viewport PNG; `scripts/test_studio_interaction_metrics.py:74-84` serializes detailed stages; `scripts/test_studio_agent_api_live.py:125-126` serializes receipts. `scripts/studio_agent_cli.py:16-20` prints native receipts.
- **Scope:** Paths are generally local/ignored and API accepts no arbitrary output paths; **no automatic public publication has been demonstrated**. However, an operator/script attaching raw evidence to CI, issues, PR comments or the audit report could break the explicit data policy. A public API contract for trusted local agents is not permission to publish model-derived content.
- **Remediation acceptance:** Enforce a public-safe reporter allowlist (case IDs, PASS/FAIL, broad error classes only), separate private evidence from distributable summaries, add negative tests for secrets/path/model-derived metadata, scrub historical docs before publication and audit artifact upload gates.
- **Hold:** Do not attach native logs, model screenshots, geometry counts, checksums or full JSON receipts to public issues/CI.

### BETA-QSA-F05 — P2 — Rig save overwrites the durable file directly

- **Source:** `tools/avatar-studio/studio_rig.c:144-169` opens target via `fopen(path,"w")`, writes variable-length records and closes; `tools/avatar-studio/studio_agent_extended.inc:171-181` invokes it.
- **Trigger:** Crash, write error or power loss during save can truncate the previous good rig. Return-value checks detect some failures but do not restore overwritten content.
- **Remediation acceptance:** Versioned temp file in same directory + flush/fsync-equivalent, atomic same-volume replacement and optional previous-good backup, with forced partial-write, crash and load-recovery oracles. Verify Windows rename semantics and do not assume cross-filesystem atomicity.
- **Confidence:** High for lack of atomic replacement; real power-failure corruption not physically simulated.

### BETA-QSA-F06 — P3 — Native Studio controller has high coupling and warning debt

- **Source:** `tools/avatar-studio/studio.c` is ~1040 lines covering GUI, input, render loop, API dispatch, state/evidence file I/O and physics configuration. RelWithDebInfo build reports C4701 (potentially uninitialized local `b`), C4244 conversion and legacy C4996 `fscanf` warnings.
- **Impact:** Wider changeset blast radius and harder focused fuzz/unit coverage; **compiler warnings are not proof of an actual exploit**.
- **Remediation acceptance:** Extract isolated exact-wire decoder/validator and session I/O from frame/UI module with parity tests; analyze C4701 with code path or sanitizer evidence before marking it as an actual uninitialized read. Track warning count/baseline in CI.
- **Scope:** Refactoring is an optional follow-on, not approved by this report.

### BETA-QSA-F07 — P3 — Broad E2E still has coverage/false-green gaps

- **Source:** `tests/moc3/test_studio_agent_api.py` exercises Python validation; `scripts/test_studio_agent_api_live.py:31-118` covers a selected native command set and negative strings; separate hierarchy/cross/brush scripts cover subset. They do not form exhaustive strict grammar, two-agent contention, crash/restart, or image-accurate visual acceptance.
- **Evidence:** Broad manual automation previously returned command ACKs, not image-fidelity or physical-model deformation proof. A historical tautological assertion from an older state was **not reproduced on inspected source**; do not re-report it as an active defect.
- **Remediation acceptance:** Generate native negative cases systematically for all exposed opcodes; assert effect/readback (not just OK), select safe synthetic rig fixtures, add crash/restart, memory-bounds and Windows+Linux gates as relevant.

## 3. Architecture assessment (source-grounded)

**Observed components and ownership:**

1. `scripts/studio_agent_api.py`: Python schema validation and file-backed caller/receipt adapter (local trust).
2. `tools/avatar-studio/studio.c`: interactive controller, native request dispatcher, UI/frame loop, transient state/evidence.
3. `tools/avatar-studio/studio_agent_extended.inc`: legacy/extended command dispatch and local capture/rig operations.
4. `tools/avatar-studio/studio_rig.[ch]`: per-drawable rig, anchor, beam, parent, weight, cross-drawable links and versioned file format.
5. `tools/avatar-studio/studio_layer_analysis.[ch]`: topology/component analytical operations.
6. `tools/avatar-studio/studio_xpbd.cpp` and `native_mesh_xpbd.hpp`: physics adapter/solver, parameter boundaries and constraints.
7. `src/`, `include/`, `contracts/`, `schemas/`: broader runtime/validation contracts, **not all independently re-reviewed at method level**.

**Trust model:** Trusted local caller -> Python validator (optional) -> shared request file -> native dispatcher -> sensitive in-memory model and ignored evidence files. Native wire parser is the enforcement boundary because a caller can write IPC without using the Python wrapper. There is no independently verified OS sandbox, per-agent OS ACL or network authentication in Studio's file queue; those missing features are explicitly acknowledged in its API documentation.

**Architecture verdict: REVISE.** Module decomposition for rig/layer/solver is useful and limits some coupling, but the native dispatcher must be hardened and queue ownership explicit before claims of robust multi-agent control.

## 4. Project structure and code-quality assessment

- **Scanned inventory:** 562 tracked paths; approximately 352 tracked C/C++/Python/headers, 15 workflow paths and broad native/Python tests. Counts describe repository source files, **not private model geometry**.
- **Strengths:** Dedicated native unit executables for rig, layer topology and XPBD; Python typed command whitelist; local-only capture destinations; bounds and finite-scalar checks in multiple APIs; `.gitignore` excludes `/build/`, `/local-assets/` and legacy screenshots; file format compatibility paths in `studio_rig_load`.
- **Quality debt:** Inconsistent native parse idioms, one shared IPC channel, monolithic Studio orchestration, direct durable rig truncation, warning debt and E2E ACK≠visual correctness.
- **Review limits:** Repository-wide line-by-line memory-safety analysis, dependency vulnerability scan, static analyzer/sanitizer run, GUI pixel oracle, load/fuzz testing and full PR/remote Git history comparison were **NOT_RUN**. No universal “clean” certification implied.
- **Project verdict: REVISE / PARTIAL COVERAGE.** Separation of source/test/doc directories is clear; controller and data-evidence ownership need sharper boundaries.

## 5. Performance/optimization (advisory; **no measured speedup claims**)

| ID | Grounded hotspot | Incremental option | Alternative / cost | Validation before approval |
|---|---|---|---|---|
| OPT-01 | `native_mesh_xpbd.hpp:180-190` constructs `std::vector<double> lambdas` inside each solver substep. | Reuse capacity/scratch with correct zero reset per substep. | Retain allocation until profiling proves cost; lifetime/state correctness complexity. | Same synthetic workload baseline vs candidate, alloc count/frame and CPU time, numerical parity, rollback test. |
| OPT-02 | `native_mesh_xpbd.hpp:194-200` iterates dynamic nodes × faces for surface exclusion, with an explicit work budget. | Broad-phase spatial bucketing/AABB coarse pruning before narrow-phase. | Extra memory/invalidation complexity; leave unchanged if workloads small. | Worst-case synthetic mesh timing, collision parity, budget-exhaustion and non-finite input gates. |
| OPT-03 | `tools/avatar-studio/studio.c:638-650,1006` periodically writes state JSON and metrics to disk. | Separate in-memory snapshot from throttled private persistence; batch/coalesce writes. | Crash-resilience/telemetry timeliness trade-off; no change until measured. | Frame pacing, disk ops/sec, data-freshness and crash/restart proof. |
| OPT-04 | `studio.c` mixes frame loop and request grammar. | Table-driven native decoding and focused fuzzable dispatcher. | Refactor regression risk; do this with strict parser RED baseline first. | Command-parity matrix and Windows physical fail-closed tests. |

All optimization proposals **NOT_GRANTED**; no code optimization or performance benchmark was performed as part of this review.

## 6. Test/evidence ledger

| Proof | Status | Tier and caveat |
|---|---|---|
| `pytest -q tests/moc3/test_studio_agent_api.py tests/moc3/test_studio_model_diagnostics.py tests/moc3/test_studio_xpbd_contract.py tests/moc3/test_layer_physics_native_entrypoint.py --tb=short` | PASS: 17 + 57 subtests | Windows local Python, current source |
| `ctest --test-dir build -C Release --output-on-failure --timeout 25` | PASS: 10/10 | Installed Release test binaries; not individually rebuilt to current HEAD in this audit |
| `studio_rig_tests.exe`, `studio_layer_analysis_tests.exe`, `studio_xpbd_parameter_tests.exe` | PASS: 3/3 | Existing Release native binaries; version coupling should be verified before release |
| `cmake --build build/avatar-studio --config RelWithDebInfo --target avatar_studio --parallel 2` | PASS with warnings | Rebuilt Native Studio from inspected working tree, Windows |
| Native parser E2E, private temp session, one clean control + malformed suffixes | **FAIL**: malformed RIG_SELECT/WEIGHT/PARENT accepted | Actual Windows Native Studio; ERR control for STUDIO_STATUS passes |
| Two-caller IPC `os.replace` barrier in disposable temp dir | **FAIL admission property** | Python concurrency reproducer; no native consumer, so no exactly-once claim |
| Local `git fsck` and unreferenced object magic classification | RISK CONFIRMED, public status **UNVERIFIED** | Only local object database; no blob contents exposed |
| Full cross-platform, fuzz, sanitizers, perf benchmark, private model public-distribution audit | NOT_RUN | Essential before stronger claims |

## 7. Remediation sequence / exit criteria

1. **P1 first:** Strict Native IPC grammar across *all* opcodes, no mutation on invalid syntax, native adversarial suite RED→GREEN.
2. **P2 queue:** Exclusive multi-agent admission and response ownership, restart/timeout/race oracles.
3. **P2 private evidence:** Local-vs-public classification, default public-safe redaction, artifact leak tests; independently determine Git orphan origin and whether any push/export occurred. Preserve evidence until authorized cleanup.
4. **P2 durability:** Atomic/backup rig write and fault-injection recovery tests.
5. **P3 correctness:** Add command effect/readback, native/GUI integrity and multi-OS CI; track compiler warnings.
6. **Optional optimization:** Benchmark only after semantic gates, choose measured/cost-justified changes.

**Release/blocker gate:** Maintain HOLD for Native Studio agent write-path integrity and model privacy clearance until relevant gates are demonstrated. No finding authorizes a source edit, history rewrite, public upload or merge.

## 8. Delivery gate

- **Report task:** DELIVERED once exact path is confirmed in `beta` and its bytes verified.
- **Audited product:** **BLOCKED** by F01, plus unresolved P2 privacy/IPC/durability.
- **Independence:** SELF_REVIEW_ONLY; an independent review is still optional as a separate process, not impersonated here.
- **No publication:** This report contains only code locations, test outcomes and privacy-safe classification. Avoid adding screenshots/model-derived receipts later.

---
Generated by ChatGPT acting as reviewer-only auditor for the owner-enrolled `beta` workspace.