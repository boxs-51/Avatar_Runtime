"""Separate agent-style API client driving one visible admission validation process."""
import hashlib
import argparse
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
    parser=argparse.ArgumentParser();group=parser.add_mutually_exclusive_group();group.add_argument('--retained',action='store_true');group.add_argument('--physics',action='store_true');group.add_argument('--publication',action='store_true');group.add_argument('--checkpoint',action='store_true');args=parser.parse_args()
    closure_before=capture() # Before child creation: all child modules load AFTER these pins.
    base_folder=ROOT/'build/moc3-package-evidence';previous=set(base_folder.glob('*/observer-session.json'))
    script='scripts/moc3_verify_checkpoint.py' if args.checkpoint else 'scripts/moc3_verify_packet_publication.py' if args.publication else 'scripts/moc3_verify_isolated_physics.py' if args.physics else 'scripts/moc3_verify_retained_session.py' if args.retained else 'scripts/moc3_verify_package_admission.py'
    process=subprocess.Popen([sys.executable,str(ROOT/script),'--window'],
                             stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,cwd=ROOT)
    deadline=time.monotonic()+180;folder=None;unauthorized=False;frames={};observed={};polls=0;phase='UNVERIFIED'
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
                def get(path,sequence=None):
                    req=urllib.request.Request(url+path,headers={'Authorization':'Bearer '+token,**({'X-Expected-Sequence':str(sequence)} if sequence is not None else {})})
                    with urllib.request.urlopen(req,timeout=3) as response:return response.read(2*1024*1024)
                status=json.loads(get('/status'));phase=status['phase'];polls+=1
                for backend,identity in status['frames'].items():
                    try:ppm=get('/frame/'+backend+'.ppm',identity.get('sequence',0))
                    except urllib.error.HTTPError as error:
                        if error.code==409:continue
                        raise
                    if hashlib.sha256(ppm).hexdigest()!=identity['ppm_sha256']:raise RuntimeError('API image identity')
                    frames[backend]=identity;observed.setdefault(backend,set()).add(identity.get('sequence',0))
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
        if args.retained or args.physics or args.publication or args.checkpoint:
            count=8 if args.physics or args.publication or args.checkpoint else 3
            ok=ok and all(ui['frames'][b].get('presentations',0)>=count and ui['frames'][b].get('sequence')==count and observed.get(b)==set(range(1,count+1)) for b in frames)
        record={'closure_before_child_creation':closure_before,'closure_after_child_exit':closure_after,'closure_unchanged':closure_before==closure_after,'verdict':'PASS' if ok else 'FAIL','unauthorized_401':unauthorized,'polls':polls,'final_phase':phase,'frames':frames,
                'ui_mapped_and_photo_samples_match':ok,'client_process_separate':True,'observed_sequences':{b:sorted(v) for b,v in observed.items()}}
        (folder/'observer-api.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
        print('PUBLIC_ADMISSION_VISIBLE_API_'+record['verdict']);print('EVIDENCE_RUN '+folder.name)
        return 0 if ok else 1
    finally:
        if process.poll() is None:process.kill();process.wait(timeout=5)
        process.stdout.close()

if __name__=='__main__':raise SystemExit(main())
