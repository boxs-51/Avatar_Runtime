"""F02 RED-first: post-apply timeout must persist local UNKNOWN_OUTCOME.

All fixtures use disposable temporary files; no real model is loaded.
"""
from pathlib import Path
import tempfile
import threading
import time
import unittest

from scripts.studio_agent_api import AgentBridge


class UnknownOutcomeAdmissionTests(unittest.TestCase):
    def test_postapply_timeout_prevents_unsafe_replay(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            applied = base / "synthetic_applied.txt"
            worker_done = threading.Event()

            def crash_after_effect():
                until = time.monotonic() + 1
                request = base / "request.txt"
                while not request.exists() and time.monotonic() < until:
                    time.sleep(0.005)
                if request.exists():
                    applied.write_text("1", encoding="ascii")
                    request.unlink()
                worker_done.set()  # simulated crash before any ACK

            worker = threading.Thread(target=crash_after_effect)
            worker.start()
            with self.assertRaises(TimeoutError):
                AgentBridge(base, timeout=0.25).call(
                    {"op": "rig.select", "drawable": 0}
                )
            worker.join(timeout=1)
            self.assertTrue(worker_done.is_set())
            self.assertEqual(applied.read_text(encoding="ascii"), "1")
            self.assertTrue((base / "uncertain_outcome").exists())
            with self.assertRaisesRegex(RuntimeError, "recovery"):
                AgentBridge(base, timeout=0.05).call(
                    {"op": "rig.select", "drawable": 0}
                )
            self.assertEqual(applied.read_text(encoding="ascii"), "1")

    def test_unprocessed_timeout_remains_quarantined(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            with self.assertRaises(TimeoutError):
                AgentBridge(base, timeout=0.04).call({"op": "studio.status"})
            self.assertTrue((base / "uncertain_outcome").exists())
            with self.assertRaises(RuntimeError):
                AgentBridge(base, timeout=0.04).call({"op": "studio.status"})

    def test_complete_ack_releases_quarantine(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            def reply_once():
                request = base / "request.txt"
                until = time.monotonic() + 1
                while not request.exists() and time.monotonic() < until:
                    time.sleep(0.005)
                self.assertTrue(request.exists())
                token = request.read_text(encoding="ascii").split(" ", 1)[0]
                (base / "response.txt").write_text(
                    token + " OK status=ready\n", encoding="ascii"
                )

            first = threading.Thread(target=reply_once)
            first.start()
            got = AgentBridge(base, timeout=1).call({"op": "studio.status"})
            first.join(timeout=1)
            self.assertTrue(got["ok"])
            self.assertFalse((base / "uncertain_outcome").exists())
            second = threading.Thread(target=reply_once)
            second.start()
            got2 = AgentBridge(base, timeout=1).call({"op": "studio.status"})
            second.join(timeout=1)
            self.assertTrue(got2["ok"])
            self.assertFalse((base / "uncertain_outcome").exists())

    def test_existing_quarantine_blocks_even_empty_queue(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            marker = base / "uncertain_outcome"
            marker.write_text("synthetic-previous-intent\n", encoding="ascii")
            with self.assertRaises(RuntimeError):
                AgentBridge(base, timeout=0.05).call({"op": "studio.status"})
            self.assertTrue(marker.exists())
            self.assertFalse((base / "request.txt").exists())


    def test_orphaned_native_temp_reply_rejects_admission(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            orphan = base / "response.tmp"
            orphan.write_text("synthetic-unpublished-reply\n", encoding="ascii")
            with self.assertRaisesRegex(RuntimeError, "recovery"):
                AgentBridge(base, timeout=0.05).call({"op": "studio.status"})
            self.assertFalse((base / "request.txt").exists())
            self.assertTrue(orphan.exists())

    def test_failed_intent_fsync_cannot_publish_request(self):
        from unittest.mock import patch

        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            with patch("scripts.studio_agent_api.os.fsync", side_effect=OSError("synthetic I/O fault")):
                with self.assertRaises(OSError):
                    AgentBridge(base, timeout=0.05).call({"op": "studio.status"})
            self.assertTrue((base / "uncertain_outcome").exists())
            self.assertFalse((base / "request.txt").exists())
            with self.assertRaisesRegex(RuntimeError, "recovery"):
                AgentBridge(base, timeout=0.05).call({"op": "studio.status"})

if __name__ == "__main__":
    unittest.main()
