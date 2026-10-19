"""Bounded retained evaluator diagnostic; NOT long-lived public control or physics simulation."""
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import time
import uuid
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from moc3_public_package import evaluate_zip,_package_envelope,_run_envelope,validate_packet,render_packet,ISOLATED_BIN
from moc3_admission_fixtures import archive,drawable_packet
from moc3_admission_provenance import capture
from moc3_local_replay import _private_manifest


def require(value,message):
    if not value:raise RuntimeError(message)


def commands(last_value=.75):
    out=bytearray(struct.pack('<I',3))
    for i,bindings in enumerate(([],[(0,last_value)],[])):
        out.extend(struct.pack('<QQII',i+1,i*33333334,0,len(bindings)))
        for slot,value in bindings:out.extend(struct.pack('<If',slot,value))
    return bytes(out)


def offsets(packet):
    count=struct.unpack_from('<I',packet,12)[0];require(1<=count<=8,'count');position=32;records=[]
    for _ in range(count):
        sequence,clock,length=struct.unpack_from('<QQQ',packet,position);records.append((position,position+24,length));position+=24+length
    require(position==len(packet),'whole envelope');return records


def vertex_positions(packet):
    """Diagnostic XY-only witness, called after independent native validation.

    Excludes opacity, color, UVs, metadata and atlas pixels so appearance-only
    changes cannot satisfy RET-04 geometry movement.
    """
    positions=[]
    for _,start,length in offsets(packet):
        na,nd=struct.unpack_from('<II',packet,start+40);position=start+48;frame=[]
        for _ in range(na):
            size=struct.unpack_from('<Q',packet,position+8)[0];position+=16+size
        for _ in range(nd):
            nv,ni,nm=struct.unpack_from('<III',packet,position+56);position+=68
            frame.append(tuple(struct.unpack_from('<ff',packet,position+i*16) for i in range(nv)))
            position+=nv*16+ni*2+nm*4
        require(position==start+length,'geometry diagnostic framing');positions.append(tuple(frame))
    return positions


def require_geometry_motion(packet):
    geometry=vertex_positions(packet)
    require(len(geometry)==3 and geometry[0]!=geometry[1] and geometry[0]==geometry[2],
            'actual XY geometry driven and released')


def run(evidence,notify=lambda *_:None):
    before=capture();report={'kind':'PUB02_A_BOUNDED_RETAINED_SEQUENCE','physics':'VALIDATED_NOT_SIMULATED',
                            'whole_PUB02':'HOLD','PUB04':'HOLD','cases':[],'closure_before':before}
    def passed(name):
        report['cases'].append({'case':name,'result':'PASS'});notify(name,None)
        (evidence/'evidence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    try:
        native=subprocess.run([str(ISOLATED_BIN/'ars_moc3_sequence_tests.exe')],capture_output=True,timeout=15)
        require(native.returncode==0 and b'SEQUENCE_PACKET_PASS' in native.stdout,'native sequence corpus');passed('native_sequence_corpus')
        from PIL import Image
        m=_private_manifest();require(m is not None,'authorized local fixture')
        files={m.name:m.read_bytes()};refs=json.loads(m.read_bytes())['FileReferences']
        for name in [refs['Moc'],refs['Physics']]:files[name]=(m.parent/name).read_bytes()
        for name in refs['Textures']:
            with Image.open(m.parent/name) as image:
                image=image.convert('RGBA');image.thumbnail((1024,1024),Image.Resampling.LANCZOS);buf=io.BytesIO();image.save(buf,format='PNG');files[name]=buf.getvalue()
        fixture=evidence/'test-derivative';fixture.mkdir()
        for name,raw in files.items():
            p=fixture/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(raw)
        # Trusted-local baseline obtains the real catalog order; IDs never cross the public worker wire.
        from moc3_package_admission import admit_package
        from moc3_local_physics import Bridge
        baseline=admit_package(fixture,m.name)
        reference=evidence/'catalog-reference';reference.mkdir();bridge=Bridge(baseline,reference,False)
        try:
            catalog=list(bridge.catalog.items())
            # The first catalog entry may control appearance only. Select a
            # conventional motion binding explicitly; the XY oracle below must
            # still prove movement, never infer it from the parameter name.
            key=next(name for name in ('ParamAngleX','ParamAngleY','ParamBodyAngleX')
                     if name in bridge.catalog and bridge.catalog[name][2]>bridge.catalog[name][1])
            slot=next(i for i,(name,_) in enumerate(catalog) if name==key);bounds=bridge.catalog[key]
            bridge.finish()
        finally:bridge.close()
        request=[(1,0,[]),(2,33333334,[(slot,.75)]),(3,66666668,[])]
        zipped=archive(files);admitted=evaluate_zip(zipped,m.name,request)
        require(admitted.status=='PACKET_CUSTODY' and admitted.job_empty and admitted.profile_removed,'actual retained admission')
        require(validate_packet(admitted.packet,admitted.nonce,sequence=True).returncode==0,'independent sequence guard')
        require(validate_packet(admitted.packet,admitted.nonce).returncode==3,'static v1 refuses sequence')
        (evidence/'sequence.bin').write_bytes(admitted.packet);passed('real_retained_session_and_version_boundary')
        require_geometry_motion(admitted.packet);passed('default_driven_release_xy_geometry')
        # Repeated admission is a fresh Session: the released/default sequence is deterministic.
        again=evaluate_zip(zipped,m.name,request);require(again.status=='PACKET_CUSTODY','deterministic repeat')
        a=bytearray(admitted.packet);b=bytearray(again.packet)
        for packet in (a,b):
            struct.pack_into('<Q',packet,16,0)
            for _,start,_ in offsets(packet):struct.pack_into('<Q',packet,start+28,0)
        require(a==b,'same complete sequence excluding fresh request nonce');passed('deterministic_repeat')
        for hardware in (False,True):
            backend='hardware' if hardware else 'warp';ref=evidence/'reference'/backend;ref.mkdir(parents=True);bridge=Bridge(baseline,ref,hardware)
            actual=[];expected=[]
            try:
                for i,values in enumerate(({}, {key:bounds[1]+.75*(bounds[2]-bounds[1])},{})):
                    expected.append(bridge.frame(values)[0]);out=evidence/'render'/backend/str(i)
                    rendered=render_packet(admitted.packet,admitted.nonce,out,hardware=hardware,sequence_index=i)
                    require(rendered.returncode==0,'native sequence render '+backend)
                    rgba=(out/'frame.rgba').read_bytes();actual.append(rgba);require(rgba==expected[-1],'same backend exact sequence pixel parity '+backend)
                    notify('parameter_frame_'+backend,{'backend':backend,'ppm':(out/'frame.ppm').read_bytes(),'rgba':rgba,'sequence':i+1,'pose':'PARAMETER_DRIVEN' if i==1 else 'DEFAULT'})
                    time.sleep(.6)
                bridge.finish()
            finally:bridge.close()
            require(actual[0]!=actual[1] and actual[0]==actual[2],'driven/released actual pixels '+backend);passed('default_driven_release_'+backend)
        def reject_wire(name,packet,nonce=None):
            expected_nonce=admitted.nonce if nonce is None else nonce
            require(validate_packet(packet,expected_nonce,sequence=True).returncode==3,'sequence guard '+name)
            out=evidence/'wire-rejections'/name;r=render_packet(packet,expected_nonce,out,sequence_index=0)
            proof=json.loads((out/'admission.json').read_text());require(r.returncode==3 and proof['device_creation_calls']==0 and proof['gpu_resource_creations']==0,'before any GPU '+name);passed('wire_'+name)
        reject_wire('cancelled_nonce',admitted.packet,admitted.nonce^1)
        reject_wire('truncated_final',admitted.packet[:-1]);reject_wire('trailing_bytes',admitted.packet+b'x')
        records=offsets(admitted.packet);last,start,_=records[-1]
        changes={'late_sequence':(last,1,'<Q'),'late_clock':(last+8,0,'<Q'),'late_generation':(start+20,2,'<Q'),
                 'late_nan_opacity':(start+0,0,'<I')}
        # Find final draw header by walking only length-delimited atlas transport in the generated mutation builder.
        na=struct.unpack_from('<I',admitted.packet,start+40)[0];draw=start+48
        for _ in range(na):length=struct.unpack_from('<Q',admitted.packet,draw+8)[0];draw+=16+length
        changes['late_nan_opacity']=(draw+20,float('nan'),'<f')
        for name,(pos,value,fmt) in changes.items():
            malformed=bytearray(admitted.packet);struct.pack_into(fmt,malformed,pos,value);reject_wire(name,bytes(malformed))
        # All commands parse/bind before the first update/output: final-command failure returns NO candidate bytes.
        nonce=42;prefix=_package_envelope(zipped,m.name,nonce);valid_commands=bytearray(commands())
        # Generated binding uses a real slot for a valid early command, then corrupts an independent late field.
        struct.pack_into('<I',valid_commands,4+24+24,slot)
        bad=[]
        value=bytearray(valid_commands);struct.pack_into('<Q',value,4+24+32,1);bad.append(('replayed_last_sequence',bytes(value)))
        value=bytearray(valid_commands);struct.pack_into('<Q',value,4+24+32+8,0);bad.append(('backwards_last_clock',bytes(value)))
        value=bytearray(valid_commands);struct.pack_into('<f',value,4+24+24+4,float('nan'));bad.append(('nonfinite_drive',bytes(value)))
        value=bytearray(valid_commands);struct.pack_into('<I',value,4+24+24,len(catalog));bad.append(('unknown_catalog_slot',bytes(value)))
        bad.append(('extra_command_bytes',bytes(valid_commands)+b'x'))
        for name,payload in bad:
            result=_run_envelope(prefix+payload,nonce,'--evaluate')
            require(result.status=='PACKAGE_COMMAND_REJECTED' and not result.packet and result.profile_removed and result.job_empty,'worker command '+name);passed('command_'+name)
        passed('all_commands_fail_before_candidate_output');report['verdict']='PASS'
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
        report=observe(lambda notify:run(evidence,notify),evidence,proof_kind='RETAINED_PARAMETER_SEQUENCE')
    else:report=run(evidence)
    print('PUB02_RETAINED_SEQUENCE_'+report['verdict']);print('EVIDENCE_RUN '+evidence.name)
    return 0 if report['verdict']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
