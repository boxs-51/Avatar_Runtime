"""Opt-in private F1 smoke: CPU package preflight plus native pose replay.

Only a model beneath ignored local-assets is selected. Output is a coarse
status token; paths, parameter IDs, hashes and model-derived data stay local.
"""

from __future__ import annotations

import json
from dataclasses import asdict, replace
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import platform
import re
import stat
import subprocess
import sys
import threading
import uuid
from unittest.mock import patch

from moc3_package_admission import AdmissionError, BudgetPolicy, admit_package
from moc3_session import AdmittedSession, EvaluatorError, MAX_TOTAL_PEAK_COMMIT
from moc3_memory import process_memory


ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "build" / "moc3-p1-native" / "Release" / "ars_moc3_harness_bridge.exe"


def _private_manifest() -> Path | None:
    local = ROOT / "local-assets"
    if not local.is_dir():
        return None
    manifests = list(local.rglob("*.model3.json"))
    if len(manifests) != 1:
        return None
    candidate = manifests[0]
    current = local
    for part in (None, *candidate.relative_to(local).parts):
        if part is not None:
            current = current / part
        attributes = getattr(current.lstat(), "st_file_attributes", 0)
        if current.is_symlink() or attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0):
            return None
    return candidate


def eligible() -> bool:
    try:
        return BRIDGE.is_file() and _private_manifest() is not None
    except (OSError, ValueError):
        return False


def run(packet: dict | None = None, progress=lambda _stage: None) -> bool:
    if not __debug__:
        raise EvaluatorError("OPTIMIZED_VERIFIER_DISABLED")
    packet = packet if packet is not None else {}
    cases = packet.setdefault("cases", [])
    def passed(case_id):
        cases.append({"case_id": case_id, "result": "PASS"})
    def requires_reject(action, error_type):
        try:
            action()
        except error_type:
            return
        raise AssertionError("expected typed rejection")

    if not eligible():
        return False
    manifest = _private_manifest()
    if manifest is None:
        return False
    progress("PACKAGE")
    policy = BudgetPolicy()
    packet["policy"] = {**asdict(policy), "max_total_peak_commit_bytes": MAX_TOTAL_PEAK_COMMIT}
    package = admit_package(manifest.parent, manifest.name, policy)
    assert package.textures and not package.moc_evaluated
    assert all(len(image.rgba) == image.width * image.height * 4 for image in package.textures)
    assert package.working_bound <= policy.max_working_bytes
    packet["private_input"] = {"moc_sha256": hashlib.sha256(package.moc_bytes).hexdigest(),
                               "atlas_sha256": [hashlib.sha256(t.rgba).hexdigest()
                                                for t in package.textures],
                               "disabled_features": package.disabled_linked_features}
    passed("F1-PACKAGE-IMMUTABLE-GRAPH")
    progress("EVALUATOR")
    with AdmittedSession(BRIDGE) as session:
        session.replace(package)
        assert session.package.moc_evaluated and session.package_generation == 1
        assert session.feature_inventory[-1] < len(package.textures)
        packet["feature_inventory"] = session.feature_inventory
        passed("F1-BLOB-HANDOFF-ATLAS-BINDING")
        assert session.command("MARK") == ["MARKED"]
        session.step({})
        assert session.command("COMPARE") == ["SAME"]
        session.step({"ParamAngleX": 10})
        assert session.command("COMPARE") == ["DIFFERENT"]
        session.step({})
        assert session.command("COMPARE") == ["SAME"]
        session.step({})
        assert session.command("COMPARE") == ["SAME"]
        passed("F1-FULL-DRAWABLE-POSE-RESET-REPEAT")
        progress("NEGATIVE")
        state = session.command("STATE")
        for pose in ({"__invalid_parameter__": 1}, {"ParamAngleX": float("nan")},
                     {"ParamAngleX": 1e30}, {"ParamAngleX": 10**1000}):
            requires_reject(lambda: session.step(pose), EvaluatorError)
            assert session.command("STATE") == state
        duplicate = session.command("FRAME 2\nParamAngleX 1\nParamAngleX 2")
        assert duplicate[0] == "ERR" and session.command("STATE") == state
        requires_reject(lambda: session.step({}, expected_generation=0), EvaluatorError)
        thread_results = []
        def wrong_thread():
            try:
                session.step({})
            except EvaluatorError as error:
                thread_results.append(error.code)
        worker = threading.Thread(target=wrong_thread)
        worker.start(); worker.join(timeout=5)
        assert not worker.is_alive() and thread_results == ["WRONG_THREAD"]
        assert session.command("STATE") == state
        passed("F1-INVALID-DUPLICATE-STALE-WRONG-THREAD")
        for index in range(session.feature_inventory[0]):
            assert session.command(f"FAULT {index}") == ["FAULT_SET"]
            requires_reject(lambda: session.step({"ParamAngleX": 10}), EvaluatorError)
            assert session.command("STATE") == state
            assert session.command("COMPARE") == ["SAME"]
        session.command("FAULT -1")
        session.step({"ParamAngleX": 10})
        assert session.command("COMPARE") == ["DIFFERENT"]
        session.step({})
        passed("F1-EVERY-DRAWABLE-ALLOCATION-ROLLBACK")
        state = session.command("STATE")
        old_package = session.package
        old_pid = session.process.pid
        for fault in range(6):
            requires_reject(lambda: session.replace(package, fault=fault), EvaluatorError)
            assert session.package is old_package and session.process.pid == old_pid
            assert session.package_generation == 1 and session.command("STATE") == state
        requires_reject(lambda: session.replace(replace(package, moc_bytes=b"NOPE"+bytes(64))),
                        EvaluatorError)
        if session.feature_inventory[-1] > 0:
            requires_reject(lambda: session.replace(replace(package, textures=package.textures[:1])),
                            EvaluatorError)
        requires_reject(lambda: session.load(manifest.parent, manifest.name,
                                             requested_features=("physics",)), AdmissionError)
        assert session.package is old_package and session.command("STATE") == state
        passed("F1-REPLACEMENT-CANDIDATE-FAILURE-PRESERVES-OLD")
        spawned = []
        original_popen = subprocess.Popen
        def record_spawn(*args, **kwargs):
            process = original_popen(*args, **kwargs)
            spawned.append(process)
            return process
        with patch("moc3_session.subprocess.Popen", side_effect=record_spawn), \
                patch("moc3_session.threading.Thread.start", side_effect=RuntimeError("injected")):
            requires_reject(lambda: session.replace(package), RuntimeError)
        assert spawned and all(process.poll() is not None for process in spawned)
        assert session.process.pid == old_pid and session.command("STATE") == state
        passed("F1-WORKER-BOOT-FAILURE-CLEANUP")
        session.peak_commit_limit = 1
        requires_reject(lambda: session.replace(package), EvaluatorError)
        session.peak_commit_limit = MAX_TOTAL_PEAK_COMMIT
        assert session.process.pid == old_pid and session.command("STATE") == state
        passed("F1-MEASURED-MEMORY-REJECTION-PRESERVES-OLD")
        session.replace(package)
        assert session.package_generation == 2 and session.process.pid != old_pid
        requires_reject(lambda: session.step({}, expected_generation=1), EvaluatorError)
        session.step({}, expected_generation=2)
        passed("F1-SUCCESSFUL-REPLACEMENT-GENERATION")
        progress("MEMORY")
        native = process_memory(session.process.pid)
        python = process_memory()
        combined = python["peak_commit_bytes"] + max(session.native_peak_pair_bytes,
                                                     native["peak_commit_bytes"])
        assert combined <= MAX_TOTAL_PEAK_COMMIT
        packet["memory"] = {"python": python, "native": native,
                            "max_native_pair_peak_commit_bytes": session.native_peak_pair_bytes,
                            "conservative_total_peak_commit_bytes": combined,
                            "package_working_bound_bytes": package.working_bound}
        passed("F1-WINDOWS-LIFETIME-PEAK-COMMIT")
    # A separate native executable checks all open/snapshot checkpoints and
    # mutates/releases the caller blob after open, with full drawable equality.
    native_test = BRIDGE.with_name("ars_moc3_snapshot_tests.exe")
    checked = subprocess.run([str(native_test), str(ROOT / "build" / "moc3-f1-native-check"),
                              str(manifest.parent), package.moc_reference, "ParamAngleX", "10"],
                             capture_output=True, timeout=60, check=False)
    assert checked.returncode == 0 and b"MOC3_SNAPSHOT_PASS" in checked.stdout
    passed("F1-NATIVE-ALL-OPEN-CHECKPOINTS-AND-BLOB-LIFETIME")
    progress("COMPLETE")
    return True


if __name__ == "__main__":
    packet = {"schema_version": 1, "phase": "F1", "proof_tier": "trusted_local_cpu",
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "environment": {"platform": platform.platform(), "python": sys.version},
              "cases": []}
    files = ("scripts/moc3_local_replay.py", "scripts/moc3_package_admission.py",
             "scripts/moc3_session.py", "scripts/moc3_memory.py", "src/moc3/harness_bridge.cpp",
             "src/moc3/independent_snapshot.cpp", "src/moc3/independent_snapshot.hpp",
             "tests/moc3/snapshot_tests.cpp", "tests/moc3/test_package_admission.py",
             "tests/moc3/test_harness_api.py", "scripts/moc3_test_harness.py",
             "scripts/moc3_harness_client.py", "CMakeLists.txt")
    packet["source_sha256"] = {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                               for name in files}
    packet["binary_sha256"] = hashlib.sha256(BRIDGE.read_bytes()).hexdigest() if BRIDGE.is_file() else None
    native_test_binary = BRIDGE.with_name("ars_moc3_snapshot_tests.exe")
    packet["native_test_sha256"] = (hashlib.sha256(native_test_binary.read_bytes()).hexdigest()
                                      if native_test_binary.is_file() else None)
    dependency = subprocess.run(
        ["git", "-C", str(ROOT / "build" / "moc3-i0" / "purismcore"), "rev-parse", "HEAD"],
        capture_output=True, text=True, check=False)
    packet["dependency_revision"] = dependency.stdout.strip() if dependency.returncode == 0 else None
    compiler_files = list((ROOT/"build"/"moc3-p1-native"/"CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if compiler_files:
        compiler = compiler_files[-1].read_text(encoding="utf-8")
        packet["environment"]["cpp_compiler"] = re.findall(
            r'set\(CMAKE_CXX_COMPILER_(?:ID|VERSION) "([^"]+)"\)', compiler)
    packet["parent_head"] = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                            capture_output=True, text=True, check=True).stdout.strip()
    try:
        passed = run(packet, lambda stage: print(f"F1_STAGE {stage}", flush=True))
    except (AdmissionError, EvaluatorError, OSError, ValueError, KeyError, AssertionError,
            subprocess.SubprocessError) as error:
        packet["error_type"] = type(error).__name__
        packet["error_code"] = getattr(error, "code", "LOCAL_ASSERTION_OR_RUNNER_FAILURE")
        passed = False
    packet["verdict"] = "PASS" if passed else "FAIL"
    packet["completed_utc"] = datetime.now(timezone.utc).isoformat()
    evidence = ROOT / "build" / "moc3-f1-local"
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / f"run-{uuid.uuid4().hex}.json").write_text(json.dumps(packet, indent=2), encoding="utf-8")
    print("F1_PRIVATE_LOCAL_REPLAY_PASS" if passed else "F1_PRIVATE_LOCAL_REPLAY_FAIL")
    raise SystemExit(0 if passed else 1)
