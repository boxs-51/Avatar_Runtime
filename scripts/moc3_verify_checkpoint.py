"""Bounded actual worker restore/ACK publication. Private data remains ignored."""
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
from moc3_publication_probe import PublicationProbe,retained_state
from moc3_public_package import _prepare_step,_run_envelope,evaluate_zip,render_packet,ISOLATED_BIN
from moc3_admission_fixtures import archive
from moc3_verify_retained_session import require
from moc3_local_replay import _private_manifest
from moc3_physics import STEP_NS


def run(evidence,notify=lambda *_:None):
    before=capture();report={'kind':'PUB04_B_BOUNDED_CHECKPOINT_RESTORE_ACK','whole_PUB02':'HOLD','whole_PUB04':'HOLD','cases':[],'closure_before':before}
    def passed(name):
        report['cases'].append({'case':name,'result':'PASS'});notify(name,None)
        (evidence/'evidence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    def worker(prepared):
        payload,nonce,mode,_=prepared;result=_run_envelope(payload,nonce,mode)
        require(result.status=='PACKET_CUSTODY' and result.job_empty and result.profile_removed,'step custody/cleanup');return result
    try:
        result=subprocess.run([str(ISOLATED_BIN/'ars_moc3_checkpoint_tests.exe'),str(ROOT/'tests/moc3/generated_physics3_multichain.json')],capture_output=True,timeout=20)
        require(result.returncode==0 and b'CHECKPOINT_RESTORE_AND_WIRE_PASS' in result.stdout,'native checkpoint tests');passed('native_numeric_restore_and_wire')
        result=subprocess.run([sys.executable,str(ROOT/'tests/moc3/test_step_publication.py')],capture_output=True,timeout=120)
        require(result.returncode==0 and b'OK' in result.stderr,'generated checkpoint publication');passed('generated_checkpoint_publication')
        from PIL import Image
        from moc3_package_admission import admit_package
        from moc3_local_physics import Bridge
        m=_private_manifest();require(m is not None,'authorized local fixture');files={m.name:m.read_bytes()};refs=json.loads(m.read_bytes())['FileReferences']
        for name in (refs['Moc'],refs['Physics']):files[name]=(m.parent/name).read_bytes()
        for name in refs['Textures']:
            with Image.open(m.parent/name) as image:
                image=image.convert('RGBA');image.thumbnail((512,512),Image.Resampling.LANCZOS);buffer=io.BytesIO();image.save(buffer,format='PNG');files[name]=buffer.getvalue()
        fixture=evidence/'explicit-512-derivative';fixture.mkdir()
        for name,data in files.items():
            p=fixture/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
        baseline=admit_package(fixture,m.name);bridge=Bridge(baseline,evidence/'catalog',False)
        try:
            catalog=dict(bridge.catalog);keys=list(catalog);key=next(k for k in ('ParamAngleX','ParamAngleY','ParamBodyAngleX') if k in catalog and catalog[k][2]>catalog[k][1]);slot=keys.index(key);bridge.finish()
        finally:bridge.close()
        controls=(0,1,1,1,0,3,1,0);frames=[(i+1,i*2*STEP_NS,[(slot,.75)] if i==1 else [],c) for i,c in enumerate(controls)]
        zipped=archive(files);continuous=evaluate_zip(zipped,m.name,frames,physics=True)
        require(continuous.status=='PACKET_CUSTODY' and continuous.job_empty and continuous.profile_removed,'continuous isolated reference')
        first=None;second=None
        for hardware in (False,True):
            backend='hardware' if hardware else 'warp';owner=PublicationProbe(evidence/'resumed'/backend,hardware);states=[]
            try:
                for i,(seq,clock,bindings,control) in enumerate(frames):
                    previous=owner.checkpoint();prepared=_prepare_step(zipped,m.name,seq,clock,bindings,control,previous);_,nonce,_,model=prepared
                    require(owner.begin_step(seq,nonce,clock,model,.75 if i==1 else 0)==['PREPARED'],'core request before actual resumed worker')
                    candidate=worker(prepared);old=owner.state();require(owner.stage_step(candidate.packet)==['STAGED'],'typed candidate/ACK guard')
                    require(owner.checkpoint()==previous and retained_state(old,owner.state()),'checkpoint not committed before query/fence')
                    require(owner.poll()==['PUBLISHED'],'query/finalcore/checkpoint publication')
                    current=owner.checkpoint();states.append(current)
                    require(struct.unpack_from('<QQ',current,16)==(seq,clock),'actual accepted checkpoint identity')
                    rgba,ppm=owner.readback();out=evidence/'continuous'/backend/str(i)
                    require(render_packet(continuous.packet,continuous.nonce,out,hardware=hardware,sequence_index=i).returncode==0,'continuous reference render')
                    require(rgba==(out/'frame.rgba').read_bytes(),'resumed vs continuous exact pixels '+backend)
                    if i==0:first=(candidate,current,model)
                    if i==1:second=(candidate,current,model)
                    notify('checkpoint_ack_'+backend,{'backend':backend,'rgba':rgba,'ppm':ppm,'sequence':seq,'pose':'RESTORED_PHYSICS' if control&1 else 'RESTORED_OFF'})
                    time.sleep(.6)
                require(any(s[72:]!=states[0][72:] for s in states[1:4]),'actual particle/velocity change beyond clock fields')
                require(states[0][72:]==states[4][72:]==states[5][72:]==states[7][72:],'OFF/reset actual rest state')
                owner.finish();passed('actual_restored_state_and_continuous_pixels_'+backend)
            finally:owner.close()
            # Independent clean owners use the known first/second candidates as negative fixtures.
            first_packet,checkpoint,model=first;second_packet,_,_=second
            for case in ('bad_ack','malformed_state','cancel','revoke','expiry','worker_crash','wrong_package','stale_checkpoint'):
                owner=PublicationProbe(evidence/'negatives'/backend/case,hardware)
                try:
                    require(owner.begin_step(1,first_packet.nonce,0,model)==['PREPARED'] and owner.stage_step(first_packet.packet)==['STAGED'] and owner.poll()==['PUBLISHED'],'checkpoint baseline')
                    previous=owner.state();pixels=owner.readback()[0];require(owner.checkpoint()==checkpoint,'baseline exact state')
                    request=_prepare_step(zipped,m.name,2,2*STEP_NS,[(slot,.75)],1,checkpoint)
                    nonce=request[1] if case in ('worker_crash','wrong_package','stale_checkpoint') else second_packet.nonce
                    require(owner.begin_step(2,nonce,2*STEP_NS,model,.75,100 if case=='expiry' else 2000)==['PREPARED'],'negative grant')
                    if case in ('worker_crash','wrong_package','stale_checkpoint'):
                        if case=='worker_crash':result=_run_envelope(b'',nonce,'--crash');expected='WORKER_REJECTED'
                        elif case=='wrong_package':
                            changed=dict(files);changed[m.name]+=b'\n';payload,_,mode,_=_prepare_step(archive(changed),m.name,2,2*STEP_NS,[(slot,.75)],1,checkpoint)
                            # Bind envelope nonce to the already prepared request.
                            payload=bytearray(payload);struct.pack_into('<Q',payload,8,nonce);result=_run_envelope(bytes(payload),nonce,mode);expected='PACKAGE_COMMAND_REJECTED'
                        else:
                            bad=bytearray(checkpoint);struct.pack_into('<Q',bad,16,0);payload,_,mode,_=_prepare_step(zipped,m.name,2,2*STEP_NS,[(slot,.75)],1,bytes(bad));payload=bytearray(payload);struct.pack_into('<Q',payload,8,nonce)
                            result=_run_envelope(bytes(payload),nonce,mode);expected='PACKAGE_COMMAND_REJECTED'
                        require(result.status==expected and not result.packet and result.job_empty and result.profile_removed,'actual rejected worker/checkpoint')
                        require(owner.command('CANCEL')==['CANCELLED'],'failed worker cancels grant')
                    else:
                        packet=bytearray(second_packet.packet)
                        if case=='bad_ack':packet[88]^=1
                        if case=='malformed_state':
                            size=struct.unpack_from('<Q',packet,40)[0];struct.pack_into('<Q',packet,120+size+76,0x7ff8000000000000)
                        staged=owner.stage_step(bytes(packet))
                        if case in ('bad_ack','malformed_state'):require(staged==['REJECTED'],'reject before GPU')
                        else:
                            require(staged==['STAGED'],'pending checkpoint candidate')
                            if case=='cancel':owner.command('CANCEL')
                            elif case=='revoke':owner.command('REVOKE_READY')
                            else:time.sleep(.12)
                            require(owner.poll()==['REJECTED'],'final checkpoint fence rejection')
                    after=owner.state();require(retained_state(previous,after) and owner.checkpoint()==checkpoint and owner.readback()[0]==pixels,'exact checkpoint/core/image retention')
                    require(after['terminal'] and owner.begin_step(2,nonce,2*STEP_NS,model)==['DENIED'],'no authority remint')
                    owner.finish();passed(case+'_'+backend)
                finally:owner.close()
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
        report=observe(lambda notify:run(evidence,notify),evidence,proof_kind='CHECKPOINT_ACK_PUBLICATION')
    else:report=run(evidence)
    print('PUB04_CHECKPOINT_'+report['verdict']);print('EVIDENCE_RUN '+evidence.name)
    return 0 if report['verdict']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
