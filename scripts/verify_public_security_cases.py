"""Local #42 public ABI observations; never promote the complete87 security gate."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import uuid
import zipfile

ROOT=Path(__file__).resolve().parents[1]
PUBLIC_CASES=("HDL-001","HDL-002","HDL-003","HDL-004","HDL-012","HDL-014","SRC-017","SRC-019")
NATIVE_CASES=("SRC-001","SRC-005","SRC-008","SRC-009","SRC-010","SRC-011","SRC-013","SRC-016","SRC-018","SRC-021","SRC-027","SRC-028","SRC-029","SRC-039","SRC-045")
CASES=PUBLIC_CASES+NATIVE_CASES
INVENTORY=Path("docs/reference/G00_SECURITY_87_CASE_INVENTORY.json")
AUTHORITY_SHA="d7e6061db9e6d42c33d9fd0b738e192b5f45c992d9098e7e767726ac741cf1f1"

def generate_quaternion_fixture(base,output):
    oracle=next(r for r in json.loads((ROOT/'tests/assets/fixtures/manifest.json').read_text()) if r['file']=='positive-dual-full.arschar')
    require(digest(base)==oracle['sha256'] and base.stat().st_size==oracle['bytes'],"BASE_FIXTURE_DRIFT")
    with zipfile.ZipFile(base) as archive:entries={n:archive.read(n) for n in archive.namelist()}
    document=json.loads(entries['character.json'])
    document['rig_semantics'].append({'id':'extra.quat','type':'quat','default':[0,0,0,1]})
    entries['character.json']=json.dumps(document,separators=(',',':'),allow_nan=False).encode()
    data=io.BytesIO()
    with zipfile.ZipFile(data,'w') as archive:
        for name in sorted(entries):
            info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.create_system=3;info.external_attr=0o100644<<16
            archive.writestr(info,entries[name])
    require(hashlib.sha256(data.getvalue()).hexdigest()==AUTHORITY_SHA,"DERIVED_FIXTURE_DRIFT")
    output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(data.getvalue())

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
        run(["cmake","--build",build,"--config","Release","--target","ars_public_security_cases","ars_public_authority_cases","--parallel","4"])
        binary=build/"bin/Release/ars_public_security_cases.exe"
        native=build/"bin/Release/ars_public_authority_cases.exe"
        binaries=[binary,native]+[build/"bin/Release"/n for n in ('ars_cpu_admission_worker.exe','ars_cpu_model_broker.exe')]
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
            expected_binary=native if case in NATIVE_CASES else binary
            require(Path(command[0]).resolve()==expected_binary.resolve() and command[1]==case and len(command)==3,"WRONG_CONFIGURED_BINARY")
            corpus=Path(command[2]);require(corpus.is_file(),"MISSING_CORPUS")
            expected=({'file':'authority-quaternion.arschar','bytes':5100,'sha256':AUTHORITY_SHA} if case in NATIVE_CASES else
                next(r for r in json.loads((ROOT/'tests/assets/fixtures/manifest.json').read_text()) if r['file']==corpus.name))
            require(corpus.name==expected['file'],"WRONG_FIXTURE")
            require(digest(corpus)==expected['sha256'] and corpus.stat().st_size==expected['bytes'],"FIXTURE_DRIFT")
            output=run(command);inspect_case(output,case)
            require({p.name:digest(p) for p in binaries}==receipt['binaries_before'],"BINARY_CHANGED_DURING_RUN")
            require(digest(corpus)==expected['sha256'],"FIXTURE_CHANGED_DURING_RUN")
            receipt.setdefault('fixtures',{})[corpus.name]=expected['sha256']
            rows[case]['local_windows']={"result":"PASS","command_index":len(receipt['commands'])-1,
                "execution_kind":"PUBLIC_ABI_NATIVE_OBSERVER" if case in NATIVE_CASES else "PUBLIC_C_ABI_LINKED",
                "profile":"Windows-local/MSVC/Release/static/public-CPU-opt-in/D3D11-OFF/no-sanitizer"}
            save()
        receipt['binary_sha256']=digest(binary);receipt['fixture_sha256']=digest(corpus)
        receipt['native_binary_sha256']=digest(native)
        receipt['binaries_after']={p.name:digest(p) for p in binaries}
        require(receipt['binaries_before']==receipt['binaries_after'],"BINARY_CHANGED_AFTER_RUN")
        receipt['source_after']=hashes();require(receipt['source_before']==receipt['source_after'],"SOURCE_CHANGED")
        receipt['result']='PARTIAL_LOCAL_PASS'
        receipt['open_requirements']=[f'{87-len(CASES)} unexecuted cases','Required windows-2022 and Ubuntu GCC/Clang environments',
            'Separate required sanitizer builds','Full deterministic race/GPU/remote adapter proof','Unmerged issue41 dependency/platform gaps']
        receipt['proof_limits']=['Native observer cases use existing friend seams for snapshots/queue/credit/epoch and deterministic injected security clock',
            'Append failure is injected; not physical OS clock or host allocation failure proof',
            'Eight original cases retain header-only linked ABI scope; native observer is a separate test executable, no shipping export']
    except BaseException as error:
        receipt['result']='FAIL';receipt['error']={"class":type(error).__name__,"message":str(error)};raise
    finally:save()
    print('A42_PARTIAL_LOCAL_PASS',out,flush=True)

if __name__=='__main__':
    if len(sys.argv)==4 and sys.argv[1]=='--generate-quaternion':generate_quaternion_fixture(Path(sys.argv[2]),Path(sys.argv[3]))
    else:main()
