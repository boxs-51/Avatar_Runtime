# Generated-only native helper crash/timeout/flood/malformed/deployment qualification.
import argparse,hashlib,json,os,shutil,subprocess,tempfile,uuid
from pathlib import Path
def main():
    p=argparse.ArgumentParser();p.add_argument('--bin',type=Path,required=True);p.add_argument('--asset',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);a=p.parse_args();binary=a.bin.resolve();asset=a.asset.resolve();root=a.evidence.resolve()/uuid.uuid4().hex;root.mkdir(parents=True)
    repo=Path(__file__).resolve().parents[1]
    names=('CMakeLists.txt','src/assets/isolated_cpu_load_windows.cpp','src/assets/isolated_cpu_load.hpp','src/moc3/import_sandbox.cpp','tests/assets/cpu_fault_worker.cpp','tests/assets/isolated_cpu_load_tests.cpp','scripts/verify_cpu_helper_faults.py')
    def hashes():return {name:hashlib.sha256((repo/name).read_bytes()).hexdigest() for name in names}
    report={'source_before':hashes(),'result':'RUNNING','cases':[],'asset_sha256':hashlib.sha256(asset.read_bytes()).hexdigest()}
    def save():(root/'evidence.json').write_text(json.dumps(report,indent=2))
    try:
        for case,fault,expected in [('missing',None,'UNSUPPORTED'),('hang',1,'OUT_OF_BUDGET'),('crash',2,'INTERNAL'),('flood',3,'OUT_OF_BUDGET'),('malformed',4,'BAD_ASSET'),('readonly',0,'OK'),('hostile-cwd-path',0,'OK')]:
            folder=root/case;folder.mkdir();shutil.copyfile(binary/'ars_isolated_cpu_load_tests.exe',folder/'probe.exe');shutil.copyfile(binary/'ars_cpu_model_broker.exe',folder/'ars_cpu_model_broker.exe')
            worker=folder/'ars_cpu_admission_worker.exe'
            if fault is not None:shutil.copyfile(binary/(f'ars_cpu_fault_{fault}.exe' if fault else 'ars_cpu_admission_worker.exe'),worker)
            if case=='readonly':worker.chmod(0o444)
            cwd=folder/'untrusted-cwd';cwd.mkdir();(cwd/'ars_cpu_admission_worker.exe').write_bytes(b'hostile executable');(cwd/'ars_cpu_model_broker.exe').write_bytes(b'hostile executable')
            env=dict(os.environ);env['PATH']=str(cwd)+os.pathsep+env.get('PATH','');env['ARS_CPU_WORKER']=str(cwd/'ars_cpu_admission_worker.exe')
            before={str(p) for p in Path(tempfile.gettempdir()).glob('ars-cpu-*')}
            result=subprocess.run([str(folder/'probe.exe'),str(asset),expected],cwd=cwd,env=env,capture_output=True,timeout=25)
            (folder/'output.log').write_bytes(result.stdout+result.stderr)
            assert result.returncode==0,(case,result.returncode,result.stdout,result.stderr)
            after={str(p) for p in Path(tempfile.gettempdir()).glob('ars-cpu-*')};assert after<=before,(case,'owned temp remains',after-before)
            report['cases'].append({'case':case,'result':'PASS','expected':expected,'exit':result.returncode,'temporary_folders_retired':True,'binaries':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.glob('*.exe')}});save();print('PASS helper '+case,flush=True)
        report['source_after']=hashes();assert report['source_before']==report['source_after'],'source changed';report['result']='PASS'
    finally:save()
    print('CPU_HELPER_FAULTS_PASS',root,flush=True)
if __name__=='__main__':main()
