"""Generated broker envelope/protocol checks, distinct from actual AppContainer proof."""
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
import uuid
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
if sys.platform=='win32':import moc3_public_preflight as broker


@unittest.skipUnless(sys.platform=='win32','Windows isolation broker')
class PublicPreflightTests(unittest.TestCase):
    def test_bad_envelope_never_starts_native_code(self):
        with patch.object(broker,'_run') as run:
            for moc,count in ((b'MOC',1),(bytearray(b'MOC3'),1),(b'MOC3',True),(b'MOC3',0),
                              (b'MOC3',17),(b'MOC3',10**1000),(b'MOC3'+bytes(16*1024*1024),1)):
                with self.assertRaises(ValueError):broker.preflight_moc(moc,count)
            run.assert_not_called()

    def fake_worker(self):
        folder=ROOT/'build/f4-public-broker-tests'/uuid.uuid4().hex[:24];folder.mkdir(parents=True)
        (folder/'ars_moc3_import_worker.exe').write_bytes(b'generated non-executable placeholder')
        return folder

    def result(self,output,code=0,removed=True):
        with patch.object(broker,'BIN',self.fake_worker()),patch.object(broker,'profile_absent',return_value=removed),\
             patch.object(broker.subprocess,'run',return_value=subprocess.CompletedProcess([],code,output)):
            return broker._run('--moc',b'generated')

    def test_status_allowlist_and_exact_pipe_cap(self):
        self.assertEqual(self.result(b'PREFLIGHT_OK\n').status,'PREFLIGHT_OK')
        for output in (b'x'*65,b'unknown\n',b'PREFLIGHT_OK\nextra',b'\xff'):
            self.assertEqual(self.result(output).status,'BROKER_PROTOCOL_REJECTED')

    def test_setup_and_cleanup_failures_never_fall_back(self):
        for code,removed in ((11,True),(0,False)):
            result=self.result(b'PREFLIGHT_OK\n',code,removed)
            self.assertEqual(result.status,'ISOLATION_SETUP_REJECTED')
            self.assertFalse(result.job_empty)

    def test_internal_mode_allowlist(self):
        with self.assertRaises(ValueError):broker._run('--arbitrary-command')

if __name__=='__main__':unittest.main()
