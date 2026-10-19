"""Generated checkpoint ACK and actual core/GPU transaction tests."""
from pathlib import Path
import hashlib
import struct
import sys
import tempfile
import time
import unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from moc3_admission_fixtures import drawable_packet
from moc3_publication_probe import PublicationProbe,retained_state,BIN
MODEL=hashlib.sha256(b'generated model').digest()

def state(sequence,clock):
    return b'ARSPHYS1'+struct.pack('<IIQQQ',1,1,sequence,clock,0)+MODEL+struct.pack('<I8d',2,0,0,0,0,0,1,0,0)

def candidate(sequence,clock,prior=b''):
    packet=bytearray(drawable_packet());struct.pack_into('<I',packet,8,4);struct.pack_into('<Q',packet,20,sequence+1);struct.pack_into('<I',packet,36,131)
    if sequence>1:packet[64:68]=b'\0\0\xff\xff'
    checkpoint=state(sequence,clock)
    return b'ARSCAND1'+struct.pack('<IIQQQQII',1,0,42,sequence,clock,len(packet),len(checkpoint),0)+MODEL+hashlib.sha256(prior).digest()+packet+checkpoint

@unittest.skipUnless(sys.platform=='win32' and (BIN/'ars_moc3_packet_publication_host.exe').exists(),'optional native host unavailable')
class StepPublication(unittest.TestCase):
    def setUp(self):self.temp=tempfile.TemporaryDirectory(dir=ROOT/'build');self.owner=PublicationProbe(self.temp.name)
    def tearDown(self):self.owner.close();self.temp.cleanup()
    def baseline(self):
        self.assertEqual(self.owner.begin_step(1,42,0,MODEL),['PREPARED']);self.assertEqual(self.owner.stage_step(candidate(1,0)),['STAGED']);self.assertEqual(self.owner.poll(),['PUBLISHED'])
        checkpoint=self.owner.checkpoint();self.assertEqual(checkpoint,state(1,0));return self.owner.state(),checkpoint,self.owner.readback()[0]
    def unchanged(self,old,checkpoint,pixels):
        self.assertTrue(retained_state(old,self.owner.state()));self.assertEqual(self.owner.checkpoint(),checkpoint);self.assertEqual(self.owner.readback()[0],pixels)
    def test_first_invalid_before_gpu(self):
        self.assertEqual(self.owner.begin_step(1,42,0,MODEL),['PREPARED']);self.assertEqual(self.owner.stage_step(candidate(1,0)[:-1]),['REJECTED'])
        self.assertEqual(self.owner.state()['device_calls'],0);self.assertEqual(self.owner.checkpoint(),b'');self.owner.finish()
    def test_checkpoint_only_changes_with_publication(self):
        old,checkpoint,pixels=self.baseline();self.assertEqual(self.owner.begin_step(2,42,33333334,MODEL,.75),['PREPARED']);self.assertEqual(self.owner.stage_step(candidate(2,33333334,checkpoint)),['STAGED'])
        self.unchanged(old,checkpoint,pixels);self.assertEqual(self.owner.poll(),['PUBLISHED']);self.assertEqual(self.owner.checkpoint(),state(2,33333334));self.assertNotEqual(self.owner.readback()[0],pixels);self.owner.finish()
    def test_stale_or_wrong_ack_before_gpu(self):
        for kind in ('ack','model','state_nan','trailing'):
            if kind!='ack':self.owner.close();self.owner=PublicationProbe(Path(self.temp.name)/kind)
            old,checkpoint,pixels=self.baseline();self.assertEqual(self.owner.begin_step(2,42,33333334,MODEL),['PREPARED']);packet=bytearray(candidate(2,33333334,checkpoint))
            if kind=='ack':packet[88]^=1
            elif kind=='model':packet[56]^=1
            elif kind=='state_nan':struct.pack_into('<Q',packet,len(packet)-64,0x7ff8000000000000)
            else:packet+=b'x'
            self.assertEqual(self.owner.stage_step(bytes(packet)),['REJECTED']);self.unchanged(old,checkpoint,pixels);self.assertEqual(self.owner.state()['gpu_creations'],old['gpu_creations']);self.owner.finish()
    def test_final_fence_retains_exact_checkpoint(self):
        for kind in ('cancel','revoke','expiry'):
            if kind!='cancel':self.owner.close();self.owner=PublicationProbe(Path(self.temp.name)/kind)
            old,checkpoint,pixels=self.baseline();self.assertEqual(self.owner.begin_step(2,42,33333334,MODEL,lease_ms=100 if kind=='expiry' else 2000),['PREPARED'])
            self.assertEqual(self.owner.stage_step(candidate(2,33333334,checkpoint)),['STAGED'])
            if kind=='cancel':self.assertEqual(self.owner.command('CANCEL'),['CANCELLED'])
            elif kind=='revoke':self.assertEqual(self.owner.command('REVOKE_READY'),['ARMED'])
            else:time.sleep(.12)
            self.assertEqual(self.owner.poll(),['REJECTED']);self.unchanged(old,checkpoint,pixels);self.assertEqual(self.owner.begin_step(2,42,33333334,MODEL),['DENIED']);self.owner.finish()
    def test_wrong_package_and_mixed_mode_denied(self):
        old,checkpoint,pixels=self.baseline();self.assertEqual(self.owner.begin_step(2,42,33333334,bytes(32)),['DENIED']);self.assertEqual(self.owner.begin(2,42,1,33333334),['DENIED']);self.unchanged(old,checkpoint,pixels);self.owner.finish()
if __name__=='__main__':unittest.main()
