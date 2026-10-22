"""Private persistent F4-B worker. --stream is image transport, never a log."""
import argparse
from collections import deque
from datetime import datetime,timezone
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import subprocess
import sys
import threading
import time
import uuid

from moc3_core_session import CoreSession,validate_control
from moc3_frame_packet import encode
from moc3_local_pixels import ROOT,BIN,PixelError,require
from moc3_local_replay import _private_manifest
from moc3_memory import process_memory
from moc3_package_admission import admit_package,AdmissionError
from moc3_physics import PhysicsError
from moc3_process_tree import wait_launch_gate

PERIOD=.25


def handles(pid):
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];k.OpenProcess.restype=wintypes.HANDLE
    k.GetProcessHandleCount.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)];k.GetProcessHandleCount.restype=wintypes.BOOL
    k.CloseHandle.argtypes=[wintypes.HANDLE];k.CloseHandle.restype=wintypes.BOOL
    process=k.OpenProcess(0x1000,False,pid)
    if not process:raise ctypes.WinError(ctypes.get_last_error())
    try:
        count=wintypes.DWORD()
        if not k.GetProcessHandleCount(process,ctypes.byref(count)):raise ctypes.WinError(ctypes.get_last_error())
        return count.value
    finally:k.CloseHandle(process)


def sample(session):
    pids={'native':session.bridge.process.pid,'worker':os.getpid()}
    return {key:{**process_memory(pid),'handles':handles(pid)} for key,pid in pids.items()}


def source_identity():
    names=('scripts/moc3_core_session.py','scripts/moc3_local_continuity.py','scripts/moc3_test_harness.py',
           'scripts/moc3_local_integration.py','scripts/moc3_local_physics.py','scripts/moc3_physics.py',
           'scripts/moc3_frame_packet.py','scripts/moc3_memory.py','scripts/moc3_process_tree.py')
    return {name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in names}


def run(args,packet):
    folder=ROOT/'build/moc3-f4b-local'/args.run_id;folder.mkdir(parents=True,exist_ok=False)
    print('F4B_STAGE PACKAGE',flush=True)
    manifest=_private_manifest();require(manifest is not None,'F4B_PRIVATE_FIXTURE_UNAVAILABLE')
    package=admit_package(manifest.parent,manifest.name)
    require(package.linked_physics_bytes is not None,'F4B_LINKED_PHYSICS_REQUIRED')
    packet['private_input']={'moc_sha256':hashlib.sha256(package.moc_bytes).hexdigest(),
                            'physics_sha256':hashlib.sha256(package.linked_physics_bytes).hexdigest()}
    inbox=queue.Queue(maxsize=1);stop=threading.Event()
    def read_controls():
        while not stop.is_set():
            line=sys.stdin.readline(4097)
            value={'eof':True} if not line else {'line':line}
            while not stop.is_set():
                try:inbox.put(value,timeout=.1);break
                except queue.Full:pass
            if not line or len(line)>4096:return
    if not args.soak:threading.Thread(target=read_controls,daemon=True).start()
    session=CoreSession(package,folder/'host',args.hardware)
    timings=deque(maxlen=9000);observations=[];control_seq=0;control=None;expires=0
    start=time.monotonic();next_frame=start;next_memory=start
    previous=None;changed=False;warm=None
    try:
        print('F4B_STAGE ACTIVE',flush=True)
        while time.monotonic()-start<args.seconds:
            now=time.monotonic();ack=None
            if args.soak:
                phase=session.sequence%80
                drive=.5 if phase<16 else None;enabled=phase<64;reset=phase==0
            else:
                try:item=inbox.get_nowait()
                except queue.Empty:item=None
                if item:
                    if item.get('eof'):break
                    require(len(item['line'])<=4096,'F4B_CONTROL_BUDGET')
                    value=validate_control(json.loads(item['line']))
                    require(value['sequence']>control_seq,'F4B_REPLAYED_CONTROL')
                    control=value;control_seq=value['sequence'];expires=now+value['lease_ms']/1000
                    ack=control_seq
                active=control if control and now<expires else None
                drive=active['drive'] if active else None;enabled=active['physics'] if active else True
                reset=active['reset'] if active else False
            frame_started=time.perf_counter_ns()
            meta,rgba,ppm,geometry=session.update(drive,enabled=enabled,reset=reset)
            if control:control['reset']=False
            identity=(hashlib.sha256(rgba).hexdigest(),geometry)
            if previous and all(a!=b for a,b in zip(identity,previous)):changed=True
            previous=identity
            if args.stream and not args.hardware:print(encode(meta,rgba,ppm),flush=True)
            if ack is not None:print(f'F4B_CONTROL {ack} {session.sequence}',flush=True)
            timings.append(time.perf_counter_ns()-frame_started)
            if now>=next_memory:
                memory=sample(session);elapsed=time.monotonic()-start
                observations.append({'elapsed_seconds':elapsed,'processes':memory})
                require(sum(m['peak_commit_bytes'] for m in memory.values())<=1536*1024*1024,'F4B_MEMORY_CAP')
                if warm is None and elapsed>=args.warmup:warm=memory
                (folder/'progress.json').write_text(json.dumps({'elapsed_seconds':elapsed,'frames':session.sequence,
                      'memory':memory,'warmup_complete':warm is not None}),encoding='utf-8')
                next_memory=now+10
            next_frame+=PERIOD
            time.sleep(max(0,next_frame-time.monotonic()))
        elapsed=time.monotonic()-start
        observations.append({'elapsed_seconds':elapsed,'processes':sample(session)})
        packet.update(elapsed_seconds=elapsed,frames=session.sequence,changed_geometry_and_pixels=changed,
                      timing_ns=list(timings),memory_observations=observations,final_core_generation=session.sequence)
        require(session.sequence>0,'F4B_NO_FRAMES')
        require(len(list((folder/'host').glob('slot-*.rgba')))<=2,'F4B_IMAGE_RING')
        packet['memory']=session.finish()
        if args.soak:
            require(elapsed>=args.seconds,'F4B_TRUNCATED_SOAK')
            require(changed,'F4B_STATIC_SOAK')
            measured=[x['processes'] for x in observations if x['elapsed_seconds']>=args.warmup]
            if len(measured)>=3:
                ws_growth=any(max(x[k]['working_set_bytes'] for x in measured)-measured[0][k]['working_set_bytes']>64*1024*1024
                              for k in ('native','worker'))
                handle_growth=any(all(b[k]['handles']>=a[k]['handles'] for a,b in zip(measured,measured[1:])) and
                                  measured[-1][k]['handles']>measured[0][k]['handles'] for k in ('native','worker'))
                packet['growth_verdict']='INVESTIGATE' if ws_growth or handle_growth else 'NO_GROWTH_TRIGGER'
                require(not ws_growth and not handle_growth,'F4B_RETAINED_GROWTH_INVESTIGATION_REQUIRED')
            packet['duration_verdict']='PASS_30MIN_AFTER_WARMUP' if args.seconds-args.warmup>=1800 and args.warmup>=300 else 'SHORT_TEST_ONLY'
            packet['quality_tier_verdict']='HOLD_NOT_720P_30FPS_OR_GPU_ALLOCATION_PROOF'
        packet['verdict']='PASS'
    finally:
        stop.set();session.close()


def main():
    wait_launch_gate()
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-id',default=uuid.uuid4().hex[:24]);parser.add_argument('--stream',action='store_true')
    parser.add_argument('--soak',action='store_true');parser.add_argument('--hardware',action='store_true')
    parser.add_argument('--seconds',type=float,default=2100);parser.add_argument('--warmup',type=float,default=300)
    args=parser.parse_args()
    if not re.fullmatch('[0-9a-f]{24}',args.run_id) or not 0<args.seconds<=2200 or not 0<=args.warmup<args.seconds:
        parser.error('invalid bounded continuity options')
    packet={'phase':'F4-B','started_utc':datetime.now(timezone.utc).isoformat(),'source_sha256':source_identity(),
            'binary_sha256':hashlib.sha256((BIN/'ars_moc3_product_bridge.exe').read_bytes()).hexdigest(),
            'parent_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'seconds_requested':args.seconds,'warmup_seconds':args.warmup,'hardware_requested':args.hardware,
            'clock_policy':'FIXED_SIX_PHYSICS_STEPS_PER_PRESENTED_UPDATE_WALL_LEASES'}
    try:run(args,packet)
    except (PixelError,PhysicsError,AdmissionError,OSError,ValueError,TypeError,KeyError,subprocess.SubprocessError) as error:
        packet.update(verdict='FAIL',reason=str(error) if isinstance(error,(PixelError,PhysicsError)) else type(error).__name__)
    folder=ROOT/'build/moc3-f4b-local'/args.run_id;folder.mkdir(parents=True,exist_ok=True)
    packet['completed_utc']=datetime.now(timezone.utc).isoformat()
    (folder/'evidence.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
    print('F4B_LOCAL_CONTINUITY_'+packet['verdict'],flush=True)
    return 0 if packet['verdict']=='PASS' else 1


if __name__=='__main__':raise SystemExit(main())
