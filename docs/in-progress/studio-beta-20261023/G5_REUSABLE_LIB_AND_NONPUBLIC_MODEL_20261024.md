# G5 Reusable Internal Library and Non-public Model Information — 2026-10-24

**Scope:** `boxs-51/Avatar_Runtime`; branch `exp/g5-local-cas-transactions`; Windows Computer Bridge `beta`.
**Requirement clarification (Owner):** protect **model information from publication** on GitHub, in SDK packages, logs and reports; **not** a new permission restriction preventing legitimate local UI/Agent model use.
**Authority:** new publication policy, scanner and internal library refactor only; no G5 writer activation, merge, deploy, public ABI change or model export.

## Implemented

1. Canonical [ADR-005](../../reference/ADR_005_MODEL_INFORMATION_NONPUBLIC_AND_SDK_PACKAGING.md) defining non-public model sources, derived geometry/identifiers, textures/screenshots, paths, raw logs, and installation artifacts; limited public-safe evidence; manual exception review and historical Git disclosure caveat. Explicitly preserves local model read/edit functions.
2. Added the policy to `README.md`, `docs/DOCUMENTATION_INDEX.md`, `docs/reference/SDK_DEVELOPMENT_PACKAGE.md` and G5 ADR-004. A public API/CPU profile does **not** imply permission to publish private models.
3. Added `scripts/check_model_publication.py` with `--staged` (complete Git index), `--ref HEAD` (revision tree) and `--package <sdk-directory>` (actual SDK staging directory) modes. Checks known model/media formats, private directories, file signature of renamed obvious binary models/images, JSON manifest/geometry, symlinks in package and oversized/unknown binaries. Denial output shows **category labels only**: no asset name, path, payload or per-model file count.
4. Expanded `.gitignore` to cover private 3D model formats. Ignore patterns are not a substitute for pre-publish checks or historical Git audits.
5. Refactored experimental G5 Native core to **independent internal C11 static library** `studio_g5_txn_core` (transaction core + deterministic rig), linked by the standalone synthetic Native G5 tests and opt-in process fixture. CMake does not install/export the library as public SDK and does not link it into production `avatar_studio`. Public ABI20 unchanged.

## Evidence at working-tree revision

| Check | Observed result | Proof tier |
|---|---|---|
| Git tree audit `--ref HEAD` | PASS, 658 Git blobs | Known signature/file/JSON guard only; **not** all history or prose |
| Git index audit `--staged` before this change | PASS, 658 Git blobs | Original tracked index; rerun after staging |
| Synthetic privacy / SDK guard | 13 PASS / 1 SKIP (symlink privilege unavailable on Windows) | Unit and temporary package fixtures, no real models |
| `pytest tests/moc3 tests/test_model_publication_guard.py` | **224 PASS, 19 SKIP, 204 subtests PASS** | Windows Python |
| G5 opt-in Native CTest | **9/9 PASS** after static library refactor | Windows MSVC Release; synthetic model-free core and two-process IPC fixture |
| Top-level Windows CTest / ABI20 | **10/10 PASS** | Windows, includes C11/C++20 ABI20 |
| `avatar_studio` Release build | PASS | Existing runtime unchanged, G5 writer unlinked |
| Public SDK installation manifest audit | **NOT_RUN** | No actual installed SDK was built for this task |
| All Git history / releases / forks | **NOT_AUDITED** | Current tree scan cannot prove absence of older leaks |
| G5 UI/Agent live writer and accepted-frame proof | **HOLD / NOT_IMPLEMENTED** | Independent gates remain open |

## Important non-goals and next checks

- The tool does not inspect published Git history, remote forks, arbitrary private data embedded in a compiled library, or all natural-language evidence/logs. Those need independent audit and artifact review.
- Do not bundle model fixtures with a reusable library unless separately licensed and deliberately approved for public redistribution.
- The production SDK profile remains separate from the internal G5 static library. Promotions require stable API, consumer and packaging tests, license provenance and confidentiality clearance; packaging a library alone does not lift G5 writer HOLD.
- Future GitHub/SDK release flows must run the scanner, manually inspect public log/PR/CI evidence and scan the **actual installed package** before publication.

**Decision:** publication confidentiality requirement recorded and partial automatic gates implemented. No new authorization/deny-access system for local Studio/Agent model operations.
