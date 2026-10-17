#!/usr/bin/env python3
"""Fail-closed M0 RC1 provenance and doc consistency check for CI.

Not a GitHub API check of live remote PR heads. A PR synthetic merge validates
its EXACT base checkout SHA. If the remote parent moves, the new PR check must
be re-run and pin file updated after source review.
"""
from __future__ import annotations

import json
import ast
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
PINS = ROOT / "docs" / "M0_CONTRACT_PARENT_PINS.json"

# Reviewed keysets are code-owned, not loaded from the mutable pin JSON.
# A missing field must NEVER silently shrink the claimed evidence surface.
REQUIRED_PREDECESSOR = frozenset((
    "schemas/character_manifest_v0.1.schema.json",
    "schemas/ars2d_rig_v0.1.schema.json",
    "examples/character_dual_mode.json",
    "examples/character2d.ars2d.json",
    "contracts/ars_p0_contract_draft.h",
    "docs/P0_OWNER_REMEDIATION_V0.5.md",
    "docs/ADR_P0_SECURITY_R2.md",
    "docs/ACCEPTANCE_GATES_V0.1.md",
    "docs/SPECIFICATION_V0.1.md",
))
REQUIRED_G00 = frozenset((
    "include/ars/ars.h",
    "docs/ADR_0001_D3D11_G00_DESIGN_RC1.md",
    "docs/P0_SECURITY_REGRESSION_GATES_V0.1.md",
    "docs/V0_1_CANONICAL_CONTRACT_RC1.md",
    "docs/V0_1_PUBLIC_ABI_FREEZE_MAP.md",
    "docs/V0_1_FORMAT_VALIDATION_FREEZE_MAP.md",
    "tests/contracts/p0_r5_state_machine_model.cpp",
    "tests/abi/design_smoke.c",
))

def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, stderr=subprocess.STDOUT).strip()

def require(condition, message):
    if not condition:
        raise AssertionError(message)

CONSOLIDATION_BRANCH = "integration/m0-consolidated-20261015"
CONSOLIDATION_MAIN = "637bdae7373aecbf50b18834f3118421d921fe3a"
CONSOLIDATION_FROZEN = "fe3e680a44ac4e71ef57dc177e73ae31198fcbd0"
CONSOLIDATION_PR15 = "7cf306dd783edd7e155ac26517c4b99b8b449552"
CONSOLIDATION_GOVERNANCE = "19069dc85a0e07fe58b9ce8aad6cbfd5f0b1b55a"
CONSOLIDATION_MAINTENANCE = "3d72b9c2cfaa77c3c0495143780f9a947b4c39c8"
CONSOLIDATION_GOVERNANCE_PATHS = frozenset((
    ".github/workflows/ars-governance-policy.yml",
    "docs/GOVERNANCE_ARS_SOLO_OWNER_001.md",
    "docs/governance/ARS_M1_SOLO_OWNER_ACK.md",
    "docs/governance/ARS_M2_DEVELOPER.md",
    "docs/governance/ARS_M3_FINAL_AUDIT.md",
    "docs/governance/ARS_M4_STACKED_PRS.md",
    "docs/governance/ARS_M5_MERGE_HISTORY.md",
    "docs/governance/ARS_M6_WAVE_STATE.md",
    "docs/governance/ARS_POLICY_V0_1_INDEX.md",
    "tests/governance/test_ars_policy_reference.py",
))
CONSOLIDATION_MAINTENANCE_PATHS = frozenset((
    ".gitignore", "docs/DEVELOPMENT.md", "docs/audits/HANDOFF_20261015.md",
))
CONSOLIDATION_DELTA = CONSOLIDATION_GOVERNANCE_PATHS | CONSOLIDATION_MAINTENANCE_PATHS | frozenset((
    ".github/workflows/m0-freeze-prep.yml",
    "docs/M0_CONTRACT_PARENT_PINS.json",
    "tests/contracts/check_m0_rc1_pins.py",
    "tests/contracts/test_m0_consolidation.py",
    "docs/M1_A_SCOPE_V0.1.md",
    "docs/audits/M0_INTEGRATION_20261015.md",
))

def check_consolidation(base, candidate):
    """One exact main-target composition; no wildcard branch or runtime edits.

    This source/ancestry check does not authorize a merge or change the owner
    decision. Existing PR15's two-file allowlist remains unchanged below.
    """
    require(base == CONSOLIDATION_MAIN,
            "Consolidation main moved: fresh exact-target review required")
    for ancestor in (CONSOLIDATION_FROZEN, CONSOLIDATION_PR15,
                     CONSOLIDATION_GOVERNANCE, CONSOLIDATION_MAINTENANCE):
        git("merge-base", "--is-ancestor", ancestor, candidate)
    changed = set(git("diff", "--name-only", CONSOLIDATION_FROZEN, candidate).splitlines())
    require(changed == CONSOLIDATION_DELTA,
            f"Consolidation path drift: {sorted(changed ^ CONSOLIDATION_DELTA)}")
    for source, paths in ((CONSOLIDATION_GOVERNANCE, CONSOLIDATION_GOVERNANCE_PATHS),
                          (CONSOLIDATION_MAINTENANCE, CONSOLIDATION_MAINTENANCE_PATHS)):
        for path in sorted(paths):
            require(git("rev-parse", candidate + ":" + path) ==
                    git("rev-parse", source + ":" + path),
                    f"Consolidation altered preserved source: {path}")
    return True

M1_A_BRANCH = "m1/a-build-foundation"
M1_A_BASE = "a307c6775388a507aeacb5fbeb09479633c20f58"
M1_A_PATHS = frozenset((
    "CMakeLists.txt", "src/abi/abi_version.cpp", "src/abi/unsupported.cpp",
    "cmake/ars_compiler_options.cmake", "cmake/ars_verify_exports.cmake",
    "tests/abi/foundation_contract.c", "tests/abi/foundation_cpp.cpp",
    "examples/plain_c/CMakeLists.txt", "examples/plain_c/main.c",
    ".github/workflows/m1-build.yml", "docs/audits/M1_A_BUILD_EVIDENCE.md",
    "tests/contracts/check_m0_rc1_pins.py", "tests/contracts/test_m1_a_custody.py",
    "tests/contracts/test_m0_consolidation.py",
))

def check_m1_a(base, candidate):
    """Approved PR18 scope: exact target, 14 paths, M0 ancestry and design blobs."""
    require(base == M1_A_BASE, "M1-A main moved: fresh exact-target review required")
    git("merge-base", "--is-ancestor", M1_A_BASE, candidate)
    changed = set(git("diff", "--name-only", M1_A_BASE, candidate).splitlines())
    require(changed == M1_A_PATHS, f"M1-A source path drift: {sorted(changed ^ M1_A_PATHS)}")
    # Read the historical manifest, never accept new current-branch SHA values.
    frozen = json.loads(git("show", M1_A_BASE + ":docs/M0_CONTRACT_PARENT_PINS.json"))
    validate_blob_manifest(frozen, candidate)
    return True

M1_B1_BRANCH = "m1/b1-runtime-lifecycle"
M1_B1_BASE = "ab21908310d48466b27bb06c92ea1623353c79f5"
M1_B1_PATHS = frozenset((
    "CMakeLists.txt", "src/abi/unsupported.cpp", "src/abi/runtime_api.cpp",
    "src/abi/api_validation.hpp", "src/core/handle_registry.hpp",
    "src/core/handle_registry.cpp", "src/core/runtime_lifecycle.hpp",
    "src/core/runtime_lifecycle.cpp", "src/core/runtime_budget.hpp",
    "tests/security/handle_registry_tests.cpp", "tests/security/runtime_lifecycle_tests.cpp",
    "tests/security/runtime_test_support.hpp", "tests/abi/runtime_core_contract.c",
    "tests/abi/runtime_core_cpp.cpp", "examples/plain_c/CMakeLists.txt",
    "examples/plain_c/main.c", "cmake/ars_compiler_options.cmake",
    ".github/workflows/m1-build.yml", ".github/workflows/m1-b-security.yml",
    "tests/contracts/check_m0_rc1_pins.py", "tests/contracts/test_m1_a_custody.py",
    "tests/contracts/test_m1_b_custody.py", "docs/audits/M1_B1_BUILD_EVIDENCE.md",
    "docs/audits/M1_B_CASE_EXECUTION.json",
))

def check_m1_b1(base, candidate):
    """Owner-approved B1 only; component tests cannot clear whole security cases."""
    require(base == M1_B1_BASE, "B1 main moved: fresh exact-target review required")
    git("merge-base", "--is-ancestor", M1_B1_BASE, candidate)
    changed = set(git("diff", "--name-only", M1_B1_BASE, candidate).splitlines())
    require(changed == M1_B1_PATHS, f"B1 source path drift: {sorted(changed ^ M1_B1_PATHS)}")
    frozen = json.loads(git("show", M1_B1_BASE + ":docs/M0_CONTRACT_PARENT_PINS.json"))
    validate_blob_manifest(frozen, candidate)
    require(git("rev-parse", candidate + ":include/ars/ars.h") ==
            git("rev-parse", M1_B1_BASE + ":include/ars/ars.h"), "B1 changed frozen public ABI")
    inventory = json.loads(git("show", M1_B1_BASE + ":docs/G00_SECURITY_87_CASE_INVENTORY.json"))
    ledger = json.loads(git("show", candidate + ":docs/audits/M1_B_CASE_EXECUTION.json"))
    require(ledger["scope"] == "M1-B1_CPU_COMPONENTS_ONLY", "B1 ledger scope drift")
    cases = ledger["cases"]
    require(len(cases) == 87 and len({c["id"] for c in cases}) == 87, "B1 case inventory drift")
    originals = {c["id"]: c for c in inventory["cases"]}
    require({c["id"] for c in cases} == set(originals), "B1 missing original cases")
    for case in cases:
        require(case["original"] == originals[case["id"]], "B1 altered frozen case/oracle")
        require(case["whole_case_verdict"] == "HOLD", "B1 cannot clear whole security cases")
    return True

M1_B2_BRANCH = "m1/b2-source-authority"
M1_B2_BASE = "535663960c33c0ebe49481a8681b8625c336fafe"
M1_B2_PATHS = frozenset((
    "CMakeLists.txt", "src/core/runtime_lifecycle.hpp", "src/core/runtime_lifecycle.cpp",
    "src/core/runtime_budget.hpp", "src/core/character_authority.hpp", "src/core/character_authority.cpp",
    "src/security/authority_types.hpp", "src/security/security_clock.hpp", "src/security/security_clock.cpp",
    "src/security/clock_windows.cpp", "src/security/clock_linux.cpp", "src/security/os_entropy.hpp",
    "src/security/os_entropy_windows.cpp", "src/security/os_entropy_linux.cpp",
    "src/security/token_ledger.hpp", "src/security/token_ledger.cpp",
    "src/security/admission_gate.hpp", "src/security/admission_gate.cpp",
    "src/security/source_authority.hpp", "src/security/source_authority.cpp",
    "src/security/ingress_queue.hpp", "src/security/ingress_queue.cpp",
    "tests/security/security_os_tests.cpp", "tests/security/source_authority_tests.cpp",
    "tests/security/source_authority_race_tests.cpp", "tests/security/runtime_lifecycle_tests.cpp",
    "tests/security/runtime_test_support.hpp", "tests/contracts/check_m0_rc1_pins.py",
    "tests/contracts/test_m1_b_custody.py", "tests/contracts/test_m1_b2_custody.py",
    ".github/workflows/m1-b-security.yml", "docs/audits/M1_B2_BUILD_EVIDENCE.md",
    "docs/audits/M1_B_CASE_EXECUTION.json",
))

def check_m1_b2(base, candidate):
    """PR22 B2 only; preserve all historical gates and keep public APIs disabled."""
    require(base == M1_B2_BASE, "B2 main moved: fresh exact-target review required")
    git("merge-base", "--is-ancestor", M1_B2_BASE, candidate)
    changed = set(git("diff", "--name-only", M1_B2_BASE, candidate).splitlines())
    require(changed == M1_B2_PATHS, f"B2 source path drift: {sorted(changed ^ M1_B2_PATHS)}")
    frozen = json.loads(git("show", M1_B2_BASE + ":docs/M0_CONTRACT_PARENT_PINS.json"))
    validate_blob_manifest(frozen, candidate)
    for path in ("include/ars/ars.h", "src/abi/unsupported.cpp", "src/abi/runtime_api.cpp",
                 "src/abi/api_validation.hpp", "cmake/ars_verify_exports.cmake"):
        require(git("rev-parse", candidate + ":" + path) == git("rev-parse", M1_B2_BASE + ":" + path),
                f"B2 changed public ABI/activation or export oracle: {path}")
    old = git("show", M1_B2_BASE + ":tests/contracts/test_m1_b_custody.py")
    expected = old.replace('BASE = "' + M1_B1_BASE + '"\n',
                           'BASE = "' + M1_B1_BASE + '"\nSNAPSHOT = "' + M1_B2_BASE + '"\n')
    expected = expected.replace('"HEAD"', 'SNAPSHOT').replace('"HEAD:include/ars/ars.h"',
                  'SNAPSHOT + ":include/ars/ars.h"').replace('"HEAD:docs/audits/M1_B_CASE_EXECUTION.json"',
                  'SNAPSHOT + ":docs/audits/M1_B_CASE_EXECUTION.json"')
    require(git("show", candidate + ":tests/contracts/test_m1_b_custody.py") == expected,
            "B2 diluted historical B1 custody instead of exact snapshot transition")
    old_gate = git("show", M1_B2_BASE + ":tests/contracts/check_m0_rc1_pins.py")
    new_gate = git("show", candidate + ":tests/contracts/check_m0_rc1_pins.py")
    def historical_segments(source):
        nodes = ast.parse(source).body
        return {node.name: ast.get_source_segment(source, node) for node in nodes
                if isinstance(node, ast.FunctionDef) and node.name in
                {"check_m1_a", "check_m1_b1", "check_consolidation", "validate_blob_manifest"}}
    require(historical_segments(old_gate) == historical_segments(new_gate), "B2 changed frozen historical gates")
    for path in M1_B2_PATHS:
        if path.startswith("src/security/") or path.startswith("src/core/character_authority"):
            source = git("show", candidate + ":" + path)
            require(not re.search(r"\bARS_API\b|ComponentFixture::|\b(random_device|mt19937|srand)\b", source),
                    f"B2 public/mock/weak entropy interface: {path}")
    inventory = json.loads(git("show", M1_B2_BASE + ":docs/G00_SECURITY_87_CASE_INVENTORY.json"))
    ledger = json.loads(git("show", candidate + ":docs/audits/M1_B_CASE_EXECUTION.json"))
    require(ledger["scope"] == "M1-B2_CPU_AUTHORITY_COMPONENTS_ONLY", "B2 ledger scope drift")
    cases = ledger["cases"]
    originals = {c["id"]: c for c in inventory["cases"]}
    require(len(cases) == 87 and len({c["id"] for c in cases}) == 87 and
            {c["id"] for c in cases} == set(originals), "B2 original case inventory drift")
    require(ledger["whole_case_pass_count"] == 0, "B2 fabricated whole-case totals")
    for case in cases:
        require(case["original"] == originals[case["id"]] and case["whole_case_verdict"] == "HOLD",
                "B2 changed frozen scenario/oracle or fabricated whole-case PASS")
    return True

M1_B3_BRANCH = "m1/b3-provenance-publish"
M1_B3_BASE = "c76958e8398e5eefde9b77d095a320cda4aac51a"
M1_B3_PATHS = frozenset((
    'CMakeLists.txt',
    'src/core/runtime_lifecycle.hpp',
    'src/core/runtime_lifecycle.cpp',
    'src/core/runtime_budget.hpp',
    'src/core/character_authority.hpp',
    'src/core/character_authority.cpp',
    'src/security/authority_types.hpp',
    'src/security/source_authority.hpp',
    'src/security/source_authority.cpp',
    'src/motion/provenance.hpp',
    'src/motion/provenance.cpp',
    'src/motion/staged_state.hpp',
    'src/motion/staged_state.cpp',
    'src/motion/cpu_snapshot.hpp',
    'src/motion/cpu_snapshot.cpp',
    'src/motion/publish_gate.hpp',
    'src/motion/publish_gate.cpp',
    'src/motion/semantic_candidate.hpp',
    'src/motion/semantic_candidate.cpp',
    'tests/security/provenance_tests.cpp',
    'tests/security/publish_race_tests.cpp',
    'tests/security/snapshot_lifetime_tests.cpp',
    'tests/security/source_authority_race_tests.cpp',
    'tests/security/runtime_lifecycle_tests.cpp',
    'tests/security/runtime_test_support.hpp',
    'tests/contracts/check_m0_rc1_pins.py',
    'tests/contracts/test_m1_b2_custody.py',
    'tests/contracts/test_m1_b3_custody.py',
    '.github/workflows/m1-b-security.yml',
    'docs/audits/M1_B3_BUILD_EVIDENCE.md',
    'docs/audits/M1_B_CASE_EXECUTION.json',
))

def check_m1_b3(base, candidate):
    """Owner-authorized B3 plus both P2 fixes, above the actual landed B2."""
    require(base == M1_B3_BASE, "B3 main moved: fresh exact-target review required")
    git("merge-base", "--is-ancestor", M1_B3_BASE, candidate)
    changed = set(git("diff", "--name-only", M1_B3_BASE, candidate).splitlines())
    require(changed == M1_B3_PATHS, f"B3 source path drift: {sorted(changed ^ M1_B3_PATHS)}")
    frozen = json.loads(git("show", M1_B3_BASE + ":docs/M0_CONTRACT_PARENT_PINS.json"))
    validate_blob_manifest(frozen, candidate)
    for path in ("include/ars/ars.h", "src/abi/unsupported.cpp", "src/abi/runtime_api.cpp",
                 "src/abi/api_validation.hpp", "cmake/ars_verify_exports.cmake",
                 "tests/contracts/test_m1_b_custody.py", "tests/security/source_authority_tests.cpp"):
        require(git("rev-parse", candidate + ":" + path) == git("rev-parse", M1_B3_BASE + ":" + path),
                f"B3 changed frozen ABI/oracle/historical test: {path}")
    old = git("show", M1_B3_BASE + ":tests/contracts/test_m1_b2_custody.py")
    expected = old.replace('BASE = "' + M1_B2_BASE + '"\n',
                           'BASE = "' + M1_B2_BASE + '"\nSNAPSHOT = "' + M1_B3_BASE + '"\n')
    expected = expected.replace('"HEAD"', 'SNAPSHOT')
    expected = re.sub(r'"HEAD:([^"\n]+)"', r'SNAPSHOT + ":\1"', expected)
    expected = expected.replace('"HEAD:"', 'SNAPSHOT + ":"')
    require(git("show", candidate + ":tests/contracts/test_m1_b2_custody.py") == expected,
            "B3 diluted historical B2 custody rather than binding the landed snapshot")
    old_gate = ast.parse(git("show", M1_B3_BASE + ":tests/contracts/check_m0_rc1_pins.py"))
    new_gate = ast.parse(git("show", candidate + ":tests/contracts/check_m0_rc1_pins.py"))
    names = {"check_consolidation", "check_m1_a", "check_m1_b1", "check_m1_b2", "validate_blob_manifest"}
    def functions(tree):
        return {node.name: ast.dump(node) for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names}
    require(functions(old_gate) == functions(new_gate), "B3 changed historical source gates")
    for path in M1_B3_PATHS:
        if path.startswith("src/"):
            code = git("show", candidate + ":" + path)
            require(not re.search(r"\bARS_API\b|ComponentFixture::|\b(random_device|mt19937|srand)\b", code),
                    f"B3 public/mock/weak interface: {path}")
    inventory = json.loads(git("show", M1_B3_BASE + ":docs/G00_SECURITY_87_CASE_INVENTORY.json"))
    ledger = json.loads(git("show", candidate + ":docs/audits/M1_B_CASE_EXECUTION.json"))
    require(ledger["scope"] == "M1-B3_CPU_PROVENANCE_COMPONENTS_ONLY" and ledger["whole_case_pass_count"] == 0,
            "B3 scope or fabricated whole-case total")
    originals = {case["id"]: case for case in inventory["cases"]}
    cases = ledger["cases"]
    require(len(cases) == 87 and {case["id"] for case in cases} == set(originals), "B3 inventory drift")
    for case in cases:
        require(case["original"] == originals[case["id"]] and case["whole_case_verdict"] == "HOLD",
                "B3 changed original scenario/oracle or fabricated whole-case PASS")
    return True

M1_C1_BRANCH = "m1/c1-native-assets"
M1_C1_BASE = "7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79"
M1_C1_PATHS = frozenset((
    'CMakeLists.txt',
    'cmake/ars_asset_dependencies.cmake',
    'docs/M1_C_DEPENDENCY_LOCK.json',
    'src/assets/asset_limits.hpp',
    'src/assets/owned_bundle.hpp',
    'src/assets/owned_bundle.cpp',
    'src/assets/zip_reader.hpp',
    'src/assets/zip_reader.cpp',
    'src/assets/png_decoder.hpp',
    'src/assets/png_decoder.cpp',
    'src/assets/validated_cpu_model.hpp',
    'src/assets/validated_cpu_model.cpp',
    'src/validation/strict_json.hpp',
    'src/validation/strict_json.cpp',
    'src/validation/manifest_validator.hpp',
    'src/validation/manifest_validator.cpp',
    'src/validation/profile_b_validator.hpp',
    'src/validation/profile_b_validator.cpp',
    'tests/assets/asset_test_support.hpp',
    'tests/assets/zip_reader_tests.cpp',
    'tests/assets/json_schema_tests.cpp',
    'tests/assets/profile_b_tests.cpp',
    'tests/assets/png_decoder_tests.cpp',
    'tests/assets/asset_budget_tests.cpp',
    'tests/assets/fixtures/generate_fixtures.py',
    'tests/assets/fixtures/manifest.json',
    'fuzz/arschar_fuzzer.cpp',
    'fuzz/CMakeLists.txt',
    '.github/workflows/m1-c-assets.yml',
    'tests/contracts/check_m0_rc1_pins.py',
    'tests/contracts/test_m1_b3_custody.py',
    'tests/contracts/test_m1_c_custody.py',
    'docs/audits/M1_C1_BUILD_EVIDENCE.md',
    'docs/audits/M1_C_CASE_EXECUTION.json',
    'src/validation/schema_worker_main.cpp',
    'src/validation/schema_worker_client.hpp',
    'src/validation/schema_worker_client.cpp',
    'src/validation/schema_worker_protocol.hpp',
    'src/validation/schema_worker_windows.cpp',
    'src/validation/schema_worker_linux.cpp',
    'tests/assets/schema_worker_tests.cpp',
))

def check_m1_c1(base, candidate):
    """Owner-approved C1 plus seven-file bounded schema helper amendment."""
    require(base == M1_C1_BASE, "C1 main moved: fresh target review required")
    git("merge-base", "--is-ancestor", base, candidate)
    changed = set(git("diff", "--name-only", base, candidate).splitlines())
    require(changed == M1_C1_PATHS, f"C1 path drift: {sorted(changed ^ M1_C1_PATHS)}")
    frozen = json.loads(git("show", base + ":docs/M0_CONTRACT_PARENT_PINS.json"))
    validate_blob_manifest(frozen, candidate)
    for path in ("include/ars/ars.h", "src/abi/unsupported.cpp", "src/abi/runtime_api.cpp", "cmake/ars_verify_exports.cmake", "docs/G00_SECURITY_87_CASE_INVENTORY.json", "docs/audits/M1_B_CASE_EXECUTION.json"):
        require(git("rev-parse", candidate + ":" + path) == git("rev-parse", base + ":" + path), "C1 changed frozen public/security boundary: " + path)
    old = git("show", base + ":tests/contracts/test_m1_b3_custody.py")
    expected = old.replace('BASE = "' + M1_B3_BASE + '"\n', 'BASE = "' + M1_B3_BASE + '"\nSNAPSHOT = "' + base + '"\n')
    expected = expected.replace('"HEAD"', "SNAPSHOT")
    expected = re.sub(r'"HEAD:([^"\n]+)"', r'SNAPSHOT + ":\1"', expected).replace('"HEAD:"', 'SNAPSHOT + ":"')
    require(git("show", candidate + ":tests/contracts/test_m1_b3_custody.py") == expected, "C1 diluted B3 tests")
    names = {"check_consolidation", "check_m1_a", "check_m1_b1", "check_m1_b2", "check_m1_b3", "validate_blob_manifest"}
    def functions(text):
        return {n.name: ast.dump(n) for n in ast.parse(text).body if isinstance(n, ast.FunctionDef) and n.name in names}
    require(functions(git("show", base + ":tests/contracts/check_m0_rc1_pins.py")) == functions(git("show", candidate + ":tests/contracts/check_m0_rc1_pins.py")), "C1 diluted historical gates")
    lock = json.loads(git("show", candidate + ":docs/M1_C_DEPENDENCY_LOCK.json"))
    require(lock["material_decision_required"]["status"] == "OWNER_APPROVED_YES" and not lock["public_activation"] and not lock["merge_authorized"], "C1 scope/activation flags")
    for dep in lock["dependencies"]:
        require(re.fullmatch(r"[0-9a-f]{40}", dep["commit"]) is not None and re.fullmatch(r"[0-9a-f]{64}", dep["archive_sha256"]) is not None, "Unpinned dependency")
        require(dep["commit"] in git("show", candidate + ":cmake/ars_asset_dependencies.cmake") and dep["archive_sha256"] in git("show", candidate + ":cmake/ars_asset_dependencies.cmake"), "Dependency lock/build mismatch")
    for path in M1_C1_PATHS:
        if path.startswith("src/"):
            require(not re.search(r"\bARS_API\b|\b(system|popen)\s*\(", git("show", candidate + ":" + path)), "C1 public/shell boundary: " + path)
    ledger = json.loads(git("show", candidate + ":docs/audits/M1_C_CASE_EXECUTION.json"))
    require(ledger["scope"] == "M1_C1_PRIVATE_CPU_2D_ONLY" and not ledger["public_activation"] and ledger["full_g02_verdict"] == "HOLD", "C1 fabricated public/full G02 result")
    return True


M1_C2_BRANCH = "m1/c2-glb-cpu"
M1_C2_BASE = "9f5d6b8a634138e8dedae8385dcb783cd385b6c8"
M1_C2_PATHS = frozenset((
    "CMakeLists.txt", "cmake/ars_asset_dependencies.cmake", "docs/M1_C_DEPENDENCY_LOCK.json",
    "src/assets/glb_reader.hpp", "src/assets/glb_reader.cpp",
    "src/assets/validated_cpu_model.hpp", "src/assets/validated_cpu_model.cpp",
    "src/validation/glb_validator.hpp", "src/validation/glb_validator.cpp", "src/validation/manifest_validator.cpp",
    "tests/assets/glb_reader_tests.cpp", "tests/assets/glb_validation_tests.cpp", "tests/assets/dual_mode_tests.cpp",
    "tests/assets/asset_test_support.hpp", "tests/assets/fixtures/generate_fixtures.py", "tests/assets/fixtures/manifest.json",
    "fuzz/glb_fuzzer.cpp", "fuzz/CMakeLists.txt", ".github/workflows/m1-c-assets.yml",
    "tests/contracts/check_m0_rc1_pins.py", "tests/contracts/test_m1_c_custody.py", "tests/contracts/test_m1_c2_custody.py",
    "docs/audits/M1_C2_BUILD_EVIDENCE.md", "docs/audits/M1_C_CASE_EXECUTION.json",
))

def check_m1_c2(base, candidate):
    """C2 child of the exact unmerged PR27 source, explicitly authorized by owner."""
    require(base == M1_C2_BASE, "C2 parent moved: restack requires source review")
    git("merge-base", "--is-ancestor", base, candidate)
    changed = set(git("diff", "--name-only", base, candidate).splitlines())
    require(changed == M1_C2_PATHS, f"C2 path drift: {sorted(changed ^ M1_C2_PATHS)}")
    frozen = json.loads(git("show", base + ":docs/M0_CONTRACT_PARENT_PINS.json"))
    validate_blob_manifest(frozen, candidate)
    for path in ("include/ars/ars.h", "src/abi/unsupported.cpp", "src/abi/runtime_api.cpp", "cmake/ars_verify_exports.cmake", "docs/G00_SECURITY_87_CASE_INVENTORY.json", "docs/audits/M1_B_CASE_EXECUTION.json"):
        require(git("rev-parse", candidate + ":" + path) == git("rev-parse", base + ":" + path), "C2 changed frozen boundary: " + path)
    names = {"check_consolidation", "check_m1_a", "check_m1_b1", "check_m1_b2", "check_m1_b3", "check_m1_c1", "validate_blob_manifest"}
    def functions(text):
        return {n.name: ast.dump(n) for n in ast.parse(text).body if isinstance(n, ast.FunctionDef) and n.name in names}
    require(functions(git("show", base + ":tests/contracts/check_m0_rc1_pins.py")) == functions(git("show", candidate + ":tests/contracts/check_m0_rc1_pins.py")), "C2 diluted historical gates")
    old = git("show", base + ":tests/contracts/test_m1_c_custody.py")
    expected = old.replace('BASE = "' + M1_C1_BASE + '"\n', 'BASE = "' + M1_C1_BASE + '"\nSNAPSHOT = "' + base + '"\n')
    expected = expected.replace('"HEAD"', "SNAPSHOT")
    expected = re.sub(r'"HEAD:([^"\n]+)"', r'SNAPSHOT + ":\1"', expected).replace('"HEAD:"', 'SNAPSHOT + ":"')
    require(git("show", candidate + ":tests/contracts/test_m1_c_custody.py") == expected, "C2 diluted C1 tests")
    lock = json.loads(git("show", candidate + ":docs/M1_C_DEPENDENCY_LOCK.json"))
    require(lock["dependencies"] == json.loads(git("show", base + ":docs/M1_C_DEPENDENCY_LOCK.json"))["dependencies"], "C2 dependency drift")
    stack = lock["c2_stack"]
    require(stack["parent_sha"] == base and stack["parent_branch"] == M1_C1_BRANCH and stack["owner_unmerged_stack_approved"] is True and not lock["public_activation"] and not lock["merge_authorized"], "C2 unauthorized stack/activation")
    build = git("show", candidate + ":CMakeLists.txt")
    require(base in build and 'add_library(ars_c1_asset_components' in build and 'c1-frozen-corpus' in build, "C2 omitted frozen C1 native replay")
    for path in M1_C2_PATHS:
        if path.startswith("src/"):
            require(not re.search(r"\bARS_API\b|\b(system|popen)\s*\(", git("show", candidate + ":" + path)), "C2 public/shell boundary: " + path)
    ledger = json.loads(git("show", candidate + ":docs/audits/M1_C_CASE_EXECUTION.json"))
    require(ledger["scope"] == "M1_C2_PRIVATE_CPU_GLB_DUAL" and ledger["base"] == base and not ledger["public_activation"] and ledger["full_g02_verdict"] == "HOLD" and ledger["B_whole_case_HOLD"] == 87 and ledger["B_whole_case_PASS"] == 0, "C2 fabricated activation/full G02 result")
    return True

M1_D_BRANCH = "m1/d-d3d11-interop-spike"
M1_D_BASE = "f5b198137f6a96a2e33bb8ac206e53c3815bbbbc"
M1_D_GPU_SOURCE = "0bcdca7f8024a1a6765526942591406bdb25a0bd"
M1_D_PATHS = frozenset((
    ".github/workflows/m1-d-gpu.yml", "CMakeLists.txt",
    "docs/M1_D_CLOSE_LOSS_NEXT_IMPLEMENTATION.md",
    "docs/M1_D_GPU_CASE_EXECUTION.json", "docs/audits/M1_D_BUILD_EVIDENCE.md",
    "docs/audits/M1_D_BUILD_RAW.txt", "docs/audits/M1_D_CTEST_RAW.txt",
    "docs/audits/M1_D_C2_LOCAL_RAW.txt",
    "docs/audits/M1_D_D1_RAW.txt", "docs/audits/M1_D_D2_RAW.txt",
    "docs/audits/M1_D_D4_RAW.txt", "docs/audits/M1_D_D6_RAW.txt",
    "docs/audits/M1_D_D7_CLOSE_RAW.txt", "docs/audits/M1_D_D8_INJECTED_RAW.txt",
    "docs/audits/M1_D_GATES_REASSESSMENT_20261017.md",
    "docs/audits/M1_D_STATIC_BUILD_RAW.txt", "docs/audits/M1_D_STATIC_CTEST_RAW.txt",
    "src/graphics/backend.hpp",
    "src/graphics/d3d11/host_device.cpp", "src/graphics/d3d11/host_device.hpp",
    "src/graphics/d3d11/close_coordinator.cpp", "src/graphics/d3d11/close_coordinator.hpp",
    "src/graphics/d3d11/query_retirement.cpp", "src/graphics/d3d11/query_retirement.hpp",
    "src/graphics/d3d11/state_scope.cpp", "src/graphics/d3d11/state_scope.hpp",
    "tests/contracts/check_m0_rc1_pins.py", "tests/contracts/test_m1_c2_custody.py",
    "tests/contracts/test_m1_d_custody.py", "tests/gpu/d3d11_host_smoke.cpp",
    "tests/gpu/d3d11_query_retirement.cpp", "tests/gpu/d3d11_live_capture.cpp",
    "tests/gpu/d3d11_state_restore.cpp", "tests/gpu/d3d11_close_physical.cpp",
    "tests/gpu/d3d11_close_protocol.cpp",
))


def check_m1_d(base, candidate):
    """Accept only the private GPU spike stacked on the exact merged C2 base."""
    require(base == M1_D_BASE, "M1-D base moved: refresh and review")
    git("merge-base", "--is-ancestor", base, candidate)
    git("merge-base", "--is-ancestor", M1_D_GPU_SOURCE, candidate)
    changed = set(git("diff", "--name-only", base, candidate).splitlines())
    require(changed == M1_D_PATHS, f"M1-D path drift: {sorted(changed ^ M1_D_PATHS)}")
    frozen = ("include/ars/ars.h", "src/abi/unsupported.cpp", "src/abi/runtime_api.cpp",
              "docs/ADR_0001_D3D11_G00_DESIGN_RC1.md",
              "docs/G00_SECURITY_87_CASE_INVENTORY.json", "docs/M0_CONTRACT_PARENT_PINS.json")
    for path in frozen:
        require(git("rev-parse", candidate + ":" + path) == git("rev-parse", base + ":" + path),
                "M1-D changed frozen boundary: " + path)
    gpu_paths = tuple(path for path in M1_D_PATHS if path.startswith(("src/graphics/", "tests/gpu/")))
    for path in gpu_paths:
        require(git("rev-parse", candidate + ":" + path) == git("rev-parse", M1_D_GPU_SOURCE + ":" + path),
                "Physical evidence no longer names exact GPU source: " + path)
    old = ast.parse(git("show", base + ":tests/contracts/check_m0_rc1_pins.py"))
    new = ast.parse(git("show", candidate + ":tests/contracts/check_m0_rc1_pins.py"))
    for name in ("check_m1_a", "check_m1_b1", "check_m1_b2", "check_m1_b3", "check_m1_c1", "check_m1_c2", "validate_blob_manifest"):
        prior = next(node for node in old.body if isinstance(node, ast.FunctionDef) and node.name == name)
        current = next(node for node in new.body if isinstance(node, ast.FunctionDef) and node.name == name)
        require(ast.dump(prior) == ast.dump(current), "M1-D diluted historical gate: " + name)
    ledger = json.loads(git("show", candidate + ":docs/M1_D_GPU_CASE_EXECUTION.json"))
    require(ledger["tested_source_sha"] == M1_D_GPU_SOURCE and
            ledger["verdict"] == "PRIVATE_SPIKE_PARTIAL; M1-D_ACCEPTANCE_HOLD; PUBLIC_RENDER_DISABLED",
            "M1-D evidence fabricated activation or full PASS")
    return True


M2_A_BRANCH = "m2/2d-render-foundation"
M2_A_BASE = "dffe10c16d5585c0d50fd7745f310543e719c522"
M2_A_PATHS = frozenset((
    ".github/workflows/m2-2d-reference.yml", "CMakeLists.txt",
    "docs/M2_RENDERING_EXECUTION_PLAN.md", "docs/audits/M2_A_BUILD_EVIDENCE.md",
    "docs/audits/M2_A_BUILD_RAW.txt", "docs/audits/M2_A_CTEST_RAW.txt",
    "src/rendering/scene2d.cpp", "src/rendering/scene2d.hpp",
    "tests/rendering/scene2d_tests.cpp",
    "tests/contracts/check_m0_rc1_pins.py", "tests/contracts/test_m1_d_custody.py",
    "tests/contracts/test_m2_a_custody.py",
))


def check_m2_a(base, candidate):
    """The first M2 increment must be stacked on the exact held M1-D head."""
    require(base == M2_A_BASE, "M2-A parent moved: refresh and review")
    git("merge-base", "--is-ancestor", base, candidate)
    require(set(git("diff", "--name-only", base, candidate).splitlines()) == M2_A_PATHS,
            "M2-A path drift")
    check_m1_d(M1_D_BASE, base)
    for path in ("include/ars/ars.h", "src/abi/unsupported.cpp",
                 "src/abi/runtime_api.cpp", "docs/M1_D_GPU_CASE_EXECUTION.json"):
        require(git("rev-parse", candidate + ":" + path) == git("rev-parse", base + ":" + path),
                "M2-A changed held/public boundary: " + path)
    return True


M2_B_BRANCH = "m2/2d-offscreen"
M2_B_BASE = "10f542f7acb10527bace0dca6d7c608b20018896"
M2_B_PATHS = frozenset((
    ".github/workflows/m2-2d-pixels.yml", "CMakeLists.txt",
    "docs/M2_B_PIXEL_CONTRACT.md", "docs/audits/M2_B_BUILD_EVIDENCE.md",
    "docs/audits/M2_B_BUILD_RAW.txt", "docs/audits/M2_B_CTEST_RAW.txt",
    "docs/audits/M2_B_HARDWARE_RAW.txt",
    "src/graphics/d3d11/state_scope.cpp", "src/graphics/d3d11/state_scope.hpp",
    "src/rendering/d3d11_scene2d.cpp", "src/rendering/d3d11_scene2d.hpp",
    "tests/rendering/d3d11_scene2d_pixels.cpp",
    "tests/contracts/check_m0_rc1_pins.py", "tests/contracts/test_m2_a_custody.py",
    "tests/contracts/test_m2_b_custody.py",
))


def check_m2_b(base, candidate):
    """Private pixel pass extends only the pinned M2-A head."""
    require(base == M2_B_BASE, "M2-B parent moved: refresh and review")
    git("merge-base", "--is-ancestor", base, candidate)
    check_m2_a(M2_A_BASE, base)
    require(set(git("diff", "--name-only", base, candidate).splitlines()) == M2_B_PATHS,
            "M2-B path drift")
    for path in ("include/ars/ars.h", "src/abi/unsupported.cpp",
                 "src/abi/runtime_api.cpp", "docs/M1_D_GPU_CASE_EXECUTION.json"):
        require(git("rev-parse", candidate + ":" + path) == git("rev-parse", base + ":" + path),
                "M2-B changed held/public boundary: " + path)
    return True


def validate_blob_manifest(obj, revision="HEAD"):
    """Check immutable required paths, blob syntax and Git tree contents.

    The two keysets are deliberately defined in CODE, not in the editable
    manifest. This is a source identity gate, not a GitHub actor review.
    """
    for field, expected in (("source_blobs", REQUIRED_PREDECESSOR),
                            ("reviewed_g00_blobs", REQUIRED_G00)):
        paths = obj.get(field)
        require(type(paths) is dict, f"Missing required {field} manifest")
        require(set(paths) == expected and len(paths) == len(expected),
                f"{field}: required immutable path set/count differs")
        for path, sha in paths.items():
            require(re.fullmatch(r"[0-9a-f]{40}", sha) is not None,
                    f"Invalid SHA for {path}")
            require(not path.startswith("/") and ".." not in Path(path).parts,
                    f"Unsafe provenance path: {path}")
            actual = git("rev-parse", revision + ":" + path)
            require(actual == sha,
                    f"Incorrect {field} blob: {path}; expected {sha}, got {actual}")
    return True

def check():
    obj = json.loads(PINS.read_text(encoding="utf-8"))
    pr1 = obj["stack"]["pr1_head"]
    pr2 = obj["stack"]["pr2_head"]
    require(obj["contract_candidate"] == "M0_RC1", "Wrong RC1 pin")
    require(re.fullmatch(r"[0-9a-f]{40}", pr1) is not None, "PR1 SHA malformed")
    require(re.fullmatch(r"[0-9a-f]{40}", pr2) is not None, "PR2 SHA malformed")
    git("cat-file", "-e", pr1 + "^{commit}")
    git("cat-file", "-e", pr2 + "^{commit}")
    subprocess.run(["git", "merge-base", "--is-ancestor", pr1, pr2],
                   cwd=ROOT, check=True)
    if os.getenv("GITHUB_EVENT_NAME") == "pull_request":
        expected_parent = git("rev-parse", "HEAD^1")
        # Exact authorization is per candidate kind, never a broad audit-branch bypass.
        child = obj.get("audit_child", {})
        if os.getenv("GITHUB_HEAD_REF") == M1_C2_BRANCH:
            expected = M1_C2_BASE
            check_m1_c2(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == M2_A_BRANCH:
            expected = M2_A_BASE
            check_m2_a(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == M2_B_BRANCH:
            expected = M2_B_BASE
            check_m2_b(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == M1_D_BRANCH:
            expected = M1_D_BASE
            check_m1_d(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == M1_C1_BRANCH:
            expected = M1_C1_BASE
            check_m1_c1(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == M1_B3_BRANCH:
            expected = M1_B3_BASE
            check_m1_b3(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == M1_B2_BRANCH:
            expected = M1_B2_BASE
            check_m1_b2(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == M1_B1_BRANCH:
            expected = M1_B1_BASE
            check_m1_b1(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == M1_A_BRANCH:
            expected = M1_A_BASE
            check_m1_a(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == CONSOLIDATION_BRANCH:
            expected = CONSOLIDATION_MAIN
            check_consolidation(expected_parent, git("rev-parse", "HEAD^2"))
        elif os.getenv("GITHUB_HEAD_REF") == child.get("head_branch"):
            expected = child["exact_parent_head"]
            require(re.fullmatch(r"[0-9a-f]{40}", expected) is not None,
                    "Malformed audit-child parent pin")
            git("cat-file", "-e", expected + "^{commit}")
            subprocess.run(["git", "merge-base", "--is-ancestor", pr2, expected],
                           cwd=ROOT, check=True)
        elif os.getenv("GITHUB_HEAD_REF") == obj.get("audit_successor", {}).get("head_branch"):
            successor = obj["audit_successor"]
            expected = successor["exact_parent_head"]
            require(re.fullmatch(r"[0-9a-f]{40}", expected) is not None,
                    "Malformed audit-successor parent pin")
            git("cat-file", "-e", expected + "^{commit}")
            subprocess.run(["git", "merge-base", "--is-ancestor",
                            child["exact_parent_head"], expected], cwd=ROOT, check=True)
        elif os.getenv("GITHUB_HEAD_REF") == obj.get("audit_p204", {}).get("head_branch"):
            successor = obj["audit_p204"]
            expected = successor["exact_parent_head"]
            require(re.fullmatch(r"[0-9a-f]{40}", expected) is not None,
                    "Malformed P2-04 audit parent")
            git("cat-file", "-e", expected+"^{commit}")
            subprocess.run(["git", "merge-base", "--is-ancestor",
                            obj["audit_successor"]["exact_parent_head"], expected],
                           cwd=ROOT, check=True)
        elif os.getenv("GITHUB_HEAD_REF") == obj.get("main_integration", {}).get("head_branch"):
            integration = obj["main_integration"]
            expected = integration["main_base_head"]
            frozen = integration["owner_approved_g00_sha"]
            require(re.fullmatch(r"[0-9a-f]{40}", expected) is not None,
                    "Malformed main integration pin")
            require(re.fullmatch(r"[0-9a-f]{40}", frozen) is not None,
                    "Malformed frozen G00 source SHA")
            require(frozen == "fe3e680a44ac4e71ef57dc177e73ae31198fcbd0",
                    "G00 owner-freeze SHA changed without a new owner decision")
            require(expected == "637bdae7373aecbf50b18834f3118421d921fe3a",
                    "Main target SHA changed without fresh integration review")
            git("cat-file", "-e", frozen + "^{commit}")
            git("cat-file", "-e", expected + "^{commit}")
            subprocess.run(["git", "merge-base", "--is-ancestor", expected, frozen],
                           cwd=ROOT, check=True)
            candidate = git("rev-parse", "HEAD^2")
            subprocess.run(["git", "merge-base", "--is-ancestor", frozen, candidate],
                           cwd=ROOT, check=True)
            allowed = {"tests/contracts/check_m0_rc1_pins.py",
                       "docs/M0_CONTRACT_PARENT_PINS.json"}
            changed = set(git("diff", "--name-only", frozen, candidate).splitlines())
            require(changed == allowed,
                    f"Material G00 source drift after freeze: {sorted(changed ^ allowed)}")
        else:
            expected = pr2
        require(expected_parent == expected,
                f"RC1 base drift: expected {expected}, got {expected_parent}")
    else:
        subprocess.run(["git", "merge-base", "--is-ancestor", pr2, "HEAD"],
                       cwd=ROOT, check=True)
    validate_blob_manifest(obj)
    rc1 = (ROOT / "docs/V0_1_CANONICAL_CONTRACT_RC1.md").read_text("utf-8")
    readiness = (ROOT / "docs/M0_V0_1_FREEZE_READINESS.md").read_text("utf-8")
    abi = (ROOT / "docs/V0_1_PUBLIC_ABI_FREEZE_MAP.md").read_text("utf-8")
    fmt = (ROOT / "docs/V0_1_FORMAT_VALIDATION_FREEZE_MAP.md").read_text("utf-8")
    m1 = (ROOT / "docs/M1_SOURCE_BOOTSTRAP_PLAN.md").read_text("utf-8")
    gpu = (ROOT / "docs/ADR_0001_D3D11_G00_DESIGN_RC1.md").read_text("utf-8")
    for doc in (rc1, readiness, abi, fmt, m1):
        require("G00" in doc and ("HOLD" in doc or "NOT FROZEN" in doc or "pending" in doc.lower()),
                "Freeze doc incorrectly omits G00/HOLD boundary")
    for sha in (pr1, pr2):
        require(sha in rc1, f"RC1 missing pinned parent {sha}")
    require("G00" in abi and "G01" in abi and "G10" in abi,
            "ABI proof stages must be distinct")
    require("G00" in m1 and "M1-D" in m1, "GPU spike must remain staged")
    require("PROFILE B" in rc1.upper() and "PROFILE B" in fmt.upper(), "2D decision drift")
    require("HOST_DEVICE" in rc1 and "ADR-0001" in readiness, "GPU design drift")
    require("HOST_DEVICE" in gpu and "M1-D" in gpu and "NOT RATIFIED" in gpu,
            "ADR-0001 design/physical split drift")
    require("BAD_ASSET / UNSUPPORTED" not in fmt and "No implicit choice" not in fmt,
            "Format error/profile selection remains ambiguous")
    require("ARS_E_BAD_ASSET" in rc1 and "ARS_E_UNSUPPORTED" in rc1 and
            "ARS_E_OUT_OF_BUDGET" in rc1, "Missing fixed taxonomy")
    require("87" in readiness, "R5 security case count drift")
    require(obj["stage_status"]["G00"] == "HOLD_DESIGN", "G00 status not hold")
    require(obj["stage_status"]["ADR_0001"] == "OWNER_DESIGN_ACCEPTED_REVIEW_HOLD",
            "ADR cannot be auto-approved")
    print("PASS RC1 provenance: PR8/PR9 ancestry, 9 predecessor and 8 reviewed G00 blob pins")
    print("PASS RC1 consistency: G00 design vs implementation, format/ABI/HOLD")
    print("NOTE: no GPU/runtime, hostile asset decoder or real SDK claim")

if __name__ == "__main__":
    try:
        check()
    except (AssertionError, subprocess.CalledProcessError, KeyError, OSError) as exc:
        print(f"FAIL RC1 contract verification: {exc}", file=sys.stderr)
        sys.exit(1)
