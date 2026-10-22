"""RED-first contract for local agent API; no private model in these fixtures."""
from __future__ import annotations
import tempfile
import threading
import time
import unittest
from pathlib import Path

import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from studio_agent_api import AgentBridge, validate_command, VERSION


class AgentApiContract(unittest.TestCase):
    def test_public_command_contract_and_bounds(self):
        self.assertEqual(VERSION, "1.0")
        self.assertEqual(validate_command({"op": "rig.select", "drawable": 7}), "RIG_SELECT 7")
        self.assertEqual(validate_command({"op": "rig.anchor", "vertex": 12, "enabled": True}), "RIG_ANCHOR 12 1")
        self.assertEqual(validate_command({"op": "rig.beam", "a": 2, "b": 9, "enabled": False}), "RIG_BEAM 2 9 0")
        self.assertEqual(validate_command({"op": "rig.apply"}), "RIG_APPLY")
        self.assertEqual(validate_command({"op": "rig.inspect"}), "RIG_INSPECT")
        self.assertEqual(validate_command({"op": "layer.scan"}), "LAYER_SCAN")
        self.assertEqual(validate_command({"op": "layer.suggest"}), "LAYER_SUGGEST")
        self.assertEqual(validate_command({"op": "layer.adopt"}), "LAYER_ADOPT")
        self.assertEqual(validate_command({"op": "layer.label", "kind": "skirt"}), "LAYER_LABEL skirt")
        self.assertEqual(validate_command({"op": "physics.set", "gravity": -12.5, "wind": 3, "damping": .95}), "PHYSICS_SET -12.5 3 0.95")
        self.assertEqual(validate_command({"op": "action.trigger", "kind": "pull", "strength": .6}), "ACTION pull 0.6")
        for invalid in (
            {"op":"unknown"},
            {"op":"layer.label","kind":"body"},
            {"op":"layer.label","kind":True},
            {"op":"layer.scan","path":"../private"},
            {"op":"rig.select","drawable":-1},
            {"op":"rig.anchor","vertex":3,"enabled":"yes"},
            {"op":"rig.beam","a":2,"b":2,"enabled":True},
            {"op":"rig.apply","file":"../../private"},
            {"op":"physics.set","gravity":float("nan"),"wind":1,"damping":.5},
            {"op":"action.trigger","kind":"shell","strength":1},
            {"op":"action.trigger","kind":"pull","strength":3},
        ):
            with self.subTest(invalid=invalid),self.assertRaises(ValueError):
                validate_command(invalid)

    def test_live_bridge_receipt_correlates_and_removes_request(self):
        with tempfile.TemporaryDirectory() as root:
            folder=Path(root)
            bridge=AgentBridge(folder, timeout=1.5)
            def fake_studio():
                deadline=time.monotonic()+1.3
                while time.monotonic()<deadline:
                    request=folder/"request.txt"
                    if request.exists():
                        first=request.read_text(encoding="ascii").strip()
                        req_id, verb=first.split(" ",1)
                        self.assertEqual(verb,"RIG_SELECT 4")
                        (folder/"response.txt").write_text(req_id+" OK drawable=4\n",encoding="ascii")
                        return
                    time.sleep(.01)
                self.fail("Studio never received request")
            worker=threading.Thread(target=fake_studio)
            worker.start()
            result=bridge.call({"op":"rig.select","drawable":4})
            worker.join(timeout=2)
            self.assertEqual(result,{"ok":True,"receipt":"drawable=4"})
            self.assertFalse((folder/"request.txt").exists())
            self.assertFalse((folder/"response.txt").exists())

    def test_no_studio_is_not_false_success(self):
        with tempfile.TemporaryDirectory() as root:
            bridge=AgentBridge(Path(root),timeout=.09)
            with self.assertRaises(TimeoutError):
                bridge.call({"op":"rig.apply"})

if __name__ == "__main__":
    unittest.main()
