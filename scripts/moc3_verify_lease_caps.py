"""Real generated-only AppContainer cumulative lease budget/cleanup probes."""
import json,sys,subprocess,shutil,uuid,time
from pathlib import Path
from moc3_public_preflight import profile_absent
from moc3_public_package import ISOLATED_BIN
def main():
    root=Path('build/alpha58-r1-caps');root.mkdir(parents=True,exist_ok=True);results=[]
    for mode in ('M','C','W','O'):
     folder=root/uuid.uuid4().hex;folder.mkdir();worker=folder/'worker.exe';shutil.copyfile(ISOLATED_BIN/'ars_moc3_lease_fault_worker.exe',worker);(folder/'worker-data.txt').write_bytes(b'sentinel')
     profile='AvatarMocImport-'+uuid.uuid4().hex;began=time.monotonic()
     p=subprocess.run([str(ISOLATED_BIN/'ars_moc3_import_sandbox.exe'),str(worker.resolve()),profile,'--lease'],input=mode.encode(),capture_output=True,timeout=15)
     state=json.loads((folder/'launcher-state.json').read_text());assert profile_absent(profile)
     assert (mode=='M' and p.returncode==0 and p.stdout==b'DENIED') or (p.returncode==16 and ((mode=='C' and state['worker_user_cpu_100ns']>=19000000) or (mode=='W' and state['wait_result']==258) or (mode=='O' and state['output_overflow'] is True)))
     results.append({'case':mode,'result':'PASS','elapsed':time.monotonic()-began,'profile_removed':True})
    folder=root/uuid.uuid4().hex;folder.mkdir();worker=folder/'worker.exe';shutil.copyfile(ISOLATED_BIN/'ars_moc3_lease_fault_worker.exe',worker);(folder/'worker-data.txt').write_bytes(b'sentinel')
    profile='AvatarMocImport-'+uuid.uuid4().hex
    p=subprocess.Popen([str(ISOLATED_BIN/'ars_moc3_import_sandbox.exe'),str(worker.resolve()),profile,'--lease'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
    p.stdin.write(b'O');p.stdin.close()
    # Deliberately leave stdout open and unread until the broker has exited.
    code=p.wait(timeout=8);state=json.loads((folder/'launcher-state.json').read_text());assert code==16 and state['forwarding_failed'] is True and profile_absent(profile)
    p.stdout.close();results.append({'case':'UNREAD_OUTPUT','result':'PASS','profile_removed':True})
    (root/'evidence.json').write_text(json.dumps(results));print('R1_REAL_LEASE_CAPS_UNREAD_PIPE_AND_CLEANUP_PASS')

if __name__=='__main__':main()
