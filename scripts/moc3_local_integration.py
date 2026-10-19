"""F4 private real-core -> linked physics -> host integration proof.

--stream is an internal image transport for the visible harness; never log it.
Default CLI output contains only fixed status tokens. All detailed data ignored.
"""
from __future__ import annotations
import argparse
from datetime import datetime,timezone
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import time
import uuid
from moc3_local_physics import Bridge
from moc3_local_pixels import ROOT,BIN,PixelError,require,eligible as pixels_eligible
from moc3_local_replay import _private_manifest
from moc3_package_admission import admit_package,AdmissionError
from moc3_physics import Physics,PhysicsError,STEP_NS
from moc3_process_tree import wait_launch_gate
from moc3_frame_packet import encode

def eligible():
    return pixels_eligible() and (BIN/'ars_moc3_product_bridge.exe').is_file()

class ProductBridge(Bridge):
    def __init__(self,package,folder,hardware,drive,*,binary='ars_moc3_product_bridge.exe'):
        super().__init__(package,folder,hardware,binary=binary)
        try:
            self.send('CORE_BIND 1\n'+drive)
            require(self.response()==['CORE_BOUND'],'F4_CORE_BIND')
        except BaseException:self.close();raise

    def prepare_core(self,sequence,parameters):
        self.send('\n'.join([f'CORE_PREP {sequence} {len(parameters)}',
                             *(f'{key} {value:.9g}' for key,value in parameters)]))
        header=self.response()
        if header[0]=='CORE_ERROR':return None,int(header[1])
        require(len(header)==3 and header[0]=='CORE_POSE' and 0<int(header[2])<=32,'F4_CORE_CANDIDATE')
        result={}
        for _ in range(int(header[2])):
            row=self.response();require(len(row)==2 and row[0] not in result,'F4_CORE_VALUES')
            result[row[0]]=float(row[1])
        return result,int(header[1])

    def command(self,command):
        self.send(command);return self.response()

def percentile(values,proportion):
    values=sorted(values)
    return values[min(len(values)-1,math.ceil(len(values)*proportion)-1)]

def replay(package,folder,hardware,stream):
    # Select an actual linked input with a usable positive native range.
    bootstrap=Bridge(package,folder/'catalog',hardware,binary='ars_moc3_product_bridge.exe')
    try:
        physics=Physics(package.linked_physics_bytes,bootstrap.catalog)
        sources=[b.key for c in physics.chains for b in c.inputs if b.axis=='Angle' and b.weight>0]
        drive=next((key for key in sources if bootstrap.catalog[key][2]>bootstrap.catalog[key][1]),None)
        require(drive is not None,'F4_NO_EXCITABLE_LINKED_SOURCE')
        native_default=bootstrap.frame({key:bounds[1] for key,bounds in bootstrap.catalog.items()})
        bootstrap.finish()
    finally:bootstrap.close()
    bridge=ProductBridge(package,folder,hardware,drive)
    try:
        physics=Physics(package.linked_physics_bytes,bridge.catalog)
        defaults={key:bounds[1] for key,bounds in bridge.catalog.items()}
        amplitude=defaults[drive]+(bridge.catalog[drive][2]-defaults[drive])*.5
        trace=[];timings=[];original=[];release=[]
        def frame(sequence,simulation_tick,value,replay_mode=False,enabled=True,mode=None):
            started=time.perf_counter_ns()
            core,generation=bridge.prepare_core(sequence,[(drive,value)] if value is not None else [])
            require(core is not None and generation==sequence,'F4_ACTUAL_CORE_GENERATION')
            require(core[drive]==float(f'{(value if value is not None else defaults[drive]):.9g}'),
                    'F4_ACTUAL_CORE_VALUE')
            candidate=physics.prepare(simulation_tick*6*STEP_NS,core,enabled=enabled)
            if not enabled:
                expected=dict(defaults);expected.update(core)
                require(dict(candidate.parameters)==expected,'F4_DISABLED_MANUAL_DEFAULT_VALUES')
            raw,geometry=bridge.frame(dict(candidate.parameters))
            require(bridge.command('CORE_COMMIT')==['CORE_CURRENT',str(generation)],'F4_CORE_FINAL_PUBLISH')
            physics.commit(candidate)
            metadata=json.loads((folder/'latest.json').read_text())
            require(metadata['frame_generation']==sequence,'F4_HOST_CORE_SEQUENCE')
            behavior='CORE_REPLAY' if replay_mode else 'CORE_RELEASE' if value is None else 'CORE_PHYSICS'
            if mode:behavior=mode
            public={key:metadata[key] for key in ('width','height','frame_generation','snapshot_generation',
                                                   'target_epoch','device_generation','backend')}
            public.update(core_generation=generation,behavior=behavior)
            ppm=(folder/metadata['image']).read_bytes()
            if stream and not hardware:
                print(encode(public,raw,ppm),flush=True)
            duration=time.perf_counter_ns()-started;timings.append(duration)
            trace.append({'core_generation':generation,'frame_generation':bridge.frames,
                          'timestamp_ns':candidate.timestamp,'behavior':behavior,'geometry_checksum':geometry,
                          'clean_sha256':hashlib.sha256(raw).hexdigest(),'duration_ns':duration})
            # Freeze input pacing, not an invented performance PASS threshold.
            time.sleep(max(0,.1-duration/1e9))
            return raw,geometry
        print('F4_STAGE CORE_SOAK',flush=True)
        for i in range(1,121):
            phase=(i-1)%40;value=amplitude if phase<8 else None
            result=frame(i,i,value)
            if i<=16:original.append(result)
            if 9<=i<=16:release.append(result)
        require(any(r[0]!=release[0][0] and r[1]!=release[0][1] for r in release[1:]),
                'F4_RELEASE_GEOMETRY_AND_PIXELS')
        print('F4_STAGE RESET_REPLAY',flush=True)
        physics.commit(physics.prepare(0,reset=True))
        for i in range(1,17):
            result=frame(120+i,i,amplitude if i<=8 else None,True)
            require(result==original[i-1],'F4_CORE_PHYSICS_RESET_REPLAY')
        print('F4_STAGE PHYSICS_TOGGLE',flush=True)
        toggle_on=frame(137,17,amplitude)
        toggle_off=frame(138,18,amplitude,enabled=False,mode='CORE_DISABLED')
        require(toggle_on[0]!=toggle_off[0] and toggle_on[1]!=toggle_off[1],
                'F4_CORE_PHYSICS_TOGGLE_GEOMETRY_AND_PIXELS')
        resumed=frame(139,19,amplitude,mode='CORE_REENABLED')
        require(resumed==original[0],'F4_CORE_PHYSICS_RESUME_FROM_REST')
        disabled_default=frame(140,20,None,enabled=False,mode='CORE_DISABLED')
        require(disabled_default==native_default,'F4_CORE_DISABLED_NATIVE_DEFAULT_RENDER')
        print('F4_STAGE NEGATIVE',flush=True)
        old_generation=140
        for parameters,sequence,wanted in (([(drive,bridge.catalog[drive][2]+1)],141,1),
                                           ([('unknown.f4',0)],141,1),
                                           ([(drive,0),(drive,0)],141,1),([],140,13)):
            values,status=bridge.prepare_core(sequence,parameters)
            require(values is None and status==wanted,'F4_NATIVE_INPUT_REJECTION')
            require(bridge.command('CORE_READ')==['CORE_CURRENT',str(old_generation)],'F4_REJECTION_PRESERVES_CORE')
        core,generation=bridge.prepare_core(141,[(drive,amplitude)])
        require(core is not None and generation==141,'F4_LATE_REVOKE_PREPARE')
        candidate=physics.prepare(21*6*STEP_NS,core)
        bridge.frame(dict(candidate.parameters))  # completed private GPU target is not advertised
        require(bridge.command('CORE_REVOKE')==['CORE_REVOKED','0'],'F4_SOURCE_REVOKE')
        require(bridge.command('CORE_COMMIT')==['CORE_ERROR','11'],'F4_FINAL_AUTHORIZATION_FENCE')
        require(bridge.command('CORE_READ')==['CORE_CURRENT','140'],'F4_LATE_REVOKE_OLD_CORE')
        require(physics.timestamp==20*6*STEP_NS,'F4_LATE_REVOKE_OLD_PHYSICS')
        # No packet is emitted for the rejected private target.
        (folder/'trace.json').write_text(json.dumps({'trace':trace,'timing_ns':timings,'mapped_input':drive,
                                                    'p50_ns':percentile(timings,.5),'p95_ns':percentile(timings,.95)},indent=2))
        require(len(list(folder.glob('slot-*.rgba')))==2 and len(list(folder.glob('slot-*.ppm')))==2,
                'F4_BOUNDED_IMAGE_RING')
        memory=bridge.finish()
        return {'verdict':'PASS','memory':memory,'adapter':json.loads((folder/'adapter.json').read_text()),
                'accepted_frames':140,'rejected_target_not_advertised':True,'core_path_physics_toggle':'PASS'}
    finally:bridge.close()

def run(run_id,packet,stream):
    require(eligible(),'F4_BUILD_OR_FIXTURE_UNAVAILABLE')
    folder=ROOT/'build/moc3-f4-local'/run_id;folder.mkdir(parents=True,exist_ok=False)
    print('F4_STAGE PACKAGE',flush=True)
    manifest=_private_manifest();package=admit_package(manifest.parent,manifest.name)
    require(package.linked_physics_bytes is not None,'F4_LINKED_PHYSICS_REQUIRED')
    packet['private_input']={'moc_sha256':hashlib.sha256(package.moc_bytes).hexdigest(),
                            'physics_sha256':hashlib.sha256(package.linked_physics_bytes).hexdigest()}
    for name,hardware in (('warp',False),('hardware',True)):
        print('F4_STAGE '+name.upper(),flush=True)
        packet[name]=replay(package,folder/name,hardware,stream)
    print('F4_STAGE COMPLETE',flush=True)

if __name__=='__main__':
    wait_launch_gate()
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-id',default=uuid.uuid4().hex[:24]);parser.add_argument('--stream',action='store_true')
    args=parser.parse_args();require(re.fullmatch('[0-9a-f]{24}',args.run_id) is not None,'INVALID_RUN_ID')
    packet={'phase':'F4','started_utc':datetime.now(timezone.utc).isoformat(),'proof_tier':'private_real_core_host'}
    names=('CMakeLists.txt','src/core/runtime_lifecycle.cpp','src/core/runtime_lifecycle.hpp',
           'src/moc3/core_consumer.cpp','src/moc3/core_consumer.hpp','src/moc3/pixel_bridge.cpp',
           'scripts/moc3_local_integration.py','scripts/moc3_local_physics.py','scripts/moc3_frame_packet.py',
           'scripts/moc3_physics.py','scripts/moc3_test_harness.py','tests/moc3/core_consumer_tests.cpp')
    packet['source_sha256']={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in names}
    packet['binary_sha256']=hashlib.sha256((BIN/'ars_moc3_product_bridge.exe').read_bytes()).hexdigest()
    packet['parent_head']=subprocess.run(['git','rev-parse','HEAD'],cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()
    try:run(args.run_id,packet,args.stream);packet['verdict']='PASS'
    except (PixelError,PhysicsError,AdmissionError,OSError,ValueError,TypeError,KeyError,subprocess.SubprocessError) as error:
        packet['verdict']='FAIL';packet['reason']=str(error) if isinstance(error,(PixelError,PhysicsError)) else type(error).__name__
    packet['completed_utc']=datetime.now(timezone.utc).isoformat()
    folder=ROOT/'build/moc3-f4-local'/args.run_id;folder.mkdir(parents=True,exist_ok=True)
    (folder/'evidence.json').write_text(json.dumps(packet,indent=2))
    print('F4_LOCAL_CORE_HOST_PASS' if packet['verdict']=='PASS' else 'F4_LOCAL_CORE_HOST_FAIL',flush=True)
    raise SystemExit(0 if packet['verdict']=='PASS' else 1)
