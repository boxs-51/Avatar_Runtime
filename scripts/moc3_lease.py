"""Private bounded isolated lease transport. No MOC/PNG/physics parsing in host."""
from dataclasses import dataclass,replace
import hashlib,json,math,queue,secrets,shutil,struct,subprocess,threading,uuid
from pathlib import Path
from moc3_public_package import ROOT,ISOLATED_BIN,PACKET_CAP,_package_envelope
from moc3_public_preflight import profile_absent

EMPTY=hashlib.sha256(b'').digest()
@dataclass(frozen=True)
class Key:
    session:int;package:int;epoch:int;generation:int;clock:int;local_sequence:int;local_clock:int;revision:int;nonce:int;model:bytes;predecessor:bytes
    def wire(self):
        values=(self.session,self.package,self.epoch,self.generation,self.clock,self.local_sequence,self.local_clock,self.revision,self.nonce)
        if any(type(v) is not int or not 0<=v<2**64 for v in values) or not all((self.session,self.package,self.epoch,self.generation,self.nonce)) or self.generation==2**64-1 or self.revision!=self.generation or not 1<=self.local_sequence<=8 or self.generation<self.local_sequence or not 0<=self.local_clock<=min(self.clock,1_000_000_000) or type(self.model) is not bytes or len(self.model)!=32 or type(self.predecessor) is not bytes or len(self.predecessor)!=32:
            raise ValueError('INVALID_LEASE_KEY')
        return struct.pack('<9Q',*values)+self.model+self.predecessor
    @classmethod
    def read(cls,value):
        if len(value)!=136:raise ValueError('INVALID_LEASE_KEY')
        key=cls(*struct.unpack_from('<9Q',value),value[72:104],value[104:136]);key.wire();return key

def checkpoint_key(value):
    if type(value) is not bytes or not 224<=len(value)<=128*1024 or value[:8]!=b'ARSPHY02' or struct.unpack_from('<II',value,144)!=(len(value)-152,0):raise ValueError('INVALID_CHECKPOINT_TRANSPORT')
    return Key.read(value[8:144]) # Custody only; independent native decoder must validate.

def command(key,bindings,control):
    if type(control) is not int or not 0<=control<=3 or type(bindings) not in (tuple,list) or len(bindings)>32:raise ValueError('INVALID_LEASE_CONTROL')
    data=bytearray(key.wire()+struct.pack('<II',control,len(bindings)));seen=set()
    for pair in bindings:
        if type(pair) not in (tuple,list) or len(pair)!=2:raise ValueError('INVALID_LEASE_BINDING')
        slot,value=pair
        if type(slot) is not int or not 0<=slot<4096 or slot in seen or type(value) not in (float,int) or not math.isfinite(value) or not -1<=value<=1:raise ValueError('INVALID_LEASE_BINDING')
        seen.add(slot);data.extend(struct.pack('<If',slot,value))
    return bytes(data)

class Lease:
    def __init__(self,package,entry,session,revision,epoch,checkpoint=b'',base_generation=0):
        self.owner=threading.get_ident();self.closed=False;self.pending=None;self.cleanup=None;self.responses=queue.Queue(maxsize=2)
        self.nonce=secrets.randbits(64) or 1;name=entry.encode('utf-8');self.model=hashlib.sha256(struct.pack('<I',len(name))+name+package).digest()
        if type(base_generation) is not int or not 0<=base_generation<2**64-2 or any(type(v) is not int or not 0<v<2**64 for v in (session,revision,epoch)) or type(checkpoint) is not bytes or len(checkpoint)>128*1024:raise ValueError('INVALID_LEASE_START')
        if checkpoint:
            previous=checkpoint_key(checkpoint)
            if (previous.session,previous.package,previous.model)!=(session,revision,self.model) or previous.epoch>=epoch:raise ValueError('INVALID_LEASE_ORIGIN')
        self.session,self.revision,self.epoch=session,revision,epoch
        self.folder=ROOT/'build/moc3-lease-trials'/uuid.uuid4().hex;self.folder.mkdir(parents=True)
        worker=self.folder/'worker.exe';shutil.copyfile(ISOLATED_BIN/'ars_moc3_import_worker.exe',worker);(self.folder/'worker-data.txt').write_bytes(b'lease read-only sentinel')
        self.profile='AvatarMocImport-'+uuid.uuid4().hex
        self.process=subprocess.Popen([str(ISOLATED_BIN/'ars_moc3_import_sandbox.exe'),str(worker),self.profile,'--lease'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,cwd=self.folder)
        def read_exact(n):
            value=bytearray()
            while len(value)<n:
                chunk=self.process.stdout.read(n-len(value))
                if not chunk:raise EOFError('LEASE_CLOSED')
                value.extend(chunk)
            return bytes(value)
        def read_loop():
            try:
                while True:
                    h=read_exact(24);magic,version,kind,size=struct.unpack('<8sIIQ',h)
                    if magic!=b'ARSLES02' or version!=2 or not 1<=kind<=7 or size>PACKET_CAP-24:raise ValueError('INVALID_LEASE_FRAME')
                    item=(kind,read_exact(size));self.responses.put_nowait(item)
            except BaseException as error:
                try:self.responses.put_nowait(error)
                except queue.Full:pass
                # Keep draining after malformed output so the broker can reap
                # its bounded job without a blocked stdout writer.
                try:
                    while self.process.stdout.read(16384):pass
                except (OSError,ValueError):pass
        self.reader=threading.Thread(target=read_loop,daemon=True);self.reader.start()
        try:
            self.process.stdin.write(_package_envelope(package,entry,self.nonce)+struct.pack('<QQQQI',session,revision,epoch,base_generation,len(checkpoint))+checkpoint);self.process.stdin.flush()
            kind,ready=self.response()
            if kind!=1 or len(ready)<100 or ready[:32]!=self.model or ready[32:64]!=hashlib.sha256(checkpoint).digest() or struct.unpack_from('<QQQQ',ready,64)!=(session,revision,epoch,base_generation):raise ValueError('INVALID_LEASE_READY')
            count=struct.unpack_from('<I',ready,96)[0]
            if not count<=4096 or len(ready)!=100+count*12:raise ValueError('INVALID_LEASE_CATALOG')
            self.catalog=tuple(struct.unpack_from('<fff',ready,100+i*12) for i in range(count))
            if any(not all(math.isfinite(v) for v in row) or not -1e6<=row[0]<=row[1]<=row[2]<=1e6 for row in self.catalog):raise ValueError('INVALID_LEASE_CATALOG')
        except BaseException:
            self.close();raise
    def _owner(self):
        if threading.get_ident()!=self.owner:raise RuntimeError('WRONG_THREAD')
        if self.closed:raise RuntimeError('LEASE_CLOSED')
    def response(self):
        try:item=self.responses.get(timeout=7)
        except queue.Empty as error:raise TimeoutError('LEASE_RESPONSE_TIMEOUT') from error
        if isinstance(item,BaseException):raise item
        return item
    def send(self,kind,body=b''):
        self._owner();self.process.stdin.write(struct.pack('<8sIIQ',b'ARSLES02',2,kind,len(body))+body);self.process.stdin.flush()
        return self.response()
    def prepare(self,key,bindings,control=1):
        self._owner()
        if self.pending is not None:raise RuntimeError('LEASE_BUSY')
        body=command(key,bindings,control)
        if (key.session,key.package,key.epoch,key.nonce,key.model)!=(self.session,self.revision,self.epoch,self.nonce,self.model):raise ValueError('INVALID_LEASE_IDENTITY')
        kind,packet=self.send(2,body)
        if kind!=3:raise ValueError('INVALID_LEASE_CANDIDATE')
        self.pending=key;return packet
    def accept(self,checkpoint):
        self._owner()
        if self.pending is None or checkpoint_key(checkpoint)!=self.pending:raise ValueError('INVALID_LEASE_ACCEPT')
        body=self.pending.wire()+hashlib.sha256(checkpoint).digest()
        if self.send(4,body)!=(5,body):raise ValueError('INVALID_LEASE_ACK')
        self.pending=None
    def close(self):
        if threading.get_ident()!=self.owner:raise RuntimeError('WRONG_THREAD')
        if self.closed:return self.cleanup
        self.closed=True
        if not self.process.stdin.closed:
            try:self.process.stdin.close()
            except (OSError,ValueError):pass
        try:code=self.process.wait(timeout=7)
        except subprocess.TimeoutExpired:
            self.process.kill();code=self.process.wait(timeout=5)
        self.reader.join(timeout=2)
        state={}
        try:state=json.loads((self.folder/'launcher-state.json').read_text(encoding='utf-8'))
        except (OSError,ValueError):pass
        self.process.stdout.close()
        self.cleanup={'profile_removed':profile_absent(self.profile),'job_empty':code in (0,16) and type(state.get('worker_exit')) is int,'broker_exit':code}
        return self.cleanup
    def crash_owned_child(self):
        """Private fault probe: validate unique owned image before terminating it."""
        import ctypes
        from ctypes import wintypes
        self._owner()
        pid=json.loads((self.folder/'worker-start.json').read_text())['worker_pid']
        k=ctypes.WinDLL('kernel32',use_last_error=True)
        k.OpenProcess.argtypes=(wintypes.DWORD,wintypes.BOOL,wintypes.DWORD);k.OpenProcess.restype=wintypes.HANDLE
        k.QueryFullProcessImageNameW.argtypes=(wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD))
        k.TerminateProcess.argtypes=(wintypes.HANDLE,wintypes.UINT);k.CloseHandle.argtypes=(wintypes.HANDLE,)
        handle=k.OpenProcess(0x1001,False,pid)
        if not handle:raise RuntimeError('OWNED_CHILD_UNAVAILABLE')
        try:
            image=ctypes.create_unicode_buffer(32768);size=wintypes.DWORD(len(image))
            if not k.QueryFullProcessImageNameW(handle,0,image,ctypes.byref(size)) or Path(image.value).resolve()!= (self.folder/'worker.exe').resolve():raise RuntimeError('OWNED_CHILD_IDENTITY_DENIED')
            if not k.TerminateProcess(handle,91):raise RuntimeError('OWNED_CHILD_TERMINATE_DENIED')
        finally:k.CloseHandle(handle)
    def finish(self):
        self._owner()
        if self.pending is not None:raise RuntimeError('LEASE_BUSY')
        if self.send(6)!=(7,b''):raise ValueError('INVALID_LEASE_CLOSE')
        return self.close()
