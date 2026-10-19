import base64
from pathlib import Path
import sys
import struct
import unittest
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from moc3_frame_packet import encode,decode,validate,MAX_RAW,PREFIX

class FramePacketTests(unittest.TestCase):
    def fixture(self):
        return ({'width':512,'height':512,'frame_generation':1,'snapshot_generation':2,
                 'target_epoch':1,'device_generation':101,'core_generation':1,
                 'backend':'D3D11_WARP','behavior':'CORE_RELEASE'},
                 bytes(512*512*4),b'P6\n512 512\n255\n'+bytes(512*512*3))
    def test_exact_owned_packet_roundtrip(self):
        self.assertEqual(decode(encode(*self.fixture())),self.fixture())
        for behavior in ('CORE_DISABLED','CORE_REENABLED'):
            meta,rgba,ppm=self.fixture();meta['behavior']=behavior
            self.assertEqual(decode(encode(meta,rgba,ppm)),(meta,rgba,ppm))
    def test_reject_bomb_truncation_and_trailing_stream(self):
        value=encode(*self.fixture())
        for bad in (value[:-5],PREFIX+base64.b64encode(zlib.compress(b'x'*(MAX_RAW+1))).decode(),
                    PREFIX+base64.b64encode(zlib.compress(b'x')+b'extra').decode()):
            with self.assertRaises((ValueError,zlib.error)):decode(bad)
    def test_identity_size_and_field_fail_closed(self):
        for key,value in (('core_generation',2),('width',True),('behavior','other'),('private_path','not-allowed')):
            meta,rgba,ppm=self.fixture();meta[key]=value
            with self.assertRaises(ValueError):encode(meta,rgba,ppm)
        meta,rgba,ppm=self.fixture()
        with self.assertRaises(ValueError):encode(meta,rgba[:-1],ppm)
    def test_deep_or_invalid_json_is_typed_failure(self):
        for header in (b'['*5000+b'0'+b']'*5000,b'\xff',b'{"width":512,"width":512}'):
            raw=struct.pack('<III',len(header),0,0)+header
            line=PREFIX+base64.b64encode(zlib.compress(raw)).decode()
            with self.assertRaises(ValueError):decode(line)
if __name__=='__main__':unittest.main()
