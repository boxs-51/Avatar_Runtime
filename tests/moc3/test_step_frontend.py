from pathlib import Path
import hashlib
import struct
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
if sys.platform=='win32':from moc3_public_package import _prepare_step

@unittest.skipUnless(sys.platform=='win32','Windows optional transport')
class StepFrontend(unittest.TestCase):
    def test_identity_and_command(self):
        package=bytes(22);entry='m.model3.json';p,nonce,mode,model=_prepare_step(package,entry,1,0,[(0,.5)],0,b'')
        self.assertEqual(mode,'--step');self.assertTrue(nonce);self.assertEqual(model,hashlib.sha256(struct.pack('<I',len(entry))+entry.encode()+package).digest())
        self.assertEqual(struct.unpack_from('<QQII',p,24+len(entry)+22),(1,0,0,1))
    def test_invalid_identity_state_and_controls(self):
        for sequence,clock,checkpoint in ((True,0,b''),(0,0,b''),(9,0,b''),(1,1,b''),(1,0,b'x'),(2,1,b''),(2,0,bytes(72)),(2,1,bytes(128*1024+1))):
            with self.assertRaises(ValueError):_prepare_step(bytes(22),'m.model3.json',sequence,clock,[],0,checkpoint)
        for control in (-1,4,True):
            with self.assertRaises(ValueError):_prepare_step(bytes(22),'m.model3.json',1,0,[],control,b'')
if __name__=='__main__':unittest.main()
