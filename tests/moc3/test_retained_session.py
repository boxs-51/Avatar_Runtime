"""Generated frontend boundary cases; native/OS execution is tested by the separate verifier."""
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
if sys.platform=='win32':import moc3_public_package as broker
if sys.platform=='win32':
    from moc3_verify_retained_session import require_geometry_motion
    from moc3_admission_fixtures import drawable_packet


@unittest.skipUnless(sys.platform=='win32','Windows isolation diagnostics')
class GeometryOracle(unittest.TestCase):
    def sequence(self,change):
        records=[]
        for i in range(3):
            packet=bytearray(drawable_packet());struct.pack_into('<I',packet,8,2);struct.pack_into('<Q',packet,20,i+2)
            if i==1:change(packet)
            records.append(struct.pack('<QQQ',i+1,i*33333334,len(packet))+packet)
        body=b''.join(records)
        return b'ARSEVAL1'+struct.pack('<IIQQ',1,3,42,32+len(body))+body

    def test_appearance_only_cannot_prove_geometry(self):
        for change in (lambda p:struct.pack_into('<f',p,68+20,.5),
                       lambda p:struct.pack_into('<f',p,68+24,.5),
                       lambda p:struct.pack_into('<f',p,68+68+8,.5),
                       lambda p:p.__setitem__(64,0)):
            with self.assertRaises(RuntimeError):require_geometry_motion(self.sequence(change))

    def test_actual_xy_change_and_release(self):
        require_geometry_motion(self.sequence(lambda p:struct.pack_into('<f',p,68+68,-.5)))

@unittest.skipUnless(sys.platform=='win32','Windows isolation diagnostics')
class RetainedFrontend(unittest.TestCase):
    def test_complete_frames_are_explicitly_opt_in(self):
        with patch.object(broker,'_run_envelope') as run:
            broker.evaluate_zip(bytes(22),'m.model3.json',[(1,0,[]),(2,16666667,[(0,.5)]),(3,33333334,[])])
            payload,nonce,mode=run.call_args.args;self.assertEqual(mode,'--evaluate');self.assertEqual(payload[:8],b'ARSMPK01');self.assertTrue(nonce)
            self.assertEqual(struct.unpack_from('<I',payload,24+len('m.model3.json')+22)[0],3)
    def test_invalid_complete_frames_never_launch(self):
        invalid=([],[(True,0,[])],[(1,1,[])],[(1,0,[]),(1,1,[])],[(1,0,[]),(2,0,[])],
            [(1,0,[]),(2,200_000_000,[])],[(1,0,[(0,float('nan'))])],[(1,0,[(0,1.1)])],
            [(1,0,[(True,.5)])],[(1,0,[(0,.5),(0,.2)])],[(1,0,[(4096,.5)])],[(1,0,[(0,True)])],
            [(1,0,[[]])],[(1,0,[(0,1,2)])])
        with patch.object(broker,'_run_envelope') as run:
            for frames in invalid:
                with self.subTest(frames=repr(frames)),self.assertRaises(ValueError):broker.evaluate_zip(bytes(22),'m.model3.json',frames)
            run.assert_not_called()
    def test_clock_and_binding_ceiling(self):
        with patch.object(broker,'_run_envelope') as run:
            frames=[(i+1,i*133333336,[(s,0) for s in range(32)]) for i in range(8)]
            broker.evaluate_zip(bytes(22),'m.model3.json',frames);self.assertTrue(run.called)
            with self.assertRaises(ValueError):broker.evaluate_zip(bytes(22),'m.model3.json',[(1,0,[(i,0) for i in range(33)])])

if __name__=='__main__':unittest.main()
