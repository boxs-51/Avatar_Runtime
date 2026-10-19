"""Actual core-authorized packet publication; private data stays under ignored build/."""
import io
import json
from pathlib import Path
import subprocess
import sys
import time
import uuid
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from moc3_admission_provenance import capture
from moc3_publication_probe import PublicationProbe,retained_state
from moc3_public_package import _prepare_evaluation,_run_envelope,render_packet
from moc3_admission_fixtures import archive
from moc3_verify_retained_session import require
from moc3_local_replay import _private_manifest
from moc3_physics import STEP_NS


def run(evidence,notify=lambda *_:None):
    before=capture();report={'kind':'PUB04_A_CORE_AUTHORIZED_PACKET_PUBLICATION','physics':'SIMULATED_BOUNDED_INDEPENDENT',
        'whole_PUB02':'HOLD','whole_PUB04':'HOLD','cases':[],'closure_before':before}
    def passed(name):
        report['cases'].append({'case':name,'result':'PASS'});notify(name,None)
        (evidence/'evidence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    try:
        result=subprocess.run([sys.executable,str(ROOT/'tests/moc3/test_packet_publication.py')],capture_output=True,timeout=120)
        require(result.returncode==0 and b'OK' in result.stderr,'generated publication regressions');passed('generated_publication_regressions')
        from PIL import Image
        from moc3_package_admission import admit_package
        from moc3_local_physics import Bridge
        manifest=_private_manifest();require(manifest is not None,'authorized fixture');files={manifest.name:manifest.read_bytes()};refs=json.loads(manifest.read_bytes())['FileReferences']
        for name in (refs['Moc'],refs['Physics']):files[name]=(manifest.parent/name).read_bytes()
        for name in refs['Textures']:
            with Image.open(manifest.parent/name) as image:
                image=image.convert('RGBA');image.thumbnail((512,512),Image.Resampling.LANCZOS);buffer=io.BytesIO();image.save(buffer,format='PNG');files[name]=buffer.getvalue()
        fixture=evidence/'explicit-512-derivative';fixture.mkdir()
        for name,data in files.items():
            path=fixture/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        baseline=admit_package(fixture,manifest.name);bridge=Bridge(baseline,evidence/'trusted-catalog',False)
        try:
            catalog=dict(bridge.catalog);keys=list(catalog);key=next(k for k in ('ParamAngleX','ParamAngleY','ParamBodyAngleX') if k in catalog and catalog[k][2]>catalog[k][1]);slot=keys.index(key);bridge.finish()
        finally:bridge.close()
        controls=(0,1,1,1,0,3,1,0)
        frames=[(i+1,i*2*STEP_NS,[(slot,.75)] if i==1 else [],control) for i,control in enumerate(controls)]
        payload,nonce,mode=_prepare_evaluation(archive(files),manifest.name,frames,physics=True)
        packet=None
        for hardware in (False,True):
            backend='hardware' if hardware else 'warp';owner=PublicationProbe(evidence/'published'/backend,hardware)
            try:
                # This actual core grant exists BEFORE worker evaluation begins.
                require(owner.begin(1,nonce,0,0)==['PREPARED'],'actual core preparation before worker output')
                if packet is None:
                    admitted=_run_envelope(payload,nonce,mode)
                    require(admitted.status=='PACKET_CUSTODY' and admitted.profile_removed and admitted.job_empty,'actual isolated physics admission');packet=admitted.packet
                    (evidence/'sequence.bin').write_bytes(packet);passed('live_core_grant_precedes_actual_worker_admission')
                for i,(_,clock,_,_) in enumerate(frames):
                    if i:require(owner.begin(i+1,nonce,i,clock,.75 if i==1 else 0)==['PREPARED'],'complete authorized request')
                    require(owner.stage(packet)==['STAGED'],'independently verified GPU candidate')
                    prior=owner.state();require(owner.command('DELAY_ONCE')==['ARMED'] and owner.command('POLL')==['BUSY'],'not query complete yet')
                    require(retained_state(prior,owner.state()),'BUSY cannot publish core/image/cursor')
                    require(owner.poll()==['PUBLISHED'],'exact query and real final core fence')
                    state=owner.state();require([state[k] for k in ('core_generation','host_generation','epoch','physics_cursor')]==[i+1,i+1,i+1,i+2],'atomic published identities')
                    rgba,ppm=owner.readback();out=evidence/'reference'/backend/str(i)
                    require(render_packet(packet,nonce,out,hardware=hardware,sequence_index=i).returncode==0,'qualified packet renderer reference')
                    require(rgba==(out/'frame.rgba').read_bytes(),'same backend exact publication pixels')
                    notify('authorized_publication_'+backend,{'backend':backend,'ppm':ppm,'rgba':rgba,'sequence':i+1,'pose':'CORE_AUTHORIZED_PHYSICS' if controls[i]&1 else 'CORE_AUTHORIZED_OFF'})
                    time.sleep(.6)
                owner.finish();passed('authorized_query_core_cursor_pixels_'+backend)
            finally:owner.close()
            for case in ('worker_crash','cancel_before_stage','cancel_after_stage','late_revoke','expired_before_stage','expired_after_stage','bad_reload','stale_nonce','bad_clock','gpu_allocation'):
                owner=PublicationProbe(evidence/'negatives'/backend/case,hardware)
                try:
                    require(owner.begin(1,nonce,0,0)==['PREPARED'] and owner.stage(packet)==['STAGED'] and owner.poll()==['PUBLISHED'],'negative case actual baseline')
                    previous=owner.state();pixels=owner.readback()[0]
                    requested_nonce=nonce^1 if case=='stale_nonce' else nonce
                    clock=2*STEP_NS+(1 if case=='bad_clock' else 0)
                    require(owner.begin(2,requested_nonce,1,clock,.75,100 if case.startswith('expired') else 2000)==['PREPARED'],'negative case live grant')
                    if case=='worker_crash':
                        failure=_run_envelope(b'',nonce,'--crash');require(failure.status=='WORKER_REJECTED' and not failure.packet and failure.profile_removed and failure.job_empty,'actual isolated worker crash')
                        require(owner.command('CANCEL')==['CANCELLED'],'failed worker cancels grant')
                    elif case=='cancel_before_stage':require(owner.command('CANCEL')==['CANCELLED'],'cancel unused grant')
                    else:
                        if case=='expired_before_stage':time.sleep(.12)
                        staged=owner.stage(packet[:-1] if case=='bad_reload' else packet,fail_target=case=='gpu_allocation')
                        if case in ('bad_reload','stale_nonce','bad_clock','gpu_allocation','expired_before_stage'):require(staged==['REJECTED'],'pre-publication rejection')
                        else:
                            require(staged==['STAGED'],'pending negative GPU transaction')
                            if case=='cancel_after_stage':require(owner.command('CANCEL')==['CANCELLED'],'cancel submitted grant')
                            if case=='late_revoke':require(owner.command('REVOKE_READY')==['ARMED'],'arm revoke at real query readiness')
                            if case=='expired_after_stage':time.sleep(.12)
                            require(owner.poll()==['REJECTED'],'final fence rejects')
                    after=owner.state();require(retained_state(previous,after) and owner.readback()[0]==pixels,'previous core/texture/cursor/pixels retained')
                    require(after['terminal'] and owner.begin(3,nonce,2,4*STEP_NS)==['DENIED'],'no silent authority remint')
                    if case in ('bad_reload','stale_nonce','bad_clock','expired_before_stage','gpu_allocation'):
                        require(after['gpu_creations']==previous['gpu_creations'],'rejection before new drawable GPU creation')
                    if case=='late_revoke':require(after['ready_calls']>previous['ready_calls'],'late revoke follows actual S_OK query')
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
        report=observe(lambda notify:run(evidence,notify),evidence,proof_kind='CORE_AUTHORIZED_PUBLICATION')
    else:report=run(evidence)
    print('PUB04_PACKET_PUBLICATION_'+report['verdict']);print('EVIDENCE_RUN '+evidence.name)
    return 0 if report['verdict']=='PASS' else 1
if __name__=='__main__':raise SystemExit(main())
