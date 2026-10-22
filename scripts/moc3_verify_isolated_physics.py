"""Actual bounded isolated physics proof; private fixture evidence stays ignored."""
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import time
import uuid
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from moc3_admission_provenance import capture
from moc3_admission_fixtures import archive
from moc3_public_package import evaluate_zip,validate_packet,render_packet,_package_envelope,_run_envelope,ISOLATED_BIN
from moc3_verify_retained_session import require,offsets,vertex_positions
from moc3_local_replay import _private_manifest
from moc3_physics import Physics,STEP_NS


def canonical(packet):
    out=bytearray(packet);struct.pack_into('<Q',out,16,0)
    for _,start,_ in offsets(out):struct.pack_into('<Q',out,start+28,0)
    return out


def run(evidence,notify=lambda *_:None):
    before=capture();report={'kind':'PUB02_B_ISOLATED_BOUNDED_PHYSICS','physics':'SIMULATED_BOUNDED_INDEPENDENT',
                            'whole_PUB02':'HOLD','PUB04':'HOLD','cases':[],'closure_before':before}
    def passed(name):
        report['cases'].append({'case':name,'result':'PASS'});notify(name,None)
        (evidence/'evidence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    def admission(frames):
        result=evaluate_zip(zipped,m.name,frames,physics=True)
        require(result.status=='PACKET_CUSTODY' and result.job_empty and result.profile_removed,'actual physics custody/cleanup')
        require(validate_packet(result.packet,result.nonce,sequence=True).returncode==0,'independent complete physics validation')
        return result
    try:
        for executable,args,token in (
            ('ars_moc3_retained_physics_tests.exe',[str(ROOT/'tests/moc3/generated_physics3.json')],b'RETAINED_PHYSICS_PASS'),
            ('ars_moc3_sequence_tests.exe',[],b'SEQUENCE_PACKET_PASS')):
            result=subprocess.run([str(ISOLATED_BIN/executable),*args],capture_output=True,timeout=15)
            require(result.returncode==0 and token in result.stdout,'native physics/protocol tests')
        passed('native_physics_and_protocol')
        from PIL import Image
        from moc3_package_admission import admit_package
        from moc3_local_physics import Bridge
        m=_private_manifest();require(m is not None,'authorized local fixture');files={m.name:m.read_bytes()};refs=json.loads(m.read_bytes())['FileReferences']
        for name in [refs['Moc'],refs['Physics']]:files[name]=(m.parent/name).read_bytes()
        for name in refs['Textures']:
            with Image.open(m.parent/name) as image:
                image=image.convert('RGBA');image.thumbnail((512,512),Image.Resampling.LANCZOS);buffer=io.BytesIO();image.save(buffer,format='PNG');files[name]=buffer.getvalue()
        fixture=evidence/'explicit-512-test-derivative';fixture.mkdir()
        for name,data in files.items():
            path=fixture/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        baseline=admit_package(fixture,m.name);reference=evidence/'catalog';reference.mkdir();bridge=Bridge(baseline,reference,False)
        try:
            catalog=dict(bridge.catalog);keys=list(catalog)
            key=next(name for name in ('ParamAngleX','ParamAngleY','ParamBodyAngleX') if name in catalog and catalog[name][2]>catalog[name][1])
            slot=keys.index(key);bounds=catalog[key];bridge.finish()
        finally:bridge.close()
        controls=(0,1,1,1,0,3,1,0)
        frames=[(i+1,i*2*STEP_NS,[(slot,.75)] if i==1 else [],control) for i,control in enumerate(controls)]
        zipped=archive(files);on=admission(frames);(evidence/'sequence.bin').write_bytes(on.packet)
        off=admission([(seq,clock,bindings,0) for seq,clock,bindings,_ in frames]);passed('actual_isolated_physics_on_off')
        xy=vertex_positions(on.packet);off_xy=vertex_positions(off.packet)
        require(any(xy[i]!=off_xy[i] for i in (1,2,3)),'actual physics changes XY vs OFF')
        require(xy[2]!=xy[3] and any(xy[i]!=off_xy[i] for i in (2,3)),'released temporal XY response')
        require(xy[0]==xy[4]==xy[7],'disable releases to defaults')
        reset=admission([(1,0,[],0),(2,2*STEP_NS,[],3)])
        require(xy[5]==vertex_positions(reset.packet)[1],'actual reset restores fresh particle state')
        repeat=admission(frames);require(canonical(on.packet)==canonical(repeat.packet),'deterministic fresh worker replay')
        passed('geometry_release_disable_reset_replay')
        for hardware in (False,True):
            backend='hardware' if hardware else 'warp';folder=evidence/'reference'/backend;folder.mkdir(parents=True);bridge=Bridge(baseline,folder,hardware)
            physics=Physics(baseline.linked_physics_bytes,catalog);actual=[];off_pixels=[]
            try:
                for i,(_,clock,_,control) in enumerate(frames):
                    manual={key:bounds[1]+.75*(bounds[2]-bounds[1])} if i==1 else {}
                    candidate=physics.prepare(clock,manual,enabled=bool(control&1),reset=bool(control&2))
                    expected=bridge.frame(dict(candidate.parameters))[0];out=evidence/'render'/backend/str(i)
                    result=render_packet(on.packet,on.nonce,out,hardware=hardware,sequence_index=i)
                    require(result.returncode==0,'physics packet GPU render')
                    proof=json.loads((out/'admission.json').read_text());require(proof['physics']=='SIMULATED_BOUNDED_INDEPENDENT','physics method labeled')
                    rgba=(out/'frame.rgba').read_bytes();require(rgba==expected,'exact Python-method reference pixels '+backend);actual.append(rgba);physics.commit(candidate)
                    other=evidence/'off'/backend/str(i);result=render_packet(off.packet,off.nonce,other,hardware=hardware,sequence_index=i)
                    require(result.returncode==0,'OFF control render');off_pixels.append((other/'frame.rgba').read_bytes())
                    notify('physics_frame_'+backend,{'backend':backend,'ppm':(out/'frame.ppm').read_bytes(),'rgba':rgba,'sequence':i+1,
                           'pose':'PHYSICS_RESET' if control&2 else 'PHYSICS_ON' if control&1 else 'PHYSICS_OFF'})
                    time.sleep(.6)
                bridge.finish()
            finally:bridge.close()
            require(any(actual[i]!=off_pixels[i] for i in (1,2,3)) and actual[2]!=actual[3],'actual ON/OFF and released pixels '+backend)
            require(actual[0]==actual[4]==actual[7],'OFF/default pixels '+backend);passed('python_reference_pixels_and_controls_'+backend)
        # Mutate only known generated framing after a real independent guard pass.
        _,last,_=offsets(on.packet)[-1]
        for name,position,value in (('mixed_late_version',last+8,2),('missing_simulation_marker',last+36,3),('unknown_flag',last+36,387),('wrong_outer_version',8,1)):
            bad=bytearray(on.packet);struct.pack_into('<I',bad,position,value)
            require(validate_packet(bytes(bad),on.nonce,sequence=True).returncode==3,'physics output guard '+name)
            out=evidence/'wire-reject'/name;result=render_packet(bytes(bad),on.nonce,out,sequence_index=0)
            proof=json.loads((out/'admission.json').read_text());require(result.returncode==3 and proof['device_creation_calls']==proof['gpu_resource_creations']==0,'physics reject before GPU '+name);passed(name)
        def command_bytes(clocks,controls):
            body=bytearray(struct.pack('<I',len(clocks)))
            for i,(clock,control) in enumerate(zip(clocks,controls)):body.extend(struct.pack('<QQII',i+1,clock,control,0))
            return body
        prefix=_package_envelope(zipped,m.name,42)
        for name,body,mode,status in (
            ('unknown_physics_control',command_bytes((0,2*STEP_NS),(0,4)),'--simulate','PACKAGE_COMMAND_REJECTED'),
            ('physics_control_in_parameter_mode',command_bytes((0,),(1,)),'--evaluate','PACKAGE_COMMAND_REJECTED'),
            ('late_backlog_discards_partial_output',command_bytes((0,1,1+8*STEP_NS),(1,1,1)),'--simulate','WORKER_REJECTED')):
            result=_run_envelope(prefix+body,42,mode)
            require(result.status==status and not result.packet and result.job_empty and result.profile_removed,'input/partial rejection '+name);passed(name)
        without=dict(files);manifest=json.loads(without[m.name]);del manifest['FileReferences']['Physics'];without[m.name]=json.dumps(manifest).encode()
        result=evaluate_zip(archive(without),m.name,[(1,0,[],1)],physics=True)
        require(result.status=='PACKAGE_PHYSICS_REJECTED' and not result.packet and result.job_empty and result.profile_removed,'missing required linked physics');passed('missing_linked_physics')
        report['verdict']='PASS'
    except Exception as error:
        report.update(verdict='FAIL',reason=type(error).__name__);(evidence/'failure.txt').write_text(str(error),encoding='utf-8')
    finally:
        after=capture();report['closure_after']=after;report['closure_unchanged']=before==after
        if before!=after:report.update(verdict='FAIL',reason='SOURCE_OR_BINARY_DRIFT')
        (evidence/'evidence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report


def main():
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--window',action='store_true');args=parser.parse_args()
    evidence=ROOT/'build/moc3-package-evidence'/uuid.uuid4().hex[:24];evidence.mkdir(parents=True)
    if args.window:
        from moc3_admission_observer import observe
        report=observe(lambda notify:run(evidence,notify),evidence,proof_kind='RETAINED_PHYSICS_SEQUENCE')
    else:report=run(evidence)
    print('PUB02_ISOLATED_PHYSICS_'+report['verdict']);print('EVIDENCE_RUN '+evidence.name)
    return 0 if report['verdict']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
