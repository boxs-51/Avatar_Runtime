"""Windows process-kill oracle for F02 local pending-intent quarantine.

The child is a disposable synthetic IPC client; no real model is accessed.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]


class PhysicalClientCrashTests(unittest.TestCase):
    def test_applied_command_and_killed_client_remains_quarantined(self):
        from scripts.studio_agent_api import AgentBridge

        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            child_code = (
                "import sys\n"
                "from pathlib import Path\n"
                "from scripts.studio_agent_api import AgentBridge\n"
                "AgentBridge(Path(sys.argv[1]),timeout=20).call("
                "{'op':'rig.select','drawable':0})\n"
            )
            child = subprocess.Popen(
                [sys.executable, "-c", child_code, str(folder)],
                cwd=ROOT,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            try:
                request = folder / "request.txt"
                intent = folder / "uncertain_outcome"
                deadline = time.monotonic() + 4
                while (not request.exists() or not intent.exists()) and time.monotonic() < deadline:
                    if child.poll() is not None:
                        break
                    time.sleep(0.01)
                self.assertTrue(request.exists(), "no published request")
                self.assertTrue(intent.exists(), "intent not durable before publish")
                # Simulate a native mutation, then Native disappears before ACK.
                applied = folder / "simulated_effect.txt"
                applied.write_text("applied-once", encoding="ascii")
                request.unlink()
                child.kill()
                child.wait(timeout=3)
                self.assertTrue(intent.exists())
                # Windows may release the killed process's byte-range lock asynchronously.
                # Prove the persistent marker blocks admission AFTER the lock is free.
                quarantine_seen = False
                deadline = time.monotonic() + 2
                while time.monotonic() < deadline:
                    try:
                        AgentBridge(folder, timeout=0.05).call(
                            {"op": "rig.select", "drawable": 0}
                        )
                    except RuntimeError as exc:
                        if "recovery" in str(exc):
                            quarantine_seen = True
                            break
                    time.sleep(0.02)
                self.assertTrue(quarantine_seen, "quarantine not enforced after lock release")
                self.assertEqual(applied.read_text(encoding="ascii"), "applied-once")
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait(timeout=3)


if __name__ == "__main__":
    unittest.main()
