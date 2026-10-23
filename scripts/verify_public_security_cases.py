"""Local #42 public ABI observations; never promote the complete87 security gate."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[1]
CASES=("HDL-001","HDL-002","HDL-003","HDL-004","HDL-012","HDL-014","SRC-017","SRC-019")
INVENTORY=Path("docs/reference/G00_SECURITY_87_CASE_INVENTORY.json")

def require(condition,message):
    if not condition:
        raise RuntimeError(message)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def inspect_case(output,case):
    require(case in CASES,"UNKNOWN_CASE")
    marker=f"SECURITY_CASE {case} PASS seed=42 live=0"
    lines=output.splitlines()
    require(output.endswith(marker+"\n") and lines.count(marker)==1,"MISSING_FINAL_CASE_ORACLE")
    require([x for x in lines if x.startswith("SECURITY_CASE ")]==[marker],"DUPLICATE_OR_FOREIGN_CASE")
    require(any(x.startswith("TRACE ") for x in lines),"NO_SCENARIO_TRACE")

def inventory_rows(data):
    rows=data["cases"]
    expected={f"{prefix}-{i:03d}" for prefix,n in (("HDL",19),("SRC",45)) for i in range(1,n+1)}
    expected.update('RACE-'+chr(i) for i in range(ord('A'),ord('W')+1))
    require(data["count"]==len(rows)==87 and {r["id"] for r in rows}==expected,"INVENTORY_COVERAGE_DRIFT")
    return [{**r,"local_windows":{"result":"NOT_RUN"},
             "required_platform_results":{p:"NOT_RUN" for p in r["required_platforms"]},
             "sanitizers":"NOT_RUN","qualification":"HOLD"} for r in rows]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build",type=Path,required=True)
    parser.add_argument("--evidence",type=Path,required=True)
    parser.add_argument("--dependency-cache",type=Path,required=True)
    args=parser.parse_args()
    require(sys.platform=="win32","WINDOWS_RUNNER_REQUIRED")
    require(not os.environ.get("GITHUB_ACTIONS"),"LOCAL_QUALIFICATION_ONLY")
    out=args.evidence.resolve()/uuid.uuid4().hex;out.mkdir(parents=True)
    build=args.build.resolve()
    files={Path("CMakeLists.txt"),INVENTORY,Path(__file__).relative_to(ROOT),Path("tests/security/public_security_cases.cpp")}
    for directory in ("src","include","cmake","tests/assets/fixtures","tests/security"):
        files.update(p.relative_to(ROOT) for p in (ROOT/directory).rglob('*') if p.is_file())
    files={p for p in files if "__pycache__" not in p.parts}
    def hashes():return {p.as_posix():digest(ROOT/p) for p in sorted(files)}
    receipt={"scope":"A42_WINDOWS_PUBLIC_CPU_PARTIAL","result":"RUNNING","gates":{"G07":"HOLD","G08":"HOLD"},
             "base":"d75fd268cea9880a5224403e69c010357fbb3580","seed":42,
             "host":{"platform":platform.platform(),"machine":platform.machine(),"python":sys.version},
             "source_before":hashes(),"commands":[],
             "cases":[]}
    def save():(out/"evidence.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
    def run(command):
        log=out/f"{len(receipt['commands']):03d}.log"
        print("RUN",len(receipt['commands']),str(command[0]),flush=True)
        with log.open("wb") as stream:
            process=subprocess.run([str(x) for x in command],cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,timeout=900)
        receipt['commands'].append({"command":[str(x) for x in command],"exit":process.returncode,"log":log.name,"sha256":digest(log)})
        save();require(process.returncode==0,"COMMAND_FAILED:"+log.name)
        return log.read_text(encoding="utf-8",errors="replace")
    try:
        receipt['cases']=inventory_rows(json.loads((ROOT/INVENTORY).read_text(encoding="utf-8")))
        run(["cmake","-S",ROOT,"-B",build,"-G","Visual Studio 18 2026","-A","x64",
             "-DARS_BUILD_RUNTIME_CORE=ON","-DARS_BUILD_ASSET_VALIDATOR=ON","-DARS_ENABLE_PUBLIC_CPU_ASSETS=ON",
             "-DARS_ENABLE_PUBLIC_D3D11_HOST=OFF","-DARS_INSTALL_RUNTIME_CORE_SDK=OFF","-DARS_ENABLE_PREP_TESTS=OFF",
             "-DBUILD_SHARED_LIBS=OFF","-DARS_ASSET_FETCH_DEPS=OFF","-DARS_ASSET_DEP_CACHE="+str(args.dependency_cache.resolve())])
        run(["cmake","--build",build,"--config","Release","--target","ars_public_security_cases","--parallel","4"])
        binary=build/"bin/Release/ars_public_security_cases.exe"
        binaries=[binary]+[build/"bin/Release"/n for n in ('ars_cpu_admission_worker.exe','ars_cpu_model_broker.exe')]
        receipt['binaries_before']={p.name:digest(p) for p in binaries}
        receipt['compiler_metadata']={p.relative_to(build).as_posix():p.read_text(errors='replace') for p in (build/'CMakeFiles').glob('*/CMakeCXXCompiler.cmake')}
        receipt['cmake_cache_sha256']=digest(build/'CMakeCache.txt')
        # Resolve the registered CTest argument, rather than guessing the fixture directory.
        tests=json.loads(run(["ctest","--test-dir",build,"-C","Release","--show-only=json-v1"]))['tests']
        selected={t['name']:t for t in tests if t['name'].startswith('alpha42_')}
        require(set(selected)=={'alpha42_'+c for c in CASES},"MISSING_OR_EXTRA_CTEST_CASE")
        rows={r['id']:r for r in receipt['cases']}
        for case in CASES:
            command=selected['alpha42_'+case]['command']
            require(Path(command[0]).resolve()==binary.resolve() and command[1]==case and len(command)==3,"WRONG_CONFIGURED_BINARY")
            corpus=Path(command[2]);require(corpus.is_file(),"MISSING_CORPUS")
            expected=next(r for r in json.loads((ROOT/'tests/assets/fixtures/manifest.json').read_text()) if r['file']==corpus.name)
            require(digest(corpus)==expected['sha256'] and corpus.stat().st_size==expected['bytes'],"FIXTURE_DRIFT")
            output=run(command);inspect_case(output,case)
            require({p.name:digest(p) for p in binaries}==receipt['binaries_before'],"BINARY_CHANGED_DURING_RUN")
            require(digest(corpus)==expected['sha256'],"FIXTURE_CHANGED_DURING_RUN")
            rows[case]['local_windows']={"result":"PASS","command_index":len(receipt['commands'])-1,
                "profile":"Windows-local/MSVC/Release/static/public-CPU-opt-in/D3D11-OFF/no-sanitizer"}
            save()
        receipt['binary_sha256']=digest(binary);receipt['fixture_sha256']=digest(corpus)
        receipt['binaries_after']={p.name:digest(p) for p in binaries}
        require(receipt['binaries_before']==receipt['binaries_after'],"BINARY_CHANGED_AFTER_RUN")
        receipt['source_after']=hashes();require(receipt['source_before']==receipt['source_after'],"SOURCE_CHANGED")
        receipt['result']='PARTIAL_LOCAL_PASS'
        receipt['open_requirements']=['79 unexecuted cases','Required windows-2022 and Ubuntu GCC/Clang environments',
            'Separate required sanitizer builds','Full deterministic race/GPU/remote adapter proof','Unmerged issue41 dependency/platform gaps']
    except BaseException as error:
        receipt['result']='FAIL';receipt['error']={"class":type(error).__name__,"message":str(error)};raise
    finally:save()
    print('A42_PARTIAL_LOCAL_PASS',out,flush=True)

if __name__=='__main__':main()
