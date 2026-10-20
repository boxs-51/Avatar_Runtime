"""Typed lease diagnostic client; actual native core/query fence owns publication."""
import time
from moc3_publication_probe import PublicationProbe
from moc3_lease import command
class LeasePublicationProbe(PublicationProbe):
    def __init__(self,folder,session,hardware=False):
        super().__init__(folder,hardware)
        if self.command(f'LEASE_OPEN {session}')!=['OPENED']:self.close();raise RuntimeError('LEASE_HOST_OPEN')
    def begin_lease(self,key,bindings,control=1,lease_ms=2000):
        body=command(key,bindings,control);return self.command(f'LEASE_BEGIN {len(body)} {lease_ms}',body)
    def stage_lease(self,packet,fail_target=False):return self.command(('LEASE_FAIL ' if fail_target else 'LEASE_STAGE ')+str(len(packet)),packet)
    def poll(self):
        end=time.monotonic()+7
        while time.monotonic()<end:
            response=self.command('LEASE_POLL')
            if response!=['BUSY']:return response
            time.sleep(.001)
        raise TimeoutError('LEASE_QUERY_QUARANTINED')
    def checkpoint(self):
        if self.command('LEASE_CHECKPOINT')!=['SAVED']:raise RuntimeError('LEASE_CHECKPOINT')
        return (self.folder/'current.leasecp').read_bytes()
    def core(self):
        if self.command('LEASE_CORE')!=['SAVED']:raise RuntimeError('LEASE_CORE')
        return (self.folder/'current.core').read_bytes()
    def readback(self):
        if self.command('LEASE_READBACK')!=['READBACK']:raise RuntimeError('LEASE_READBACK')
        return (self.folder/'current.rgba').read_bytes(),(self.folder/'current.ppm').read_bytes()
    def state(self):
        reply=self.command('LEASE_STATUS');keys=('generation','host_generation','epoch','package','clock','pending','terminal','device_calls','creations','ingress','core_status','ready_calls')
        if len(reply)!=13 or reply[0]!='STATE':raise RuntimeError('LEASE_STATE')
        return dict(zip(keys,map(int,reply[1:])))
