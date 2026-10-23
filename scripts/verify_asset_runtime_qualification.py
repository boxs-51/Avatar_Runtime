"""#41 installed public ABI qualification. A Windows slice never closes G01/G02."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
STATUS = {"OK": 0, "UNSUPPORTED": 2, "BAD_ASSET": 3, "OUT_OF_BUDGET": 4}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def corpus_rows(corpus):
    """Use the frozen independent oracle, never the implementation's verdicts."""
    records = json.loads((ROOT / "tests/assets/fixtures/manifest.json").read_text(encoding="utf-8"))
    rows = []
    for record in records:
        name = record["file"]
        if not name.endswith(".arschar"):
            continue  # Public MEMORY_BUNDLE imports bundles, not naked GLB/PNG files.
        require(re.fullmatch(r"[A-Za-z0-9_-]+\.arschar", name), "UNSAFE_CORPUS_NAME")
        require(record["expected"] in STATUS, "UNKNOWN_CORPUS_ORACLE")
        data = (corpus / name).read_bytes()
        require(len(data) == record["bytes"] and hashlib.sha256(data).hexdigest() == record["sha256"], "CORPUS_DRIFT")
        rows.append({"file": name, "expected": STATUS[record["expected"]], "family": record["family"]})
    require(rows and len({r["file"] for r in rows}) == len(rows), "EMPTY_OR_DUPLICATE_CORPUS")
    return rows


def inspect_cases(text, rows):
    require(bool(rows), "EMPTY_PUBLIC_CASES")
    found = re.findall(r"^CASE (\S+) (\d+) PASS$", text, re.MULTILINE)
    actual = [(name, int(status)) for name, status in found]
    expected = [(r["file"], r["expected"]) for r in rows]
    require(actual == expected, "MISSING_DUPLICATE_OR_WRONG_PUBLIC_CASE")
    summary = f"ASSET_QUALIFICATION_PASS cases={len(rows)} live=0"
    require(text.endswith(summary + "\n") and text.splitlines().count(summary) == 1,
            "MISSING_FINAL_RESOURCE_ORACLE")
    return [{**row, "actual": status, "result": "PASS"} for row, (_, status) in zip(rows, actual)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--dependency-cache", type=Path, required=True)
    args = parser.parse_args(argv)
    require(sys.platform == "win32", "WINDOWS_RUNNER_REQUIRED_LINUX_GATE_REMAINS_OPEN")
    root = args.evidence.resolve() / uuid.uuid4().hex
    root.mkdir(parents=True)
    sources = [ROOT / "CMakeLists.txt", Path(__file__), ROOT / "tests/assets/fixtures/manifest.json",
               ROOT / "tests/assets/fixtures/generate_fixtures.py", ROOT / "tests/rendering/public_asset_upload_probe.cpp"]
    for folder in ("src", "include", "cmake", "tests/abi"):
        sources += [p for p in (ROOT / folder).rglob("*") if p.is_file()]
    def hashes():
        return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    receipt = {"scope": "A41_WINDOWS_INSTALLED_PUBLIC_ABI", "result": "RUNNING", "source_before": hashes(),
               "gates": {"G01": "HOLD", "G02": "HOLD"}, "legs": [], "commands": [],
               "open_requirements": ["Linux GCC/Clang public activation unsupported and unrun",
                    "Direct zero-GPU-upload-on-reject attribution", "Full-duration fuzz closure reconciliation",
                    "Independent ABI/security review and gate disposition"]}
    def save():
        (root / "evidence.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    def run(command, cwd=ROOT, env=None):
        log = root / f"{len(receipt['commands']):03d}.log"
        print("RUN", len(receipt["commands"]), str(command[0]), flush=True)
        with log.open("wb") as output:
            result = subprocess.run(list(map(str, command)), cwd=cwd, env=env, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=900)
        receipt["commands"].append({"command": list(map(str, command)), "exit": result.returncode,
             "log": log.name, "sha256": hashlib.sha256(log.read_bytes()).hexdigest()})
        save()
        require(result.returncode == 0, f"COMMAND_FAILED:{log.name}:{result.returncode}")
        return log.read_text(encoding="utf-8", errors="replace")
    try:
        corpus = root / "corpus"
        run([sys.executable, ROOT / "tests/assets/fixtures/generate_fixtures.py", corpus,
             ROOT / "tests/assets/fixtures/manifest.json", "--reference-schema"])
        rows = corpus_rows(corpus)
        cases = root / "cases.tsv"
        cases.write_text("".join(f"{r['expected']}\t{r['file']}\n" for r in rows), encoding="ascii")
        for shared in (False, True):
            build = args.build_root.resolve() / ("d" if shared else "s")
            run(["cmake", "-S", ROOT, "-B", build, "-G", "Visual Studio 18 2026", "-A", "x64",
                 "-DARS_BUILD_RUNTIME_CORE=ON", "-DARS_BUILD_ASSET_VALIDATOR=ON",
                 "-DARS_ENABLE_PUBLIC_CPU_ASSETS=ON", "-DARS_ENABLE_PUBLIC_D3D11_HOST=ON",
                 "-DARS_INSTALL_RUNTIME_CORE_SDK=ON", "-DARS_ENABLE_PREP_TESTS=OFF",
                 "-DARS_ASSET_FETCH_DEPS=OFF", "-DARS_ASSET_DEP_CACHE=" + str(args.dependency_cache.resolve()),
                 "-DBUILD_SHARED_LIBS=" + ("ON" if shared else "OFF")])
            for config in ("Release", "Debug"):
                case = root / (("shared-" if shared else "static-") + config.lower())
                case.mkdir()
                run(["cmake", "--build", build, "--config", config, "--target", "ars_runtime_core",
                     "ars_cpu_admission_worker", "ars_cpu_model_broker", "ars_asset_qualification_upload_probe", "--parallel", "4"])
                if shared:
                    run(["ctest", "--test-dir", build, "-C", config, "-R", "^b1_exact_exports$",
                         "--no-tests=error", "--output-on-failure"])
                original, installed = case / "original-sdk", case / "relocated-sdk"
                run(["cmake", "--install", build, "--config", config, "--prefix", original])
                shutil.move(str(original), str(installed))
                require(not original.exists(), "SDK_RELOCATION_FAILED")
                require((installed / "include/ars/ars.h").read_bytes() == (ROOT / "include/ars/ars.h").read_bytes(), "ABI_HEADER_DRIFT")
                for cmake in installed.rglob("*.cmake"):
                    text = cmake.read_text().replace("\\", "/").lower()
                    require(all(str(p).replace("\\", "/").lower() not in text for p in (ROOT, build)), "INSTALLED_BUILD_PATH_LEAK")
                source = case / "consumer-src"
                source.mkdir()
                for target, name in (("qualification", "asset_qualification_consumer.c"),
                                     ("lifecycle", "cpu_profile_consumer.c"), ("gpu", "public_gpu_consumer.c")):
                    shutil.copyfile(ROOT / "tests/abi" / name, source / (target + ".c"))
                (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.22)
project(AssetQualification LANGUAGES C CXX)
find_package(AvatarRuntime 0.1 CONFIG REQUIRED COMPONENTS RuntimeCore)
if(NOT AvatarRuntime_PROFILE STREQUAL "OPT_IN_PUBLIC_D3D11_HOST")
 message(FATAL_ERROR "Wrong installed profile")
endif()
foreach(target IN ITEMS qualification lifecycle gpu)
 add_executable(${target} ${target}.c)
 set_target_properties(${target} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES LINKER_LANGUAGE CXX)
 target_link_libraries(${target} PRIVATE ARS::RuntimeCore d3d11 dxgi)
 target_compile_definitions(${target} PRIVATE ARS_EXPECT_PUBLIC_D3D11=1)
 target_compile_options(${target} PRIVATE /W4 /WX)
 ars_copy_cpu_helpers(${target})
endforeach()
''', encoding="ascii")
                consumer = case / "consumer-build"
                run(["cmake", "-S", source, "-B", consumer, "-G", "Visual Studio 18 2026", "-A", "x64", "-DCMAKE_PREFIX_PATH=" + str(installed)])
                run(["cmake", "--build", consumer, "--config", config, "--parallel", "4"])
                deployed = case / "deployed"
                shutil.copytree(consumer / config, deployed)
                hostile = case / "hostile-cwd"
                hostile.mkdir()
                for name in ("ars_cpu_admission_worker.exe", "ars_cpu_model_broker.exe"):
                    (hostile / name).write_bytes(b"not an executable")
                env = dict(os.environ, ARS_CPU_WORKER=str(hostile / "ars_cpu_admission_worker.exe"))
                env["PATH"] = str(hostile) + os.pathsep + env.get("PATH", "")
                details = inspect_cases(run([deployed / "qualification.exe", corpus, cases], cwd=hostile, env=env), rows)
                require(corpus_rows(corpus) == rows, "CORPUS_CHANGED_DURING_PUBLIC_RUN")
                for valid in ("positive-2d", "positive-3d", "positive-dual-full"):
                    output = run([deployed / "lifecycle.exe", corpus / (valid + ".arschar")], cwd=hostile, env=env)
                    require("PASS public CPU" in output, "NO_LIFECYCLE_ORACLE")
                output = run([deployed / "gpu.exe", corpus / "positive-dual-full.arschar"], cwd=hostile, env=env)
                require("PASS installed C11 GPU" in output, "NO_GPU_ORACLE")
                native = build / "bin" / config / "ars_asset_qualification_upload_probe.exe"
                native_details = inspect_cases(run([native, corpus, cases], cwd=hostile, env=env), rows)
                require(corpus_rows(corpus) == rows, "CORPUS_CHANGED_DURING_NATIVE_RUN")
                receipt["legs"].append({"case": case.name, "result": "PASS", "cases": details,
                     "native_upload_probe": {"result": "PASS", "cases": native_details,
                                              "binary_sha256": hashlib.sha256(native.read_bytes()).hexdigest()},
                     "installed_sha256": {p.relative_to(installed).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in installed.rglob("*") if p.is_file()},
                     "deployed_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in deployed.iterdir() if p.is_file()}})
                save()
                print("PASS", case.name, "public corpus cases", len(rows), flush=True)
        receipt["source_after"] = hashes()
        require(receipt["source_before"] == receipt["source_after"], "SOURCE_CHANGED")
        receipt["open_requirements"].remove("Direct zero-GPU-upload-on-reject attribution")
        receipt["proof_limits"] = ["Upload observation is the existing production-renderer creation seam, with positive control; not driver-wide telemetry",
                                   "Corpus hashes checked before and after runs; no concurrent corpus-tampering experiment"]
        receipt["result"] = "WINDOWS_SLICE_PASS"
    except BaseException as exc:
        receipt["result"] = "FAIL"
        receipt["error"] = {"class": type(exc).__name__, "message": str(exc)}
        raise
    finally:
        save()
    print("A41_WINDOWS_SLICE_PASS", root, flush=True)


if __name__ == "__main__":
    main()
