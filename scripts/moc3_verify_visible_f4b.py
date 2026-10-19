"""Actual private CORE-LIVE API/window checks; only coarse results leave build/."""
import hashlib
import http.client
import json
from pathlib import Path
import sys
import time
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from moc3_harness_client import call,DESCRIPTOR


def check(value,message):
    if not value:raise RuntimeError(message)


def wait(predicate,seconds=35):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        value=predicate()
        if value:return value
        time.sleep(.04)
    raise RuntimeError('live witness timeout')


def raw(sid):
    d=json.loads(DESCRIPTOR.read_text(encoding='utf-8'))
    c=http.client.HTTPConnection('127.0.0.1',d['port'],timeout=5)
    c.request('GET',f'/v1/sessions/{sid}/frames/latest/rgba',headers={'X-Moc3-Test-Token':d['token']})
    r=c.getresponse();check(r.status==200,'actual native bytes');data=r.read()
    identity={k:r.getheader(k) for k in ('X-Run-Id','X-Core-Generation','X-Frame-Generation')};c.close()
    return hashlib.sha256(data).hexdigest(),identity


def main():
    _,status=call('GET','/v1/status');check(status['ui_visible'],'mapped window required')
    if status['session']:sid=status['session']['session_id']
    else:
        code,session=call('POST','/v1/sessions',{'fixture_alias':'synthetic_t0','mode':'live'})
        check(code==201,'create control container');sid=session['session_id']
    code,run=call('POST',f'/v1/sessions/{sid}/runs',{'suite_id':'CORE-LIVE'});check(code==202,'start persistent worker')
    rid=run['run_id']
    def current():return call('GET',f'/v1/runs/{rid}')[1]
    wait(lambda:(v:=current()).get('frame_generation') and v)
    def control(seq,drive=None,physics=False,reset=False,lease_ms=2000):
        code,value=call('POST',f'/v1/runs/{rid}/controls',dict(sequence=seq,drive=drive,physics=physics,reset=reset,lease_ms=lease_ms))
        check(code==202 and value['state']=='QUEUED','queue distinct from rendered')
        wait(lambda:current().get('accepted_control_sequence')==seq)
        return value
    control(1,reset=True);baseline,_=raw(sid)
    bad={'sequence':2,'drive':2,'physics':False,'reset':False,'lease_ms':1000}
    check(call('POST',f'/v1/runs/{rid}/controls',bad)[0]==400,'out of range rejected')
    bad['drive']=.5;bad['sequence']=1
    check(call('POST',f'/v1/runs/{rid}/controls',bad)[0]==409,'replayed control rejected')
    control(2,drive=.5,lease_ms=1000);driven,identity=raw(sid)
    check(driven!=baseline and identity['X-Run-Id']==rid and identity['X-Core-Generation']==identity['X-Frame-Generation'],
          'actual changed native pose and core identity')
    def released():
        code,frame=call('GET',f'/v1/sessions/{sid}/frames/latest')
        return frame if code==200 and frame['behavior']=='CORE_RELEASE' else None
    wait(released,5)
    control(3,reset=True);reset_bytes,_=raw(sid);check(reset_bytes==baseline,'native default reset reproducible')
    control(4,drive=.5,physics=True);wait(lambda:call('GET',f'/v1/status')[1]['ui_frame'])
    time.sleep(.3);_,status=call('GET','/v1/status')
    check(status['ui_visible'] and status['ui_frame']['run_id']==rid and status['ui_frame']['core_generation']>=1,'actual visible core frame')
    code,batch=call('GET',f'/v1/runs/{rid}/events?after=0');check(code==200 and not batch['cursor_gap'],'bounded event proof')
    acks=[e for e in batch['events'] if e['kind']=='CONTROL_COMMITTED']
    check([e['control_sequence'] for e in acks]==[1,2,3,4],'joint committed control witnesses')
    call('POST',f'/v1/runs/{rid}/cancel',{})
    final=wait(lambda:(v:=current())['state']=='COMPLETE' and v)
    check(final['verdict']=='CANCELLED' and final['process_tree_empty'],'owned worker cancellation')
    check(call('POST',f'/v1/runs/{rid}/controls',dict(sequence=5,drive=None,physics=True,reset=False,lease_ms=100))[0]==409,'closed control rejected')
    _,new=call('POST',f'/v1/sessions/{sid}/runs',{'suite_id':'CORE-LIVE'});new_id=new['run_id']
    check(new_id!=rid,'fresh run identity')
    wait(lambda:call('GET',f'/v1/runs/{new_id}')[1].get('frame_generation'))
    check(call('POST',f'/v1/runs/{rid}/controls',dict(sequence=6,drive=.5,physics=True,reset=False,lease_ms=100))[0]==404,'old run cannot drive new worker')
    _,fresh=call('GET',f'/v1/sessions/{sid}/frames/latest')
    check(fresh['run_id']==new_id and fresh['behavior']=='CORE_RELEASE','fresh owner has no held input')
    call('POST',f'/v1/runs/{new_id}/cancel',{})
    second=wait(lambda:(v:=call('GET',f'/v1/runs/{new_id}')[1])['state']=='COMPLETE' and v)
    check(second['verdict']=='CANCELLED' and second['process_tree_empty'],'fresh worker cleanup')
    packet={'verdict':'PASS','run':final,'fresh_run':second,'control_events':acks,'visible':True,
            'drive_changes_pixels':True,'expiry_releases':True,'reset_default_exact':True,
            'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (ROOT/'build/moc3-f4b-local'/rid/'api-evidence.json').write_text(json.dumps(packet,indent=2),encoding='utf-8')
    print('F4B_VISIBLE_NATIVE_CONTROL_EXPIRY_RESET_CANCEL_PASS')
    print('EVIDENCE_RUN '+rid)


if __name__=='__main__':main()
