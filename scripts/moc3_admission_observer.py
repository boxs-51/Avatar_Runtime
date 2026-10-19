"""Visible, authenticated local observer for the STATIC admission proof (no animation).

API and Tk consume the same immutable readback bytes. No asset upload/import endpoint.
"""
import hashlib
from http.server import BaseHTTPRequestHandler,HTTPServer
import json
import os
from pathlib import Path
import secrets
import threading
import time
import tkinter as tk
from moc3_test_harness import _harden_local_path


def observe(run,evidence):
    lock=threading.Lock();state={'phase':'RUNNING','last_case':'starting','cases_passed':0,
        'physics':'VALIDATED_NOT_SIMULATED','proof':'STATIC_DEFAULT_POSE','frames':{}}
    frames={};token=secrets.token_urlsafe(32);result={};ui_proof={}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*_args):pass
        def do_GET(self):
            if not secrets.compare_digest(self.headers.get('Authorization',''),'Bearer '+token):
                self.send_error(401);return
            with lock:
                if self.path=='/status':body=json.dumps(state).encode();kind='application/json'
                elif self.path in ('/frame/warp.ppm','/frame/hardware.ppm'):
                    frame=frames.get(self.path.split('/')[2].split('.')[0])
                    if frame is None:self.send_error(404);return
                    body=frame['ppm'];kind='image/x-portable-pixmap'
                else:self.send_error(404);return
            self.send_response(200);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(body)));self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(body)
    server=HTTPServer(('127.0.0.1',0),Handler)
    api=threading.Thread(target=server.serve_forever,daemon=True);api.start()
    descriptor=evidence/'observer-session.json'
    if not _harden_local_path(evidence):raise RuntimeError('observer folder ACL')
    fd=os.open(descriptor,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w',encoding='utf-8') as f:json.dump({'base_url':f'http://127.0.0.1:{server.server_port}','token':token},f)
    if not _harden_local_path(descriptor):raise RuntimeError('observer descriptor ACL')
    root=tk.Tk();root.title('Avatar Runtime - static public admission proof')
    root.geometry('1080x650')
    title=tk.Label(root,text='PNG / Physics3 admission -> independent drawable guard -> GPU',font=('Segoe UI',15));title.pack(pady=8)
    label=tk.Label(root,text='STATIC default pose | Physics VALIDATED, NOT SIMULATED',font=('Segoe UI',12));label.pack()
    progress=tk.Label(root,text='Starting isolated checks',font=('Consolas',10));progress.pack(pady=8)
    row=tk.Frame(root);row.pack();panels={};photos={}
    for backend in ('warp','hardware'):
        col=tk.Frame(row);col.pack(side=tk.LEFT,padx=4);tk.Label(col,text=backend.upper()).pack();panels[backend]=tk.Label(col,text='Waiting for independently validated readback',width=65,height=30);panels[backend].pack()
    def notify(case,frame=None):
        with lock:
            state['last_case']=case;state['cases_passed']+=frame is None
            if frame:
                backend=frame['backend'];frames[backend]=frame
                state['frames'][backend]={'width':512,'height':512,'ppm_sha256':hashlib.sha256(frame['ppm']).hexdigest(),
                    'rgba_sha256':hashlib.sha256(frame['rgba']).hexdigest(),'physics':'VALIDATED_NOT_SIMULATED','pose':'DEFAULT'}
    def work():
        try:result['report']=run(notify)
        except Exception as error:result['report']={'verdict':'FAIL','reason':type(error).__name__}
        with lock:state['phase']=result['report']['verdict']
    worker=threading.Thread(target=work);worker.start();finished_at=None
    root.protocol('WM_DELETE_WINDOW',lambda:None) # This one controlled proof run cleans up on completion.
    def refresh():
        nonlocal finished_at
        with lock:snapshot=json.loads(json.dumps(state));current=dict(frames)
        progress.configure(text=f"{snapshot['phase']} | {snapshot['cases_passed']} checks | {snapshot['last_case']}")
        for backend,frame in current.items():
            if backend not in photos:
                image=tk.PhotoImage(data=frame['ppm'],format='PPM');photos[backend]=image;panels[backend].configure(image=image,text='',width=512,height=512)
                rgb=frame['ppm'].split(b'\n',3)[3]
                samples_match=all(tuple(image.get(x,y))==tuple(rgb[(y*512+x)*3:(y*512+x)*3+3])
                                  for x,y in ((0,0),(64,64),(128,256),(256,256),(384,384),(511,511)))
                ui_proof[backend]={'mapped':bool(root.winfo_ismapped()),'ppm_sha256':hashlib.sha256(frame['ppm']).hexdigest(),
                    'width':image.width(),'height':image.height(),'photo_samples_match':samples_match}
        if not worker.is_alive():
            if finished_at is None:finished_at=time.monotonic()
            if time.monotonic()-finished_at>15:root.destroy();return
        root.after(100,refresh)
    root.after(100,refresh)
    try:root.mainloop();worker.join()
    finally:server.shutdown();server.server_close();api.join(timeout=2);descriptor.unlink(missing_ok=True)
    (evidence/'observer-ui.json').write_text(json.dumps({'frames':ui_proof,'final_state':state}),encoding='utf-8')
    return result['report']
