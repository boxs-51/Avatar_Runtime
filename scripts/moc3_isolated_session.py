"""Private logical owner session using only isolated lease evaluation and native publication."""
import hashlib,math,secrets,threading,time
from pathlib import Path
from moc3_lease import Lease,Key,EMPTY,checkpoint_key,command
from moc3_public_package import _package_envelope
from moc3_lease_publication import LeasePublicationProbe
STEP=16_666_667
class SessionError(RuntimeError):pass
class IsolatedSession:
    def __init__(self,package,entry,folder,hardware=False):
        self.owner=threading.get_ident();self.session=secrets.randbits(64) or 1;self.folder=Path(folder);self.folder.mkdir(parents=True,exist_ok=True)
        _package_envelope(package,entry,1)
        self.package=package;self.entry=entry;self.package_revision=1;self.epoch=0;self.worker=None;self.checkpoint=b'';self.accepted=None;self.closed=False;self.cleanups=[];self.request_sequence=0
        self.host=LeasePublicationProbe(self.folder/'host',self.session,hardware)
    def _owner(self):
        if threading.get_ident()!=self.owner:raise SessionError('WRONG_THREAD')
        if self.closed:raise SessionError('SESSION_CLOSED')
    def _reap(self,graceful=False):
        if self.worker is None:return
        worker=self.worker;self.worker=None
        try:
            if graceful and worker.pending is None and worker.process.poll() is None:cleanup=worker.finish()
            else:cleanup=worker.close()
        except BaseException:cleanup=worker.close()
        self.cleanups.append(cleanup)
        if not cleanup['job_empty'] or not cleanup['profile_removed']:raise SessionError('LEASE_CLEANUP_UNVERIFIED')
    def _worker(self,package,entry,revision,checkpoint,base):
        if self.worker is not None:
            age=time.monotonic()-self.worker_started
            if self.worker.process.poll() is not None or age>=3 or (self.accepted and self.accepted.local_sequence>=8):self._reap(True)
        if self.worker is None:
            self.epoch+=1
            if self.epoch>=2**64-1:raise SessionError('EPOCH_EXHAUSTED')
            self.worker_started=time.monotonic();self.worker=Lease(package,entry,self.session,revision,self.epoch,checkpoint,base)
        return self.worker
    def status(self):
        self._owner();value=self.host.state();value.update(accepted_package=value['package'],package=self.package_revision,session=self.session,request_sequence=self.request_sequence,worker_active=self.worker is not None and self.worker.process.poll() is None)
        return value
    def snapshot(self):
        self._owner()
        if not self.checkpoint:return b'',b'',b''
        return self.host.core(),self.host.checkpoint(),self.host.readback()[0]
    def update(self,bindings=(),*,control=1,advance_ns=6*STEP,lease_ms=2000,fault=None,package=None,entry=None):
        self._owner()
        if type(lease_ms) is not int or not 100<=lease_ms<=2000 or type(advance_ns) is not int or not 1<=advance_ns<=8*STEP:raise ValueError('INVALID_SESSION_TIMING')
        # Freeze and validate the complete owner input before launching a job.
        probe=Key(self.session,1,1,1,0,1,0,1,1,bytes(32),EMPTY)
        command(probe,bindings,control)
        bindings=tuple((slot,float(value)) for slot,value in bindings)
        self.request_sequence+=1;deadline=time.monotonic()+lease_ms/1000
        replacing=package is not None;payload=package if replacing else self.package;manifest=entry if replacing else self.entry;revision=self.package_revision+1 if replacing else self.package_revision
        if replacing:
            if bindings or control!=3:raise ValueError('REPLACEMENT_REQUIRES_DEFAULT_RESET')
            _package_envelope(payload,manifest,1)
            self._reap(True)
        origin=b'' if replacing else self.checkpoint;base=self.accepted.generation if self.accepted else 0
        try:
            worker=self._worker(payload,manifest,revision,origin,base)
            previous=None if replacing else self.accepted
            generation=base+1;clock=previous.clock+advance_ns if previous else 0
            local_sequence=previous.local_sequence+1 if previous and previous.epoch==worker.epoch else 1
            local_clock=previous.local_clock+advance_ns if previous and previous.epoch==worker.epoch else advance_ns if previous else 0
            key=Key(self.session,revision,worker.epoch,generation,clock,local_sequence,local_clock,generation,worker.nonce,worker.model,hashlib.sha256(origin).digest())
            remaining=int((deadline-time.monotonic())*1000)
            if remaining<100:raise SessionError('INPUT_EXPIRED_DURING_ADMISSION')
            if self.host.begin_lease(key,bindings,control,min(lease_ms,remaining))!=['PREPARED']:raise SessionError('HOST_BEGIN_DENIED')
            if fault=='crash':worker.crash_owned_child()
            elif fault=='timeout':time.sleep(5.1)
            packet=worker.prepare(key,list(bindings),control)
            if fault=='malformed':packet=packet[:-1]
            if self.host.stage_lease(packet,fail_target=fault=='target')!=['STAGED']:raise SessionError('CANDIDATE_REJECTED')
            if fault=='cancel':self.host.command('LEASE_CANCEL')
            elif fault=='cancel_ready':self.host.command('LEASE_CANCEL_READY')
            elif fault=='revoke':self.host.command('LEASE_REVOKE_READY')
            elif fault=='expiry':time.sleep(lease_ms/1000+.02)
            if self.host.poll()!=['PUBLISHED']:raise SessionError('FINAL_FENCE_REJECTED')
            accepted=self.host.checkpoint()
            if checkpoint_key(accepted)!=key:raise SessionError('HOST_CHECKPOINT_MISMATCH')
            # Native core/frame/checkpoint has already committed. ACK failure
            # only retires the lease; it can never roll this publication back.
            self.checkpoint=accepted;self.accepted=key
            if replacing:self.package,self.entry,self.package_revision=payload,manifest,revision
            ack_lost=fault=='ack_loss'
            try:
                if ack_lost:raise SessionError('INJECTED_ACK_LOSS')
                worker.accept(accepted)
            except BaseException:self._reap();ack_lost=True
            result=self.status();result['ack_lost']=ack_lost;return result
        except BaseException:
            try:self.host.command('LEASE_CANCEL')
            finally:self._reap()
            raise
    def replace(self,package,entry):return self.update(control=3,package=package,entry=entry)
    def close(self):
        if threading.get_ident()!=self.owner:raise SessionError('WRONG_THREAD')
        if self.closed:return
        try:self._reap(True);self.host.finish()
        finally:self.host.close();self.closed=True
