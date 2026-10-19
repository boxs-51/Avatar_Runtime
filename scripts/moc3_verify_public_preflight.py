"""Actual Windows public-parser isolation probes; private payload/evidence stay ignored."""
import csv
from dataclasses import asdict
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import time
import uuid
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from moc3_public_preflight import _run,preflight_moc,BIN
from moc3_local_replay import _private_manifest
from moc3_package_admission import admit_package


def check(value,message):
    if not value:raise RuntimeError(message)


def latest_diagnostic():
    folder=max((ROOT/'build/moc3-import-trials').iterdir(),key=lambda p:p.stat().st_mtime)
    return json.loads((folder/'launcher-state.json').read_text(encoding='utf-8'))


def main():
    folder=ROOT/'build/f4-public-preflight-evidence'/uuid.uuid4().hex[:24];folder.mkdir(parents=True)
    packet={'kind':'F4_PUBLIC_PARSER_PREFLIGHT_ONLY','public_activation':False,'cases':[],
            'source_sha256':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
               ('CMakeLists.txt','src/moc3/import_worker.cpp','src/moc3/import_sandbox.cpp',
                'scripts/moc3_public_preflight.py','scripts/moc3_verify_public_preflight.py')},
            'binary_sha256':{name:hashlib.sha256((BIN/name).read_bytes()).hexdigest() for name in
               ('ars_moc3_import_worker.exe','ars_moc3_import_sandbox.exe')}}
    def case(name,mode,payload=b'',arguments=(),expected=None):
        started=time.monotonic();result=_run(mode,payload,arguments)
        check(result.status in expected and result.profile_removed and result.job_empty,name+' isolation verdict')
        diagnostic=latest_diagnostic()
        packet['cases'].append({'case':name,**asdict(result),'elapsed_seconds':time.monotonic()-started,'diagnostic':diagnostic})
        return diagnostic
    try:
        canary=folder/'owner-only.txt';canary.write_bytes(b'generated outside-worker sentinel')
        identity=subprocess.run(['whoami','/user','/fo','csv','/nh'],capture_output=True,text=True,check=True,timeout=5)
        sid=next(csv.reader(identity.stdout.splitlines()))[1];check(sid.startswith('S-1-'),'owner SID')
        subprocess.run(['icacls',str(canary),'/inheritance:r','/grant:r','*'+sid+':F'],capture_output=True,check=True,timeout=5)
        check(canary.read_bytes()==b'generated outside-worker sentinel','host can read canary')
        with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as listener:
            listener.bind(('127.0.0.1',0));listener.listen(1)
            listener.settimeout(1)
            with socket.create_connection(listener.getsockname(),timeout=1):
                connection,_=listener.accept();connection.close()
            old_canary=os.environ.get('ARS_MOC3_IMPORT_PROBE_ENV_CANARY')
            os.environ['ARS_MOC3_IMPORT_PROBE_ENV_CANARY']='generated-environment-isolation-sentinel'
            try:
                case('identity_file_network_child_readonly_environment','--probe',arguments=(canary,listener.getsockname()[1]),
                     expected={'ISOLATION_DENIED','ISOLATION_NO_CONNECTION_WINDOW'})
            finally:
                if old_canary is None:os.environ.pop('ARS_MOC3_IMPORT_PROBE_ENV_CANARY',None)
                else:os.environ['ARS_MOC3_IMPORT_PROBE_ENV_CANARY']=old_canary
            listener.settimeout(.05)
            try:connection,_=listener.accept()
            except TimeoutError:pass
            else:connection.close();raise RuntimeError('unexpected network connection')
            listener.settimeout(1)
            with socket.create_connection(listener.getsockname(),timeout=1):
                connection,_=listener.accept();connection.close()
            packet['loopback_controls_before_after']='PASS'
            packet['universal_network_attribution']='HOLD_NO_WFP_TRACE_IPV6_UDP_OR_REMOTE_PROBES'
        check(not Path(str(canary)+'.write').exists(),'outside write side effect')
        case('os_memory_limit','--memory',expected={'MEMORY_DENIED'})
        cpu=case('os_cpu_limit','--cpu',expected={'CPU_REJECTED','WORKER_REJECTED'})
        check(19_000_000<=cpu['worker_user_cpu_100ns']<=30_000_000,'actual CPU limit witness')
        case('wall_timeout','--hang',expected={'TIMEOUT_REJECTED'})
        case('crash_containment','--crash',expected={'WORKER_REJECTED'})
        case('bounded_pipe_flood','--flood',expected={'OUTPUT_REJECTED'})
        for i,payload in enumerate((b'',struct.pack('<II',3,1)+b'MOC',struct.pack('<II',17*1024*1024,1),
                                   struct.pack('<II',4,0)+b'MOC3',struct.pack('<II',4,1)+b'MOC3EXTRA')):
            case('invalid_envelope_'+str(i),'--moc',payload,expected={'INVALID_ENVELOPE'})
        # Generated hostile bytes; no private sample bytes/hashes are exported.
        for i in range(12):
            data=bytearray(b'MOC3'+bytes(124));data[4]=i;data[8:12]=bytes([255-i])*4
            case('generated_mutation_'+str(i),'--moc',struct.pack('<II',len(data),1)+data,
                 expected={'MODEL_REJECTED','WORKER_REJECTED','TIMEOUT_REJECTED'})
        manifest=_private_manifest();check(manifest is not None,'private fixture available')
        package=admit_package(manifest.parent,manifest.name)
        result=preflight_moc(package.moc_bytes,len(package.textures))
        check(result.status=='PREFLIGHT_OK' and result.profile_removed and result.job_empty,'actual isolated model preflight')
        packet['cases'].append({'case':'authorized_private_positive',**asdict(result)})
        packet['verdict']='PASS'
    except (OSError,ValueError,KeyError,RuntimeError,subprocess.SubprocessError) as error:
        packet.update(verdict='FAIL',reason=type(error).__name__)
        (folder/'failure.txt').write_text(str(error),encoding='utf-8')
    (folder/'evidence.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
    print('PUBLIC_PARSER_ISOLATION_'+packet['verdict']);print('EVIDENCE_RUN '+folder.name)
    return 0 if packet['verdict']=='PASS' else 1


if __name__=='__main__':raise SystemExit(main())
