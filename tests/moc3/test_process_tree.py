"""Real Windows descendant retirement, independent of private models/GPU."""
import os
from pathlib import Path
import subprocess
import sys
import time
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_process_tree import ProcessTree


@unittest.skipUnless(os.name=="nt","Windows job lifetime")
class ProcessTreeTests(unittest.TestCase):
    def test_gated_runner_and_descendant_both_retire_before_cancel_completes(self):
        tree=ProcessTree()
        process=None
        code=("import subprocess,sys,time;sys.path.insert(0,'scripts');"
              "from moc3_process_tree import wait_launch_gate;wait_launch_gate();"
              "child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)']);"
              "print('DESCENDANT_READY',flush=True);time.sleep(60)")
        try:
            process=subprocess.Popen([sys.executable,"-c",code],cwd=ROOT,env=tree.environment(),
                                     stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
            tree.attach_and_release(process.pid)
            self.assertEqual(process.stdout.readline().strip(),"DESCENDANT_READY")
            self.assertGreaterEqual(tree.active(),2)
            tree.terminate()
            self.assertTrue(tree.wait_empty())
            process.wait(timeout=5)
            self.assertEqual(tree.active(),0)
        finally:
            tree.terminate()
            if process is not None:
                process.wait(timeout=5)
                process.stdout.close()
            tree.close()


if __name__=="__main__":
    unittest.main()
