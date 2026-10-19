"""Private F3 manifest-linked physics -> evaluator -> host readback proof."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import queue
import re
import struct
import subprocess
import threading
import time
import uuid

from moc3_local_pixels import ROOT, BIN, PixelError, require, eligible
from moc3_local_replay import _private_manifest
from moc3_package_admission import admit_package, AdmissionError
from moc3_physics import Physics, PhysicsError, STEP_NS
from moc3_memory import process_memory
from moc3_process_tree import wait_launch_gate
from moc3_session import MAX_TOTAL_PEAK_COMMIT

class Bridge:
    def __init__(self, package, folder, hardware, *, binary='ars_moc3_pixel_bridge.exe'):
        self.folder=folder;self.frames=0;self.started=False
        self.lines=queue.Queue(maxsize=16)
        args=[str(BIN/binary),str(folder)]
        if hardware:args.append('--hardware')
        self.process=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        def reader():
            while True:
                line=self.process.stdout.readline(1024)
                self.lines.put(line)
                if not line:return
        self.reader=threading.Thread(target=reader,daemon=True)
        try:
            self.reader.start();self.started=True
            self.process.stdin.write(b'ARSF2PK1'+struct.pack('<II',len(package.moc_bytes),len(package.textures)))
            self.process.stdin.write(package.moc_bytes)
            for image in package.textures:
                self.process.stdin.write(struct.pack('<II',image.width,image.height));self.process.stdin.write(image.rgba)
            self.process.stdin.flush();require(self.response()==['READY'],'F3_NATIVE_READY')
            self.send('CATALOG');header=self.response()
            require(len(header)==2 and header[0]=='CATALOG' and 0<int(header[1])<=4096,'F3_NATIVE_CATALOG')
            self.catalog={}
            for _ in range(int(header[1])):
                row=self.response();require(len(row)==4 and row[0] not in self.catalog,'F3_NATIVE_CATALOG')
                self.catalog[row[0]]=tuple(map(float,row[1:]))
        except BaseException:
            self.close();raise

    def send(self,text):
        self.process.stdin.write(text.encode('ascii')+b'\n');self.process.stdin.flush()

    def response(self):
        try:line=self.lines.get(timeout=30)
        except queue.Empty as error:raise PixelError('F3_NATIVE_TIMEOUT') from error
        require(bool(line) and len(line)<1024,'F3_NATIVE_CLOSED')
        return line.decode('ascii').split()

    def frame(self,parameters,notify=None,behavior='PHYSICS_ON'):
        self.send('\n'.join([f'FRAME {len(parameters)}',*(f'{k} {v:.9g}' for k,v in parameters.items())]))
        self.frames+=1
        require(self.response()==['FRAME',str(self.frames),str(self.frames+1),str(self.frames)],'F3_FRAME_IDENTITY')
        meta=json.loads((self.folder/'latest.json').read_text())
        require(meta['frame_generation']==self.frames and meta['target_epoch']==self.frames,'F3_READBACK_IDENTITY')
        rgba=(self.folder/meta['rgba']).read_bytes()
        require(len(rgba)==512*512*4 and any(rgba[3::4]),'F3_NONBLANK_READBACK')
        self.send('GEOMETRY');geometry=self.response()
        require(len(geometry)==2 and geometry[0]=='GEOMETRY','F3_GEOMETRY')
        if notify:
            public={k:meta[k] for k in ('width','height','frame_generation','snapshot_generation',
                                      'target_epoch','device_generation','backend')}
            public['image']=f"warp/{meta['image']}"
            public['behavior']=behavior
            (self.folder.parent/f'frame-{self.frames}.json').write_text(json.dumps(public))
            notify(self.frames)
            time.sleep(.08)
        return rgba,geometry[1]

    def finish(self):
        memory={'native':process_memory(self.process.pid),'parent':process_memory()}
        (self.folder/'memory.json').write_text(json.dumps(memory))
        require(sum(m['peak_commit_bytes'] for m in memory.values())<=MAX_TOTAL_PEAK_COMMIT,'F3_COMBINED_MEMORY_BUDGET')
        self.send('QUIT');require(self.response()==['BYE'],'F3_DEBUG_AND_CLOSE')
        require(self.process.wait(timeout=5)==0,'F3_NATIVE_EXIT')
        return memory

    def close(self):
        if self.process.poll() is None:self.process.kill();self.process.wait(timeout=5)
        if self.started:self.reader.join(timeout=1)
        self.process.stdin.close();self.process.stdout.close()

def replay(package,folder,hardware,notify):
    bridge=Bridge(package,folder,hardware)
    try:
        physics=Physics(package.linked_physics_bytes,bridge.catalog)
        sources=[b.key for c in physics.chains for b in c.inputs if b.axis=='Angle' and b.weight>0]
        require(bool(sources),'F3_NO_EXCITABLE_ANGLE_SOURCE')
        drive=next((key for key in sources if bridge.catalog[key][2]>bridge.catalog[key][1]),None)
        require(drive is not None,'F3_NO_POSITIVE_INPUT_RANGE')
        low,default,high=bridge.catalog[drive];amplitude=default+(high-default)*.5
        poses=[{drive:amplitude} if i<2 else {} for i in range(8)]
        on=[];different=[];release=[];trace=[]
        print('F3_STAGE ON_OFF',flush=True)
        for i,manual in enumerate(poses,1):
            candidate=physics.prepare(i*6*STEP_NS,manual)
            on_frame=bridge.frame(dict(candidate.parameters),notify)
            physics.commit(candidate)  # only after evaluator + exact GPU commit
            trace.append({'timestamp_ns':candidate.timestamp,'manual':manual,'parameters':candidate.parameters,
                          'particle_state':candidate.state,'steps':candidate.steps,
                          'geometry_checksum':on_frame[1],'frame_generation':bridge.frames})
            on.append(on_frame)
            base={k:v[1] for k,v in bridge.catalog.items()};base.update(manual)
            off_frame=bridge.frame(base,notify,'PHYSICS_OFF')
            different.append(on_frame[0]!=off_frame[0] and on_frame[1]!=off_frame[1])
            if i>2:release.append(different[-1])
        require(any(different),'F3_ON_OFF_GEOMETRY_AND_PIXELS')
        require(any(release),'F3_RELEASE_GEOMETRY_AND_PIXELS')
        require(any(on[i][0]!=on[2][0] and on[i][1]!=on[2][1] for i in range(3,len(on))),
                'F3_RELEASE_TEMPORAL_RESPONSE')
        print('F3_STAGE RESET_REPLAY',flush=True)
        physics.commit(physics.prepare(0,reset=True))
        for i,manual in enumerate(poses,1):
            c=physics.prepare(i*6*STEP_NS,manual);frame=bridge.frame(dict(c.parameters),notify,'RESET_REPLAY')
            require(frame==on[i-1],'F3_RESET_REPLAY');physics.commit(c)
        print('F3_STAGE RELOAD',flush=True)
        physics=Physics(package.linked_physics_bytes,bridge.catalog)
        for i,manual in enumerate(poses,1):
            c=physics.prepare(i*6*STEP_NS,manual);frame=bridge.frame(dict(c.parameters),notify,'RELOAD_REPLAY')
            require(frame==on[i-1],'F3_RELOAD_REPLAY');physics.commit(c)
        prior=(physics.timestamp,physics.generation,physics.state)
        for timestamp in (0,physics.timestamp+9*STEP_NS):
            try:physics.prepare(timestamp)
            except PhysicsError:pass
            else:raise PixelError('F3_CLOCK_NEGATIVE_ACCEPTED')
        require(prior==(physics.timestamp,physics.generation,physics.state),'F3_NEGATIVE_MUTATED_STATE')
        c=physics.prepare(physics.timestamp+STEP_NS,{},enabled=False)
        require(all(v==bridge.catalog[k][1] for k,v in c.parameters),'F3_DISABLE_DEFAULTS')
        bridge.frame(dict(c.parameters),notify,'DISABLED');physics.commit(c)
        (folder/'trace.json').write_text(json.dumps({'catalog':bridge.catalog,'drive':drive,'trace':trace}))
        memory=bridge.finish()
        return {'verdict':'PASS','frames':bridge.frames,'memory':memory,
                'adapter':json.loads((folder/'adapter.json').read_text()),
                'linked_sha256':hashlib.sha256(package.linked_physics_bytes).hexdigest(),
                'on_frame_sha256':[hashlib.sha256(raw).hexdigest() for raw,_ in on]}
    finally:bridge.close()

def run(run_id,packet):
    require(eligible(),'F3_LOCAL_FIXTURE_OR_BUILD_UNAVAILABLE')
    folder=ROOT/'build/moc3-f3-local'/run_id;folder.mkdir(parents=True,exist_ok=False)
    print('F3_STAGE PACKAGE',flush=True)
    manifest=_private_manifest();package=admit_package(manifest.parent,manifest.name)
    require(package.linked_physics is not None and package.linked_physics_bytes is not None,'F3_NO_LINKED_PHYSICS')
    packet['private_input']={'moc_sha256':hashlib.sha256(package.moc_bytes).hexdigest(),
                            'atlas_sha256':[hashlib.sha256(t.rgba).hexdigest() for t in package.textures]}
    for name,hardware in (('warp',False),('hardware',True)):
        print('F3_STAGE '+name.upper(),flush=True)
        packet[name]=replay(package,folder/name,hardware,
                           (lambda n:print(f'F3_FRAME {n}',flush=True)) if not hardware else None)
    print('F3_STAGE COMPLETE',flush=True)

if __name__=='__main__':
    wait_launch_gate()
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run-id',default=uuid.uuid4().hex[:24])
    args=parser.parse_args();require(re.fullmatch('[0-9a-f]{24}',args.run_id) is not None,'INVALID_RUN_ID')
    packet={'phase':'F3','proof_tier':'host_integrated_local','started_utc':datetime.now(timezone.utc).isoformat()}
    names=('scripts/moc3_physics.py','scripts/moc3_local_physics.py','src/moc3/independent_snapshot.cpp',
           'src/moc3/independent_snapshot.hpp','src/moc3/pixel_bridge.cpp','scripts/moc3_test_harness.py',
           'tests/moc3/test_linked_physics.py','CMakeLists.txt')
    packet['source_sha256']={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in names}
    packet['binary_sha256']=hashlib.sha256((BIN/'ars_moc3_pixel_bridge.exe').read_bytes()).hexdigest()
    packet['parent_head']=subprocess.run(['git','rev-parse','HEAD'],cwd=ROOT,capture_output=True,text=True,check=True).stdout.strip()
    try:run(args.run_id,packet);packet['verdict']='PASS'
    except (PixelError,PhysicsError,AdmissionError,OSError,ValueError,TypeError,KeyError,subprocess.SubprocessError) as error:
        packet['verdict']='FAIL';packet['reason']=str(error) if isinstance(error,(PixelError,PhysicsError)) else type(error).__name__
    folder=ROOT/'build/moc3-f3-local'/args.run_id;folder.mkdir(parents=True,exist_ok=True)
    packet['completed_utc']=datetime.now(timezone.utc).isoformat()
    (folder/'evidence.json').write_text(json.dumps(packet,indent=2))
    print('F3_LOCAL_LINKED_PHYSICS_PASS' if packet['verdict']=='PASS' else 'F3_LOCAL_LINKED_PHYSICS_FAIL',flush=True)
    raise SystemExit(0 if packet['verdict']=='PASS' else 1)
