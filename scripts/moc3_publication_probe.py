"""Bounded trusted diagnostic pipe client; no asset upload or model parser."""
from pathlib import Path
import queue
import subprocess
import threading
import time
ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build/m2-d-private/bin/Release'

class PublicationProbe:
    def __init__(self,folder,hardware=False):
        self.folder=Path(folder);self.folder.mkdir(parents=True,exist_ok=True)
        self.process=subprocess.Popen([str(BIN/'ars_moc3_packet_publication_host.exe'),str(self.folder),'--hardware' if hardware else '--warp'],
                                      stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
        self.responses=queue.Queue(maxsize=128)
        def reader():
            while True:
                line=self.process.stdout.readline(4096)
                self.responses.put(line)
                if not line:return
        self.reader=threading.Thread(target=reader,daemon=True);self.reader.start()
        if self.response()!=['READY']:self.close();raise RuntimeError('publication bootstrap')

    def response(self):
        try:line=self.responses.get(timeout=30)
        except queue.Empty as error:raise RuntimeError('publication response timeout') from error
        if not line or len(line)>=4096:raise RuntimeError('publication transport closed')
        return line.decode('ascii').split()

    def command(self,text,packet=None):
        self.process.stdin.write(text.encode('ascii')+b'\n')
        if packet is not None:self.process.stdin.write(packet)
        self.process.stdin.flush();return self.response()

    def begin(self,sequence,nonce,index,clock,value=0,lease_ms=2000):
        return self.command(f'BEGIN {sequence} {nonce} {index} {clock} {value:.9g} {lease_ms}')

    def stage(self,packet,*,fail_target=False):
        return self.command(('STAGE_FAIL ' if fail_target else 'STAGE ')+str(len(packet)),packet)

    def begin_step(self,sequence,nonce,clock,model,value=0,lease_ms=2000):
        return self.command(f'STEP_BEGIN {sequence} {nonce} {clock} {value:.9g} {lease_ms} {model.hex()}')

    def stage_step(self,packet,*,fail_target=False):
        return self.command(('STEP_FAIL ' if fail_target else 'STEP_STAGE ')+str(len(packet)),packet)

    def checkpoint(self):
        if self.command('CHECKPOINT')!=['CHECKPOINT']:raise RuntimeError('checkpoint readback')
        return (self.folder/'current.checkpoint').read_bytes()

    def poll(self):
        deadline=time.monotonic()+20
        while time.monotonic()<deadline:
            response=self.command('POLL')
            if response!=['BUSY']:return response
            time.sleep(.001)
        raise RuntimeError('publication query deadline: process quarantine required')

    def state(self):
        response=self.command('STATUS')
        if len(response)!=13 or response[0]!='STATE':raise RuntimeError('publication state shape')
        keys=('core_generation','host_generation','epoch','physics_cursor','terminal','pending','target_matches_accepted','device_calls','gpu_creations','ready_calls','core_status')
        return dict(zip(keys,map(int,response[1:12])),core_value=float(response[12]))

    def readback(self):
        if self.command('READBACK')!=['READBACK']:raise RuntimeError('publication readback')
        return (self.folder/'current.rgba').read_bytes(),(self.folder/'current.ppm').read_bytes()

    def finish(self):
        if self.command('QUIT')!=['BYE'] or self.process.wait(timeout=20)!=0:raise RuntimeError('publication cleanup/debug failure')
        self.close()

    def close(self):
        if self.process.poll() is None:self.process.kill();self.process.wait(timeout=5)
        self.reader.join(timeout=2)
        self.process.stdin.close();self.process.stdout.close()


def retained_state(before,after):
    keys=('core_generation','host_generation','epoch','physics_cursor','core_value')
    return all(before[k]==after[k] for k in keys) and after['target_matches_accepted']==1
