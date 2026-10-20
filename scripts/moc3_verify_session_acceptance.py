"""Run R1-R5 private local proof with unchanged public source/binary closure."""
import hashlib,json,struct,subprocess,sys,time
from pathlib import Path
from moc3_admission_provenance import ROOT,capture
from moc3_private_lease_fixture import private_fixture
from moc3_lease import Lease,Key


def retained(root):
    package,entry,_=private_fixture(root/'fixture');worker=Lease(package,entry,7,1,1);pid=worker.process.pid;accepted=b''
    try:
        for i in range(3):
            key=Key(7,1,1,i+1,i*33333334,i+1,i*33333334,i+1,worker.nonce,worker.model,hashlib.sha256(accepted).digest())
            packet=worker.prepare(key,[],1);assert packet[:8]==b'ARSCND02' and worker.process.pid==pid
            size,cp_size=struct.unpack_from('<QI',packet,160);accepted=packet[176+size:];assert len(accepted)==cp_size
            worker.accept(accepted)
        cleanup=worker.finish();assert cleanup['profile_removed'] and cleanup['job_empty'] and cleanup['broker_exit']==0
        (root/'retained.json').write_text(json.dumps({'result':'PASS','same_lease_commands':3,'cleanup':cleanup}))
    finally:worker.close()
    # Real stale ACCEPT must kill the speculative lease, never advance it.
    worker=Lease(package,entry,8,1,1)
    try:
        key=Key(8,1,1,1,0,1,0,1,worker.nonce,worker.model,hashlib.sha256(b'').digest())
        packet=worker.prepare(key,[],1);size,_=struct.unpack_from('<QI',packet,160);checkpoint=packet[176+size:];worker.accept(checkpoint)
        from dataclasses import replace
        next_key=replace(key,generation=2,revision=2,clock=33333334,local_sequence=2,local_clock=33333334,predecessor=hashlib.sha256(checkpoint).digest())
        worker.prepare(next_key,[],1)
        try:worker.send(4,key.wire()+hashlib.sha256(checkpoint).digest())
        except (EOFError,OSError):pass
        else:raise AssertionError('STALE_ACK_ACCEPTED')
        cleanup=worker.close();assert cleanup['profile_removed'] and cleanup['job_empty'] and cleanup['broker_exit']==16
        (root/'stale-ack.json').write_text(json.dumps({'result':'PASS','cleanup':cleanup}))
    finally:worker.close()



def main():
    root=ROOT/'build/alpha58-final-acceptance';root.mkdir(parents=True,exist_ok=True);packet={'started':time.time(),'before':capture(),'verdict':'FAIL'}
    try:
        retained(root)
        for script in ('moc3_verify_lease_caps.py','moc3_verify_session_lifecycle.py','moc3_verify_session_viewer.py'):
            with (root/(script+'.log')).open('wb') as log:
                subprocess.run([sys.executable,str(ROOT/'scripts'/script)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=240,check=True)
        packet['after']=capture();assert packet['before']==packet['after'],'CLOSURE_CHANGED_DURING_RUN'
        packet['verdict']='PASS'
    finally:
        packet['completed']=time.time();(root/'evidence.json').write_text(json.dumps(packet,indent=2))
    print('R1_R5_LOCAL_SOURCE_BINARY_CLOSURE_PASS')
if __name__=='__main__':main()
