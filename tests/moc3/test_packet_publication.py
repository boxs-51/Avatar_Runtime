"""Generated hostile protocol and real core/GPU publication negatives."""
from pathlib import Path
import struct
import sys
import tempfile
import time
import unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from moc3_admission_fixtures import drawable_packet
from moc3_publication_probe import PublicationProbe,retained_state,BIN

def generated_sequence():
    records=[]
    for i in range(3):
        packet=bytearray(drawable_packet());struct.pack_into('<I',packet,8,3);struct.pack_into('<Q',packet,20,i+2);struct.pack_into('<I',packet,36,131)
        if i==1:packet[64:68]=b'\0\0\xff\xff'
        records.append(struct.pack('<QQQ',i+1,i*33333334,len(packet))+packet)
    body=b''.join(records);return b'ARSEVAL1'+struct.pack('<IIQQ',2,3,42,len(body)+32)+body

@unittest.skipUnless(sys.platform=='win32' and (BIN/'ars_moc3_packet_publication_host.exe').exists(),'optional native Windows host unavailable')
class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=ROOT/'build');self.owner=PublicationProbe(self.temp.name);self.packet=generated_sequence()
    def tearDown(self):self.owner.close();self.temp.cleanup()
    def baseline(self):
        self.assertEqual(self.owner.begin(1,42,0,0),['PREPARED']);self.assertEqual(self.owner.stage(self.packet),['STAGED']);self.assertEqual(self.owner.poll(),['PUBLISHED'])
        state=self.owner.state();self.assertEqual([state[k] for k in ('core_generation','host_generation','epoch','physics_cursor')],[1,1,1,2]);return state,self.owner.readback()[0]
    def retained(self,before,pixels):
        self.assertTrue(retained_state(before,self.owner.state()));self.assertEqual(self.owner.readback()[0],pixels)
    def test_malformed_before_first_gpu(self):
        self.assertEqual(self.owner.begin(1,42,0,0),['PREPARED']);self.assertEqual(self.owner.stage(self.packet[:-1]),['REJECTED'])
        state=self.owner.state();self.assertEqual(state['device_calls'],0);self.assertEqual(state['gpu_creations'],0);self.assertEqual(state['core_generation'],0);self.owner.finish()
    def test_busy_then_final_core_and_image(self):
        before,pixels=self.baseline();self.assertEqual(self.owner.begin(2,42,1,33333334,.75),['PREPARED']);self.assertEqual(self.owner.stage(self.packet),['STAGED'])
        self.assertEqual(self.owner.command('DELAY_ONCE'),['ARMED']);self.assertEqual(self.owner.command('POLL'),['BUSY']);self.retained(before,pixels)
        self.assertEqual(self.owner.poll(),['PUBLISHED']);state=self.owner.state();self.assertEqual(state['core_generation'],2);self.assertEqual(state['core_value'],.75);self.assertNotEqual(self.owner.readback()[0],pixels);self.owner.finish()
    def test_late_revoke_at_actual_query_ready(self):
        before,pixels=self.baseline();self.assertEqual(self.owner.begin(2,42,1,33333334),['PREPARED']);self.assertEqual(self.owner.stage(self.packet),['STAGED'])
        self.assertEqual(self.owner.command('REVOKE_READY'),['ARMED']);self.assertEqual(self.owner.poll(),['REJECTED']);self.retained(before,pixels)
        self.assertGreater(self.owner.state()['ready_calls'],before['ready_calls']);self.assertEqual(self.owner.begin(3,42,2,66666668),['DENIED']);self.owner.finish()
    def test_cancel_after_submission(self):
        before,pixels=self.baseline();self.assertEqual(self.owner.begin(2,42,1,33333334),['PREPARED']);self.assertEqual(self.owner.stage(self.packet),['STAGED'])
        self.assertEqual(self.owner.command('CANCEL'),['CANCELLED']);self.assertEqual(self.owner.poll(),['REJECTED']);self.retained(before,pixels);self.owner.finish()
    def test_malformed_transport_quarantines_pending_process(self):
        before,pixels=self.baseline();self.assertEqual(self.owner.begin(2,42,1,33333334),['PREPARED']);self.assertEqual(self.owner.stage(self.packet),['STAGED'])
        self.assertTrue(self.owner.state()['pending']);self.retained(before,pixels)
        self.owner.process.stdin.write(b'BEGIN malformed\n');self.owner.process.stdin.flush()
        self.assertEqual(self.owner.process.wait(timeout=5),2)
        self.assertEqual((self.owner.folder/'current.rgba').read_bytes(),pixels)
    def test_actual_elapsed_lease_expiry_after_submission(self):
        before,pixels=self.baseline();self.assertEqual(self.owner.begin(2,42,1,33333334,lease_ms=100),['PREPARED']);self.assertEqual(self.owner.stage(self.packet),['STAGED']);time.sleep(.12)
        self.assertEqual(self.owner.poll(),['REJECTED']);self.retained(before,pixels);self.owner.finish()
    def test_rejected_reload_and_gpu_allocation(self):
        for kind in ('invalid','nonce','clock','gpu'):
            if kind!='invalid':self.owner.close();self.owner=PublicationProbe(Path(self.temp.name)/kind)
            before,pixels=self.baseline();self.assertEqual(self.owner.begin(2,43 if kind=='nonce' else 42,1,33333335 if kind=='clock' else 33333334),['PREPARED'])
            self.assertEqual(self.owner.stage(self.packet[:-1] if kind=='invalid' else self.packet,fail_target=kind=='gpu'),['REJECTED']);self.retained(before,pixels)
            self.assertEqual(self.owner.state()['gpu_creations'],before['gpu_creations']);self.owner.finish()

if __name__=='__main__':unittest.main()
