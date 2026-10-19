"""Default-off static package diagnostic. Opaque ZIP transport; no public asset parsing here.

The native host must independently validate the returned hostile packet on every use.
Physics is admitted, NOT simulated. This is not the public SDK entry point.
"""
from dataclasses import dataclass
from pathlib import Path
import ctypes
import hashlib
from ctypes import wintypes
import json
import math
import secrets
import shutil
import struct
import subprocess
import uuid
from moc3_public_preflight import profile_absent

ROOT=Path(__file__).resolve().parents[1]
ISOLATED_BIN=ROOT/'build/moc3-public-isolation/bin/Release'
HOST_BIN=ROOT/'build/m2-d-private/bin/Release'
PACKET_CAP=192*1024*1024
REJECTIONS={*(('PACKAGE_'+stage+'_REJECTED').encode('ascii') for stage in ('COMMAND','BUDGET','ENVELOPE','ZIP','MANIFEST','PNG','MOC','PHYSICS','OUTPUT')),b'OUTPUT_REJECTED',b'CPU_REJECTED',b'TIMEOUT_REJECTED',b'WORKER_REJECTED'}

@dataclass(frozen=True)
class Admission:
    status:str
    packet:bytes
    nonce:int
    profile_removed:bool
    job_empty:bool
    evidence:Path


def _package_envelope(package:bytes,manifest_entry:str,nonce:int)->bytes:
    if type(package) is not bytes or not 22<=len(package)<=96*1024*1024:
        raise ValueError('INVALID_PACKAGE_ENVELOPE')
    if type(manifest_entry) is not str:
        raise ValueError('INVALID_MANIFEST_ENVELOPE')
    name=manifest_entry.encode('utf-8',errors='strict')
    if not 1<=len(name)<=512:raise ValueError('INVALID_MANIFEST_ENVELOPE')
    return b'ARSMPK01'+struct.pack('<QII',nonce,len(name),len(package))+name+package


def admit_zip(package:bytes,manifest_entry:str)->Admission:
    nonce=secrets.randbits(64) or 1
    return _run_envelope(_package_envelope(package,manifest_entry,nonce),nonce)


def evaluate_zip(package:bytes,manifest_entry:str,frames,*,physics=False)->Admission:
    return _run_envelope(*_prepare_evaluation(package,manifest_entry,frames,physics=physics))


def _prepare_evaluation(package:bytes,manifest_entry:str,frames,*,physics=False):
    # Trusted transport custody can be prepared before its native authorization grant.
    if type(physics) is not bool:raise ValueError('INVALID_PHYSICS_MODE')
    nonce=secrets.randbits(64) or 1;payload=_package_envelope(package,manifest_entry,nonce)
    if type(frames) not in (tuple,list) or not 1<=len(frames)<=8:raise ValueError('INVALID_COMPLETE_FRAMES')
    commands=bytearray(struct.pack('<I',len(frames)));last=0
    for i,frame in enumerate(frames):
        if type(frame) not in (tuple,list) or len(frame)!=(4 if physics else 3):raise ValueError('INVALID_COMPLETE_FRAME')
        sequence,clock,bindings=frame[:3];control=frame[3] if physics else 0
        if type(control) is not int or not 0<=control<=3:raise ValueError('INVALID_PHYSICS_CONTROL')
        if type(sequence) is not int or sequence!=i+1 or type(clock) is not int or not 0<=clock<=1_000_000_000:
            raise ValueError('INVALID_FRAME_IDENTITY')
        if (i==0 and clock!=0) or (i>0 and not last<clock<=last+8*16_666_667):raise ValueError('INVALID_FRAME_CLOCK')
        last=clock
        if type(bindings) not in (tuple,list) or len(bindings)>32:raise ValueError('INVALID_BINDINGS')
        commands.extend(struct.pack('<QQII',sequence,clock,control,len(bindings)));seen=set()
        for pair in bindings:
            if type(pair) not in (tuple,list) or len(pair)!=2:raise ValueError('INVALID_BINDING')
            slot,value=pair
            if type(slot) is not int or not 0<=slot<4096 or slot in seen or type(value) not in (int,float) or not -1<=value<=1 or not math.isfinite(value):
                raise ValueError('INVALID_BINDING')
            seen.add(slot);commands.extend(struct.pack('<If',slot,value))
    return payload+commands,nonce,'--simulate' if physics else '--evaluate'


def _prepare_step(package,entry,sequence,clock,bindings,control,checkpoint):
    if type(sequence) is not int or not 1<=sequence<=8 or type(clock) is not int or not 0<=clock<=1_000_000_000:
        raise ValueError('INVALID_STEP_IDENTITY')
    if type(checkpoint) is not bytes or len(checkpoint)>128*1024 or (sequence==1 and (checkpoint or clock!=0)) or (sequence>1 and (len(checkpoint)<72 or clock==0)):
        raise ValueError('INVALID_STEP_PREDECESSOR')
    payload,nonce,_=_prepare_evaluation(package,entry,[(1,0,bindings,control)],physics=True)
    name=entry.encode('utf-8');offset=24+len(name)+len(package)
    command=bytearray(payload[offset+4:]);struct.pack_into('<QQ',command,0,sequence,clock)
    identity=hashlib.sha256(struct.pack('<I',len(name))+name+package).digest()
    return payload[:offset]+command+struct.pack('<I',len(checkpoint))+checkpoint,nonce,'--step',identity


def _run_envelope(payload:bytes,nonce:int,mode='--package')->Admission:
    # Diagnostics can exercise worker faults, but callers cannot supply arbitrary commands.
    if mode not in ('--package','--evaluate','--simulate','--step','--crash','--flood','--hang'):raise ValueError('INVALID_MODE')
    if type(payload) is not bytes or len(payload)>96*1024*1024+(128*1024+4096 if mode=='--step' else 4096 if mode in ('--evaluate','--simulate') else 536):raise ValueError('INVALID_ENVELOPE')
    folder=ROOT/'build/moc3-package-trials'/uuid.uuid4().hex[:24];folder.mkdir(parents=True,exist_ok=False)
    worker=folder/'worker.exe';shutil.copyfile(ISOLATED_BIN/'ars_moc3_import_worker.exe',worker)
    (folder/'worker-data.txt').write_bytes(b'generated read-only worker sentinel')
    profile='AvatarMocImport-'+uuid.uuid4().hex
    try:
        run=subprocess.run([str(ISOLATED_BIN/'ars_moc3_import_sandbox.exe'),str(worker),profile,mode],
                           input=payload,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,timeout=15,cwd=folder)
    except subprocess.TimeoutExpired:
        env=ctypes.WinDLL('userenv',use_last_error=True)
        env.DeleteAppContainerProfile.argtypes=[wintypes.LPCWSTR];env.DeleteAppContainerProfile.restype=ctypes.c_long
        env.DeleteAppContainerProfile(profile)
        return Admission('BROKER_TIMEOUT_REJECTED',b'',nonce,profile_absent(profile),False,folder)
    removed=profile_absent(profile);empty=False
    try:
        state=json.loads((folder/'launcher-state.json').read_text(encoding='utf-8'))
        # Exit 0 from this pinned launcher is only reachable after actual ActiveProcesses==0.
        empty=run.returncode==0 and type(state.get('worker_exit')) is int
    except (OSError,ValueError):pass
    status='ISOLATION_SETUP_REJECTED';packet=b''
    if run.returncode==0 and removed and empty:
        if (48<=len(run.stdout)<=PACKET_CAP and run.stdout[:8]==(b'ARSCAND1' if mode=='--step' else b'ARSEVAL1' if mode in ('--evaluate','--simulate') else b'ARSDRAW1') and
            state['worker_exit']==0 and state.get('wait_result')==0 and state.get('output_overflow') is False):
            # Custody only: this is not a decoded/verified scene. Host still validates every byte.
            status='PACKET_CUSTODY';packet=run.stdout
        elif run.stdout.strip() in REJECTIONS:status=run.stdout.strip().decode('ascii')
        else:status='BROKER_PROTOCOL_REJECTED'
    (folder/'broker-state.json').write_text(json.dumps({'status':status,'launcher_exit':run.returncode,
        'profile_removed':removed,'job_empty':empty,'output_bytes':len(run.stdout)}),encoding='utf-8')
    return Admission(status,packet,nonce,removed,empty,folder)


def validate_packet(packet:bytes,nonce:int,*,sequence=False):
    if type(packet) is not bytes or len(packet)>PACKET_CAP:raise ValueError('INVALID_PACKET_TRANSPORT')
    return subprocess.run([str(ISOLATED_BIN/'ars_moc3_packet_guard.exe'),str(nonce),*(['--sequence'] if sequence else [])],input=packet,
                          stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,timeout=15)


def render_packet(packet:bytes,nonce:int,output:Path,*,hardware=False,sequence_index=None):
    if type(packet) is not bytes or len(packet)>PACKET_CAP:raise ValueError('INVALID_PACKET_TRANSPORT')
    return subprocess.run([str(HOST_BIN/'ars_moc3_packet_render_host.exe'),str(output),str(nonce),
                           '--hardware' if hardware else '--warp',*(['--sequence',str(sequence_index)] if sequence_index is not None else [])],input=packet,stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL,timeout=30)
