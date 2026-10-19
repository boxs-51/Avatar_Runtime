"""Separate agent-style API client driving one visible admission validation process."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
import urllib.error
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from moc3_admission_provenance import capture

def main():
    closure_before=capture() # Before child creation: all child modules load AFTER these pins.
    base_folder=ROOT/'build/moc3-package-evidence';previous=set(base_folder.glob('*/observer-session.json'))
    process=subprocess.Popen([sys.executable,str(ROOT/'scripts/moc3_verify_package_admission.py'),'--window'],
                             stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,cwd=ROOT)
    deadline=time.monotonic()+180;folder=None;unauthorized=False;frames={};polls=0;phase='UNVERIFIED'
    try:
        while time.monotonic()<deadline:
            candidates=set(base_folder.glob('*/observer-session.json'))-previous
            if candidates:
                descriptor=max(candidates,key=lambda p:p.stat().st_mtime);folder=descriptor.parent
                data=json.loads(descriptor.read_text(encoding='utf-8'));url=data['base_url'];token=data['token']
                if not unauthorized:
                    try:urllib.request.urlopen(url+'/status',timeout=2)
                    except urllib.error.HTTPError as error:
                        if error.code!=401:raise
                        unauthorized=True
                def get(path):
                    req=urllib.request.Request(url+path,headers={'Authorization':'Bearer '+token})
                    with urllib.request.urlopen(req,timeout=3) as response:return response.read(2*1024*1024)
                status=json.loads(get('/status'));phase=status['phase'];polls+=1
                for backend,identity in status['frames'].items():
                    ppm=get('/frame/'+backend+'.ppm')
                    if hashlib.sha256(ppm).hexdigest()!=identity['ppm_sha256']:raise RuntimeError('API image identity')
                    frames[backend]=identity
            if phase!='RUNNING' and folder is not None:break
            if process.poll() is not None:break
            time.sleep(.1)
        if time.monotonic()>=deadline:raise RuntimeError('observer test timeout')
        stdout=process.communicate(timeout=30)[0]
        if folder is None:raise RuntimeError('observer descriptor absent')
        ui=json.loads((folder/'observer-ui.json').read_text(encoding='utf-8'))
        ok=process.returncode==0 and phase=='PASS' and unauthorized and set(frames)=={'warp','hardware'}
        for backend,identity in frames.items():
            panel=ui['frames'][backend]
            ok=ok and panel['mapped'] and panel['photo_samples_match'] and panel['width']==panel['height']==512 and panel['ppm_sha256']==identity['ppm_sha256']
        closure_after=capture();ok=ok and closure_before==closure_after
        record={'closure_before_child_creation':closure_before,'closure_after_child_exit':closure_after,'closure_unchanged':closure_before==closure_after,'verdict':'PASS' if ok else 'FAIL','unauthorized_401':unauthorized,'polls':polls,'final_phase':phase,'frames':frames,
                'ui_mapped_and_photo_samples_match':ok,'client_process_separate':True}
        (folder/'observer-api.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
        print('PUBLIC_ADMISSION_VISIBLE_API_'+record['verdict']);print('EVIDENCE_RUN '+folder.name)
        return 0 if ok else 1
    finally:
        if process.poll() is None:process.kill();process.wait(timeout=5)
        process.stdout.close()

if __name__=='__main__':raise SystemExit(main())
