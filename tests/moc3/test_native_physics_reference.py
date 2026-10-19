"""Independent Python-method comparison on a redistributable generated chain."""
from pathlib import Path
import json
import struct
import subprocess
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
from moc3_physics import Physics
from moc3_native_test_target import configured_target

class NativeReference(unittest.TestCase):
    def test_fixed_clock_controls_and_release(self):
        executable=configured_target('ARS_MOC3_PHYSICS_TEST_EXE',ROOT/'build/moc3-public-isolation/bin/Release/ars_moc3_retained_physics_tests.exe')
        if not executable.exists():self.skipTest('native optional build unavailable')
        def f32(value):return struct.unpack('<f',struct.pack('<f',value))[0]
        for name in ('generated_physics3.json','generated_physics3_multichain.json'):
            with self.subTest(fixture=name):
                fixture=ROOT/'tests/moc3'/name
                actual=json.loads(subprocess.check_output([str(executable),str(fixture),'--trace'],timeout=10))
                physics=Physics(fixture.read_bytes(),{'P':(-30,0,30),'Q':(-45,0,45)});expected=[]
                for i in range(7):
                    candidate=physics.prepare(i*33333334,{'P':15} if i==1 else {},enabled=i!=4,reset=i in (0,5))
                    expected.append([f32(value) for _,value in candidate.parameters]);physics.commit(candidate)
                self.assertEqual(actual,expected)

if __name__=='__main__':unittest.main()
