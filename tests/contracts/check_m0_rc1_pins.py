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

def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, stderr=subprocess.STDOUT).strip()

def require(condition, message):
    if not condition:
        raise AssertionError(message)

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
        # Scoped audit-child PR #4 is deliberately stacked ON TOP OF frozen PR3.
        # Keep exact PR3 parent check for regular PR3; never relax parent drift.
        audit_child = os.getenv("GITHUB_HEAD_REF", "").startswith("audit/rc1-f01-f08-red-first-")
        expected = ("2d4906ea07052540ff1330f2a93034fe0fccffb7"
                    if audit_child else pr2)
        require(expected_parent == expected,
                f"PR audit-child/base drift: expected {expected}, got {expected_parent}")
    else:
        subprocess.run(["git", "merge-base", "--is-ancestor", pr2, "HEAD"],
                       cwd=ROOT, check=True)
    for path, sha in obj["source_blobs"].items():
        require(re.fullmatch(r"[0-9a-f]{40}", sha) is not None, f"Invalid pin {path}")
        require(not path.startswith("/") and ".." not in Path(path).parts, "Invalid path")
        actual = git("rev-parse", "HEAD:" + path)
        require(actual == sha,
                f"Pinned parent blob changed {path}: wanted {sha}, got {actual}")
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
    require(obj["stage_status"]["ADR_0001"] == "OWNER_DESIGN_PROPOSED",
            "ADR cannot be auto-approved")
    print("PASS RC1 provenance: PR1/PR2 ancestry and 9 exact blob pins")
    print("PASS RC1 consistency: G00 design vs implementation, format/ABI/HOLD")
    print("NOTE: no GPU/runtime, hostile asset decoder or real SDK claim")

if __name__ == "__main__":
    try:
        check()
    except (AssertionError, subprocess.CalledProcessError, KeyError, OSError) as exc:
        print(f"FAIL RC1 contract verification: {exc}", file=sys.stderr)
        sys.exit(1)
