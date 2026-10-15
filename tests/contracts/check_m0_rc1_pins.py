#!/usr/bin/env python3
"""Fail-closed M0 RC1 provenance and doc consistency check for CI.

Not a GitHub API check of live remote PR heads. A PR synthetic merge validates
its EXACT base checkout SHA. If the remote parent moves, the new PR check must
be re-run and pin file updated after source review.
"""
from __future__ import annotations

import json
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
        if os.getenv("GITHUB_HEAD_REF") == M1_B1_BRANCH:
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
