"""Source-build and verify the opt-in installed feature-off ABI20 development package."""
import argparse,hashlib,json,os,shutil,subprocess,sys,time,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]


def verify(root,configs):
    root=root.resolve();root.mkdir(parents=True,exist_ok=False);receipt={'started':time.time(),'result':'FAIL','cases':[]}
    source_names=('CMakeLists.txt','cmake/ars_runtime_sdk.cmake','cmake/AvatarRuntimeConfig.cmake.in','include/ars/ars.h','tests/sdk/CMakeLists.txt','tests/sdk/consumer.c','tests/sdk/consumer.cpp','docs/reference/SDK_DEVELOPMENT_PACKAGE.md','docs/in-progress/ALPHA_59_PUBLIC_ABI_CONTRACT.md','scripts/verify_installed_sdk.py')
    paths=[ROOT/name for name in source_names]+sorted((ROOT/'src/abi').glob('*'))+sorted((ROOT/'src/core').glob('*'))+sorted((ROOT/'src/security').glob('*'))+sorted((ROOT/'src/motion').glob('*'))
    def closure():return {p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.is_file()}
    receipt['source_before']=closure()
    index=0
    def run(args,*,success=True):
        nonlocal index
        index+=1;log=root/f'{index:03d}.log'
        with log.open('wb') as output:
            result=subprocess.run(list(map(str,args)),cwd=root,stdout=output,stderr=subprocess.STDOUT,timeout=240)
        if (result.returncode==0)!=success:raise RuntimeError('COMMAND_OUTCOME_'+log.name)
        return log
    try:
        for shared in (False,True):
            build=root/('shared-build' if shared else 'static-build')
            for config in configs:
                name=('shared-' if shared else 'static-')+config.lower();case=root/name;case.mkdir()
                prefix=case/'original-sdk';relocated=case/'relocated-sdk'
                run(['cmake','-S',ROOT,'-B',build,'-DARS_BUILD_RUNTIME_CORE=ON','-DARS_INSTALL_RUNTIME_CORE_SDK=ON','-DBUILD_SHARED_LIBS='+('ON' if shared else 'OFF'),'-DCMAKE_BUILD_TYPE='+config])
                run(['cmake','--build',build,'--config',config,'--target','ars_runtime_core'])
                if shared:run(['ctest','--test-dir',build,'-C',config,'-R','^b1_exact_exports$','--output-on-failure'])
                run(['cmake','--install',build,'--config',config,'--prefix',prefix])
                assert prefix.resolve().is_relative_to(root) and relocated.parent.resolve().is_relative_to(root) and prefix.resolve()!=root and not relocated.exists()
                shutil.move(str(prefix),str(relocated));assert not prefix.exists()
                files=sorted(p for p in relocated.rglob('*') if p.is_file());names=[p.relative_to(relocated).as_posix() for p in files]
                assert (relocated/'include/ars/ars.h').read_bytes()==(ROOT/'include/ars/ars.h').read_bytes()
                assert all(n.startswith(('include/ars/','lib/','bin/','share/AvatarRuntime/')) for n in names)
                assert {n for n in names if n.startswith('include/')}=={'include/ars/ars.h'}
                assert all('foundation' not in n and 'stub' not in n and 'moc3' not in n for n in names)
                for file in relocated.rglob('*.cmake'):
                    text=file.read_text().replace('\\','/').lower()
                    assert str(ROOT).replace('\\','/').lower() not in text and str(build).replace('\\','/').lower() not in text,'BUILD_TREE_LEAK'
                source=case/'consumer-src';shutil.copytree(ROOT/'tests/sdk',source)
                consumer=case/'consumer-build'
                run(['cmake','-S',source,'-B',consumer,'-DCMAKE_PREFIX_PATH='+str(relocated),'-DCMAKE_BUILD_TYPE='+config])
                run(['cmake','--build',consumer,'--config',config])
                run(['ctest','--test-dir',consumer,'-C',config,'--output-on-failure'])
                # The installed header must be used: poisoning a copied SDK
                # cannot be masked by a checkout/build-tree include fallback.
                bad=case/'bad-sdk';shutil.copytree(relocated,bad)
                header=bad/'include/ars/ars.h';header.write_text('#error INSTALLED_HEADER_SENTINEL\n'+header.read_text())
                bad_consumer=case/'bad-consumer'
                run(['cmake','-S',source,'-B',bad_consumer,'-DCMAKE_PREFIX_PATH='+str(bad),'-DCMAKE_BUILD_TYPE='+config])
                sentinel=run(['cmake','--build',bad_consumer,'--config',config],success=False)
                assert 'INSTALLED_HEADER_SENTINEL' in sentinel.read_text(errors='replace')
                receipt['cases'].append({'case':name,'result':'PASS','installed_files':names,'installed_sha256':{p.relative_to(relocated).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files}})
        # Package selection and opt-in guards are real configure failures.
        for name,opts in [('no-core',['-DARS_INSTALL_RUNTIME_CORE_SDK=ON']),('sanitizer',['-DARS_BUILD_RUNTIME_CORE=ON','-DARS_INSTALL_RUNTIME_CORE_SDK=ON','-DARS_RUNTIME_SANITIZER=ADDRESS'])]:
            run(['cmake','-S',ROOT,'-B',root/name,*opts],success=False)
        for name,request in [('version','find_package(AvatarRuntime 0.2 CONFIG REQUIRED)'),('component','find_package(AvatarRuntime 0.1 CONFIG REQUIRED COMPONENTS Unknown)'),('architecture','set(CMAKE_SIZEOF_VOID_P 4)\nfind_package(AvatarRuntime 0.1 CONFIG REQUIRED)'),('platform','set(CMAKE_SYSTEM_NAME UnsupportedPlatform)\nfind_package(AvatarRuntime 0.1 CONFIG REQUIRED)')]:
            source=root/(name+'-src');source.mkdir();(source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.22)\nproject(Negative LANGUAGES C CXX)\n'+request+'\n')
            run(['cmake','-S',source,'-B',root/(name+'-build'),'-DCMAKE_PREFIX_PATH='+str(relocated)],success=False)
        receipt['source_after']=closure();assert receipt['source_before']==receipt['source_after'],'SOURCE_CHANGED'
        receipt['result']='PASS'
    finally:
        receipt['completed']=time.time();(root/'evidence.json').write_text(json.dumps(receipt,indent=2))
    print('INSTALLED_ABI20_RELOCATED_STATIC_SHARED_PASS cases='+str(len(receipt['cases'])))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--work-root',type=Path);parser.add_argument('--configs',nargs='+',choices=('Release','Debug'),default=['Release','Debug']);args=parser.parse_args()
    verify(args.work_root or ROOT/'build/alpha59-installed'/uuid.uuid4().hex,args.configs)
