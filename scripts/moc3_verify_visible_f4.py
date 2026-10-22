"""Run the private F4 suite through a mapped window; never export private frames."""
import hashlib
import http.client
import json
from pathlib import Path
import sys
import time
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from moc3_harness_client import call,DESCRIPTOR

def check(ok,message):
    if not ok:raise RuntimeError(message)

def wait_run(rid,collect=False):
    deadline=time.monotonic()+100;cursor=0;events=[]
    while time.monotonic()<deadline:
        if collect:
            code,batch=call('GET',f'/v1/runs/{rid}/events?after={cursor}')
            check(code==200 and not batch['cursor_gap'],'bounded event cursor lost')
            events.extend(batch['events']);cursor=batch['next_seq']
        code,result=call('GET',f'/v1/runs/{rid}')
        if result.get('state')=='COMPLETE':
            if collect:
                _,batch=call('GET',f'/v1/runs/{rid}/events?after={cursor}')
                check(not batch['cursor_gap'],'final event cursor lost');events.extend(batch['events'])
            return result,events
        time.sleep(.05)
    raise RuntimeError('F4 terminal timeout')

_,status=call('GET','/v1/status')
check(status['ui_visible'],'mapped window required')
if status['session']:sid=status['session']['session_id']
else:
    code,session=call('POST','/v1/sessions',{'fixture_alias':'synthetic_t0','mode':'deterministic'})
    check(code==201,'create control session');sid=session['session_id']
code,run=call('POST',f'/v1/sessions/{sid}/runs',{'suite_id':'CORE-INTEGRATION'})
check(code==202,'start core integration');rid=run['run_id']
result,events=wait_run(rid,True)
check(result['verdict']=='PASS' and result['reason']=='TRUSTED_LOCAL_F4_CORE_HOST_VERIFIED' and
      result['process_tree_empty'],'actual core integration failed')
frames=[e for e in events if e['kind']=='HOST_FRAME_COMMITTED']
check([e['frame_generation'] for e in frames]==list(range(1,141)),'actual monotonic frame sequence')
check(all(e['core_generation']==e['frame_generation']==e['target_epoch'] for e in frames),'actual core identity')
_,meta=call('GET',f'/v1/sessions/{sid}/frames/latest')
check(meta['core_generation']==meta['frame_generation']==140 and meta['behavior']=='CORE_DISABLED','rejected target advertised')
folder=ROOT/'build/moc3-f4-local'/rid
trace=json.loads((folder/'warp/trace.json').read_text())
descriptor=json.loads(DESCRIPTOR.read_text())
for suffix in ('rgba','image'):
    connection=http.client.HTTPConnection('127.0.0.1',descriptor['port'],timeout=5)
    connection.request('GET',f'/v1/sessions/{sid}/frames/latest/{suffix}',headers={'X-Moc3-Test-Token':descriptor['token']})
    response=connection.getresponse();check(response.status==200,'actual bytes response')
    for header,key in (('X-Run-Id','run_id'),('X-Session-Id','session_id'),('X-Frame-Generation','frame_generation'),
                       ('X-Target-Epoch','target_epoch'),('X-Core-Generation','core_generation'),
                       ('X-Snapshot-Generation','snapshot_generation'),('X-Device-Generation','device_generation')):
        check(response.getheader(header)==str(meta[key]),'exact binary identity')
    raw=response.read();connection.close()
    if suffix=='rgba':check(hashlib.sha256(raw).hexdigest()==trace['trace'][-1]['clean_sha256'],'actual clean readback')
    else:check(raw==(folder/'warp/slot-0.ppm').read_bytes(),'actual accepted presenter')
time.sleep(.3);_,status=call('GET','/v1/status')
check(status['ui_visible'] and status['ui_frame']=={k:meta[k] for k in ('run_id','frame_generation','target_epoch','core_generation')},'mapped UI core frame')
print('F4_VISIBLE_REAL_CORE_API_PASS',flush=True)
_,run=call('POST',f'/v1/sessions/{sid}/runs',{'suite_id':'CORE-INTEGRATION'})
cancel_id=run['run_id'];deadline=time.monotonic()+30
while time.monotonic()<deadline:
    _,current=call('GET',f'/v1/runs/{cancel_id}')
    if (current.get('frame_generation') or 0)>=1:break
    check(current['state']!='COMPLETE','cancel missed active core worker');time.sleep(.03)
else:raise RuntimeError('no actual core frame before cancel')
call('POST',f'/v1/runs/{cancel_id}/cancel',{})
cancel,_=wait_run(cancel_id)
check(cancel['verdict']=='CANCELLED' and cancel['process_tree_empty'],'core worker descendant cleanup')
packet={'verdict':'PASS','full_run':result,'cancel_run':cancel,'frame':meta,'ui_frame':status['ui_frame'],
        'monotonic_event_sequence_verified':True,'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
(folder/'api-evidence.json').write_text(json.dumps(packet,indent=2))
print('F4_CORE_NATIVE_CANCEL_JOB_EMPTY_PASS',flush=True)
print('EVIDENCE_RUN '+rid,flush=True)
