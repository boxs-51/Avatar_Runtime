# Real relocated installed public CPU SDK x static/shared x Release/Debug.
import argparse,hashlib,json,os,shutil,subprocess,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--evidence',type=Path,required=True);parser.add_argument('--corpus',type=Path,required=True);parser.add_argument('--static-build',type=Path,default=ROOT/'build/alpha60-public-cpu');parser.add_argument('--shared-build',type=Path,default=ROOT/'build/alpha60-public-cpu-shared');args=parser.parse_args()
    root=args.evidence.resolve()/uuid.uuid4().hex;root.mkdir(parents=True);index=0
    sources=[ROOT/'CMakeLists.txt',ROOT/'tests/abi/cpu_profile_consumer.c',Path(__file__),ROOT/'docs/in-progress/ALPHA_60_PUBLIC_CPU_COMPLETION_DESIGN.md',ROOT/'docs/in-progress/ALPHA_59_PUBLIC_ABI_CONTRACT.md',ROOT/'docs/reference/SDK_DEVELOPMENT_PACKAGE.md']+list((ROOT/'cmake').glob('*.cmake*'))
    for folder in ('core','motion','security','abi','assets','validation'):sources+=list((ROOT/'src'/folder).glob('*'))
    def hashes():return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sources if p.is_file()}
    receipt={'result':'RUNNING','source_before':hashes(),'cases':[],'root':str(root)}
    def save():(root/'evidence.json').write_text(json.dumps(receipt,indent=2))
    def run(command,*,cwd=None,env=None):
        nonlocal index
        index+=1;log=root/f'{index:03d}.log'
        with log.open('wb') as output:r=subprocess.run(list(map(str,command)),cwd=cwd or ROOT,env=env,stdout=output,stderr=subprocess.STDOUT,timeout=600)
        assert r.returncode==0,('command',str(log),r.returncode);return log
    try:
        for shared,build in ((False,args.static_build.resolve()),(True,args.shared_build.resolve())):
            run(['cmake','-S',ROOT,'-B',build,'-G','Visual Studio 18 2026','-A','x64','-DARS_ENABLE_PUBLIC_CPU_ASSETS=ON','-DARS_BUILD_RUNTIME_CORE=ON','-DARS_BUILD_ASSET_VALIDATOR=ON','-DARS_INSTALL_RUNTIME_CORE_SDK=ON','-DARS_ENABLE_PREP_TESTS=OFF','-DARS_ASSET_FETCH_DEPS=OFF','-DARS_ASSET_DEP_CACHE='+str(ROOT/'build/c-deps'),'-DBUILD_SHARED_LIBS='+('ON' if shared else 'OFF')])
            for config in ('Release','Debug'):
                name=('shared-' if shared else 'static-')+config.lower();case=root/name;case.mkdir()
                run(['cmake','--build',build,'--config',config,'--target','ars_runtime_core','ars_cpu_admission_worker','ars_cpu_model_broker','--parallel','4'])
                if shared:run(['ctest','--test-dir',build,'-C',config,'-R','^b1_exact_exports$','--output-on-failure'])
                original=case/'original-sdk';installed=case/'relocated-sdk';run(['cmake','--install',build,'--config',config,'--prefix',original])
                assert original.resolve().is_relative_to(root) and not installed.exists();shutil.move(str(original),str(installed));assert not original.exists()
                for cmake in installed.rglob('*.cmake'):
                    text=cmake.read_text().replace('\\','/').lower();assert str(ROOT).replace('\\','/').lower() not in text and str(build).replace('\\','/').lower() not in text,('build tree leak',cmake)
                assert (installed/'include/ars/ars.h').read_bytes()==(ROOT/'include/ars/ars.h').read_bytes()
                src=case/'consumer-src';src.mkdir();shutil.copyfile(ROOT/'tests/abi/cpu_profile_consumer.c',src/'consumer.c');shutil.copyfile(src/'consumer.c',src/'consumer.cpp')
                (src/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.22)
project(PublicCpuConsumer LANGUAGES C CXX)
find_package(AvatarRuntime 0.1 CONFIG REQUIRED COMPONENTS RuntimeCore)
if(NOT AvatarRuntime_PROFILE STREQUAL "ISOLATED_PUBLIC_CPU_ASSETS")
 message(FATAL_ERROR "Wrong installed profile")
endif()
add_executable(consumer_c consumer.c)
set_target_properties(consumer_c PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES LINKER_LANGUAGE CXX)
add_executable(consumer_cpp consumer.cpp)
set_target_properties(consumer_cpp PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES)
foreach(target IN ITEMS consumer_c consumer_cpp)
 target_link_libraries(${target} PRIVATE ARS::RuntimeCore)
 ars_copy_cpu_helpers(${target})
 target_compile_options(${target} PRIVATE /W4 /WX)
endforeach()
''')
                consumer=case/'consumer-build';run(['cmake','-S',src,'-B',consumer,'-G','Visual Studio 18 2026','-A','x64','-DCMAKE_PREFIX_PATH='+str(installed)]);run(['cmake','--build',consumer,'--config',config,'--parallel','4'])
                deployed=case/'deployed';shutil.copytree(consumer/config,deployed);hostile=case/'hostile-cwd';hostile.mkdir()
                (hostile/'ars_cpu_admission_worker.exe').write_bytes(b'hostile not a PE executable');(hostile/'ars_cpu_model_broker.exe').write_bytes(b'hostile not a PE executable')
                env=dict(os.environ);env['PATH']=str(hostile)+os.pathsep+env.get('PATH','');env['ARS_CPU_WORKER']=str(hostile/'ars_cpu_admission_worker.exe')
                checks=[]
                for executable in ('consumer_c.exe','consumer_cpp.exe'):
                    for asset in ('positive-2d','positive-3d','positive-dual-full'):
                        log=run([deployed/executable,args.corpus.resolve()/(asset+'.arschar')],cwd=hostile,env=env);assert b'PASS public CPU' in log.read_bytes();checks.append([executable,asset])
                receipt['cases'].append({'case':name,'result':'PASS','checks':checks,'installed_sha256':{p.relative_to(installed).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in installed.rglob('*') if p.is_file()},'deployed_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in deployed.iterdir() if p.is_file()}});save();print('PASS installed relocated '+name,flush=True)
        receipt['source_after']=hashes();assert receipt['source_before']==receipt['source_after'],'source changed';receipt['result']='PASS'
    finally:save()
    print('PUBLIC_CPU_SDK_MATRIX_PASS',root,flush=True)
if __name__=='__main__':main()
