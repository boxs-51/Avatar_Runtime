"""Private mapped viewer and loopback agent API over the isolated native session.

No model parser or trusted-local evaluator in this path. Endpoints are local,
authenticated and owner-thread serialized; this is not a public SDK ABI.
"""
import base64,hmac,http.server,json,queue,secrets,threading,time,tkinter as tk
from pathlib import Path
from moc3_isolated_session import IsolatedSession

class BoundedServer(http.server.ThreadingHTTPServer):
    daemon_threads=True
    block_on_close=False
    def __init__(self,*args,**kwargs):
        self.slots=threading.BoundedSemaphore(8);self.connections=set();self.handlers=set();self.connection_lock=threading.Lock()
        super().__init__(*args,**kwargs)
    def get_request(self):
        connection,address=super().get_request();connection.settimeout(1);return connection,address
    def process_request(self,request,address):
        if not self.slots.acquire(blocking=False):request.close();return
        with self.connection_lock:self.connections.add(request)
        handler=threading.Thread(target=self.process_request_thread,args=(request,address),daemon=True)
        with self.connection_lock:
            self.handlers={thread for thread in self.handlers if thread.is_alive()};self.handlers.add(handler)
        try:handler.start()
        except BaseException:
            with self.connection_lock:self.connections.discard(request)
            with self.connection_lock:self.handlers.discard(handler)
            self.slots.release();request.close();raise
    def process_request_thread(self,request,address):
        try:super().process_request_thread(request,address)
        finally:
            with self.connection_lock:self.connections.discard(request)
            self.slots.release()
    def handle_error(self,request,address):
        import sys
        if not isinstance(sys.exc_info()[1],OSError):super().handle_error(request,address)
    def server_close(self):
        import socket
        with self.connection_lock:connections=list(self.connections)
        for connection in connections:
            try:connection.shutdown(socket.SHUT_RDWR)
            except OSError:pass
            connection.close()
        super().server_close()
        deadline=time.monotonic()+2
        with self.connection_lock:handlers=list(self.handlers)
        for handler in handlers:handler.join(timeout=max(0,deadline-time.monotonic()))
        self.shutdown_verified=all(not handler.is_alive() for handler in handlers)

class CommandMailbox:
    def __init__(self):
        self.commands=queue.Queue(maxsize=8);self.lock=threading.Lock();self.closed=False;self.waiters=set()
    def submit(self,path,data):
        result=queue.Queue(maxsize=1)
        with self.lock:
            if self.closed:raise ValueError('COMMAND_ADMISSION_CLOSED')
            self.commands.put_nowait((path,data,result,time.monotonic()+2));self.waiters.add(result)
        return result
    def release(self,result):
        with self.lock:self.waiters.discard(result)
    def seal(self):
        with self.lock:self.closed=True
    def close(self):
        with self.lock:
            self.closed=True
            for result in self.waiters:
                try:result.put_nowait((409,{'error':'SESSION_CLOSED'}))
                except queue.Full:pass
            while True:
                try:self.commands.get_nowait()
                except queue.Empty:break


class Viewer:
    def __init__(self,package,entry,folder,hardware=False):
        self.folder=Path(folder);self.folder.mkdir(parents=True,exist_ok=True)
        self.session=IsolatedSession(package,entry,self.folder/'session',hardware)
        self.root=tk.Tk();self.root.title('Avatar Runtime - isolated session / local evidence')
        self.image=tk.Label(self.root);self.image.pack();self.label=tk.Label(self.root,text='Ready - isolated worker / native publication');self.label.pack()
        self.mailbox=CommandMailbox();self.commands=self.mailbox.commands;self.token=secrets.token_urlsafe(32);self.sequence=0;self.done=False;self.stopped=False;self.stop_requested=threading.Event();self.photo=None;self.presented=0;self.mapped=False;self.presented_generation=0
        viewer=self
        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version='HTTP/1.0'
            def log_message(self,*args):pass
            def do_POST(self):
                self.connection.settimeout(3)
                if self.headers.get('Host')!=f'127.0.0.1:{viewer.server.server_port}' or not hmac.compare_digest(self.headers.get('Authorization',''),'Bearer '+viewer.token):self.reply(403,{'error':'DENIED'});return
                try:
                    length=int(self.headers.get('Content-Length','0'))
                    if not 1<=length<=16384 or self.headers.get('Transfer-Encoding'):raise ValueError()
                    def unique(pairs):
                        result={}
                        for k,v in pairs:
                            if k in result:raise ValueError()
                            result[k]=v
                        return result
                    data=json.loads(self.rfile.read(length),object_pairs_hook=unique,parse_constant=lambda _:(_ for _ in ()).throw(ValueError()))
                    if type(data) is not dict or self.path not in ('/status','/update','/frame','/close','/revoke'):raise ValueError()
                    result=viewer.mailbox.submit(self.path,data)
                    try:code,body=result.get(timeout=15);self.reply(code,body)
                    finally:viewer.mailbox.release(result)
                except (ValueError,queue.Full,queue.Empty,OSError):self.reply(400,{'error':'INVALID_OR_BUSY'})
            def reply(self,code,body):
                raw=json.dumps(body).encode();self.send_response(code);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(raw)));self.send_header('Cache-Control','no-store');self.end_headers()
                try:self.wfile.write(raw)
                except OSError:pass
        self.server=BoundedServer(('127.0.0.1',0),Handler);self.server.timeout=.2
        self.thread=threading.Thread(target=self.server.serve_forever,kwargs={'poll_interval':.05},daemon=True);self.thread.start()
        self.endpoint={'url':f'http://127.0.0.1:{self.server.server_port}','token':self.token}
        (self.folder/'agent-endpoint.json').write_text(json.dumps(self.endpoint))
        self.root.protocol('WM_DELETE_WINDOW',self.stop);self.root.after(10,self.tick)
    def present(self):
        rgba,ppm=self.session.host.readback();photo=tk.PhotoImage(data=ppm,format='PPM')
        self.image.configure(image=photo);self.photo=photo;self.root.update_idletasks()
        assert self.root.winfo_ismapped() and self.image.winfo_ismapped()
        # Verify widget pixels against actual accepted native readback, not a second render.
        rgb=ppm.split(b'\n',3)[3]
        for y in range(0,512,31):
            for x in range(0,512,29):assert photo.get(x,y)==tuple(rgb[(y*512+x)*3:(y*512+x)*3+3])
        self.presented+=1;self.mapped=True;self.presented_generation=self.session.accepted.generation
        self.label.configure(text=f'Accepted {self.presented_generation} | epoch {self.session.epoch} | isolated worker | physics independent')
    def execute(self,path,data,deadline=None):
        if path in ('/status','/frame'):
            if data:raise ValueError()
            state=self.session.status();state.update(presented_generation=self.presented_generation,mapped=self.mapped,sequence=self.sequence)
            if path=='/frame':
                if not self.session.accepted:raise ValueError()
                state['ppm_base64']=base64.b64encode(self.session.host.readback()[1]).decode()
            return state
        keys={'session','package','generation','sequence'}
        if path=='/update':keys|={'bindings','control','advance_ns'}
        if set(data)!=keys or any(type(data[k]) is not int for k in ('session','package','generation','sequence')):raise ValueError()
        current=self.session.accepted.generation if self.session.accepted else 0
        if (data['session'],data['package'],data['generation'],data['sequence'])!=(self.session.session,self.session.package_revision,current,self.sequence+1):raise ValueError()
        if path=='/update':
            # Validate before consuming sequence or launching work.
            from moc3_lease import Key,EMPTY,command
            command(Key(1,1,1,1,0,1,0,1,1,bytes(32),EMPTY),data['bindings'],data['control'])
            if type(data['advance_ns']) is not int or not 1<=data['advance_ns']<=8*16_666_667:raise ValueError()
            remaining=int((deadline-time.monotonic())*1000) if deadline else 2000
            if remaining<100:raise ValueError()
            self.sequence+=1
            self.session.update(data['bindings'],control=data['control'],advance_ns=data['advance_ns'],lease_ms=min(2000,remaining));self.present()
            return self.execute('/status',{})
        self.sequence+=1
        if path=='/revoke':
            if self.session.host.command('LEASE_REVOKE')!=['REVOKED']:raise ValueError()
            self.session._reap()
            return {'revoked':True,'sequence':self.sequence}
        self.mailbox.seal();self.session.close();self.done=True
        return {'closed':True,'sequence':self.sequence}
    def tick(self):
        if self.stop_requested.is_set():self.stop();return
        try:
            path,data,result,deadline=self.commands.get_nowait()
        except queue.Empty:pass
        else:
            try:
                if time.monotonic()>deadline:raise ValueError()
                body=self.execute(path,data,deadline);result.put_nowait((200,body))
            except Exception:
                import traceback
                with (self.folder/'local-errors.log').open('a') as log:log.write(traceback.format_exc())
                result.put_nowait((409,{'error':'REQUEST_REJECTED','sequence':self.sequence}))
        if not self.done:self.root.after(10,self.tick)
        else:self.root.after(100,self.stop)
    def run(self):
        try:self.root.mainloop()
        finally:self.stop()
    def stop(self):
        if self.stopped:return
        self.stopped=True;self.mailbox.seal()
        try:
            if not self.done:self.session.close();self.done=True
        finally:
            # Atomically close admission before waking every in-flight waiter.
            self.mailbox.close()
            self.server.shutdown();self.server.server_close();self.thread.join(timeout=3)
            # Drop every Tcl-backed object on its creating thread before an
            # HTTP/test thread can release the last Viewer reference.
            try:
                self.image.configure(image='');self.photo=None
                self.image.destroy();self.label.destroy();self.root.destroy()
            finally:self.image=None;self.label=None;self.root=None
            if not self.server.shutdown_verified:raise RuntimeError('HTTP_SHUTDOWN_UNVERIFIED')

if __name__=='__main__':
    from moc3_private_lease_fixture import private_fixture
    root=Path('build/alpha58-viewer-manual');package,entry,_=private_fixture(root/'fixture')
    Viewer(package,entry,root).run()
