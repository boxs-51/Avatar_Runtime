"""Generated custody/cleanup regressions; actual OS evidence is a separate verifier."""
from pathlib import Path
import json
import subprocess
import sys
import unittest
from unittest.mock import patch
import uuid
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
if sys.platform=='win32':
    import moc3_public_package as broker
    from moc3_admission_fixtures import drawable_packet

@unittest.skipUnless(sys.platform=='win32','Windows AppContainer broker')
class PackageBrokerTests(unittest.TestCase):
    def fake(self,output,code=0,removed=True,worker_exit=0,wait=0,overflow=False,log=True):
        folder=ROOT/'build/moc3-package-broker-tests'/uuid.uuid4().hex[:24];folder.mkdir(parents=True)
        (folder/'ars_moc3_import_worker.exe').write_bytes(b'generated placeholder')
        def run(*args,**kwargs):
            if log:(Path(kwargs['cwd'])/'launcher-state.json').write_text(json.dumps({'worker_exit':worker_exit,'wait_result':wait,'output_overflow':overflow}),encoding='utf-8')
            return subprocess.CompletedProcess([],code,output)
        with patch.object(broker,'ISOLATED_BIN',folder),patch.object(broker,'profile_absent',return_value=removed),patch.object(broker.subprocess,'run',side_effect=run):
            return broker._run_envelope(b'generated',42)
    def test_custody_is_not_scene_validation(self):
        result=self.fake(drawable_packet());self.assertEqual(result.status,'PACKET_CUSTODY');self.assertTrue(result.packet)
        self.assertEqual(self.fake(b'ARSDRAW1'+bytes(40)).status,'PACKET_CUSTODY') # Native guard must reject this later.
    def test_cleanup_and_launcher_failure_never_return_packet(self):
        for kwargs in ({'code':11},{'removed':False},{'log':False}):
            with self.subTest(kwargs=kwargs):
                result=self.fake(drawable_packet(),**kwargs);self.assertFalse(result.packet);self.assertEqual(result.status,'ISOLATION_SETUP_REJECTED')
    def test_worker_failure_or_partial_output_is_not_custody(self):
        for kwargs in ({'worker_exit':1},{'wait':258},{'overflow':True}):
            with self.subTest(kwargs=kwargs):
                result=self.fake(drawable_packet(),**kwargs);self.assertFalse(result.packet);self.assertEqual(result.status,'BROKER_PROTOCOL_REJECTED')
        for data in (b'ARSDRAW1',b'unknown',b'PACKAGE_BUDGET_REJECTED\nextra',b'\xff'):
            self.assertEqual(self.fake(data).status,'BROKER_PROTOCOL_REJECTED')
    def test_typed_rejections(self):
        for value in broker.REJECTIONS:
            result=self.fake(value+b'\n');self.assertEqual(result.status,value.decode());self.assertFalse(result.packet)
    def test_invalid_transport_does_not_launch(self):
        with patch.object(broker,'_run_envelope') as run:
            for package,entry in ((b'bad','a'),(bytearray(22),'a'),(bytes(22),''),(bytes(22),'x'*513),(bytes(22),None)):
                with self.assertRaises(ValueError):broker.admit_zip(package,entry)
            run.assert_not_called()
        with self.assertRaises(ValueError):broker._run_envelope(b'',42,'--arbitrary')

if __name__=='__main__':unittest.main()
