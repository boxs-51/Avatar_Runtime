import sys,time,json,http.client
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from moc3_harness_client import call,DESCRIPTOR
def check(ok,why):
    if not ok: raise RuntimeError(why)
def wait_run(rid):
    deadline=time.monotonic()+55
    while time.monotonic()<deadline:
        code,r=call('GET',f'/v1/runs/{rid}')
        if r.get('state')=='COMPLETE': return r
        time.sleep(.05)
    raise RuntimeError('timeout')
code,s=call('POST','/v1/sessions',{'fixture_alias':'synthetic_t0','mode':'deterministic'})
if code==409:
    _,status=call('GET','/v1/status')
    s=status['session']
sid=s['session_id']
code,r=call('POST',f'/v1/sessions/{sid}/runs',{'suite_id':'PHYS-LINK'})
rid=r['run_id']; result=wait_run(rid)
check(result['verdict']=='PASS' and result['reason']=='TRUSTED_LOCAL_F3_LINKED_PHYSICS_VERIFIED' and result['process_tree_empty'],'F3 full run failed')
code,events=call('GET',f'/v1/runs/{rid}/events?after=0')
check([e['frame_generation'] for e in events['events'] if e['kind']=='HOST_FRAME_COMMITTED']==list(range(1,34)),'frame timeline')
code,meta=call('GET',f'/v1/sessions/{sid}/frames/latest')
check(meta.get('behavior')=='DISABLED','final physics state identity')
desc=json.loads(DESCRIPTOR.read_text())
for suffix,filename in [('rgba','frame-33.rgba'),('image','frame-33.ppm')]:
    conn=http.client.HTTPConnection('127.0.0.1',desc['port'],timeout=5)
    conn.request('GET',f'/v1/sessions/{sid}/frames/latest/{suffix}',headers={'X-Moc3-Test-Token':desc['token']})
    response=conn.getresponse()
    check(response.status==200,'binary status')
    for header,key in [('X-Run-Id','run_id'),('X-Session-Id','session_id'),('X-Frame-Generation','frame_generation'),('X-Target-Epoch','target_epoch'),('X-Snapshot-Generation','snapshot_generation'),('X-Device-Generation','device_generation')]:
        check(response.getheader(header)==str(meta[key]),'binary identity')
    check(response.read()==(ROOT/'build/moc3-f3-local'/rid/'warp'/filename).read_bytes(),'actual readback bytes')
    conn.close()
time.sleep(.4)
code,status=call('GET','/v1/status')
check(status['ui_visible'] and status['ui_frame']=={k:meta[k] for k in ('run_id','frame_generation','target_epoch')},'visible UI exact frame')
print('F3_VISIBLE_API_READBACK_PASS',flush=True)
code,r=call('POST',f'/v1/sessions/{sid}/runs',{'suite_id':'PHYS-LINK'})
cancel_id=r['run_id'];deadline=time.monotonic()+50
while time.monotonic()<deadline:
    _,current=call('GET',f'/v1/runs/{cancel_id}')
    if (current.get('frame_generation') or 0)>=1: break
    check(current.get('state')!='COMPLETE','cancel missed active native renderer')
    time.sleep(.02)
else: raise RuntimeError('no live frame for cancellation')
call('POST',f'/v1/runs/{cancel_id}/cancel',{})
cancel=wait_run(cancel_id)
check(cancel['verdict']=='CANCELLED' and cancel['process_tree_empty'],'cancel descendants')
packet={'full_run':result,'frame':meta,'ui_visible':status['ui_visible'],'ui_frame':status['ui_frame'],'cancel_run':cancel,'verdict':'PASS'}
(ROOT/'build/moc3-f3-local'/rid/'api-evidence.json').write_text(json.dumps(packet,indent=2))
print('F3_LIVE_NATIVE_CANCEL_JOB_EMPTY_PASS',flush=True)
print('EVIDENCE_RUN '+rid,flush=True)
