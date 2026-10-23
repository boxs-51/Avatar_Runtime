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
            {"op":"rig.cross","target":1,"parent":0,"child":2,"stiffness":.5,"enabled":1},
            {"op":"rig.cross","target":1,"parent":0,"child":2,"stiffness":1.2,"enabled":True},
            {"op":"rig.brush","radius":0,"weight":.5},
            {"op":"rig.brush","radius":.5,"weight":float("nan")},
            {"op":"rig.brush","radius":float("inf"),"weight":.5},
            {"op":"layer.component.adopt","component":-1},
            {"op":"layer.component.inspect","component":2048},
            {"op":"studio.play","enabled":1},
            {"op":"studio.capture","kind":"desktop"},
            {"op":"xpbd.set","name":"substeps","value":4.1},
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

    def test_extended_studio_api(self):
        cases=[
            ({"op":"layer.topology"},"LAYER_TOPOLOGY"),
            ({"op":"layer.component.relation","first":0,"second":1},"LAYER_COMPONENT_RELATION 0 1"),
            ({"op":"rig.parent","parent":0,"child":2,"stiffness":.85,"enabled":True},"RIG_PARENT 0 2 0.85 1"),
            ({"op":"rig.weight","vertex":3,"weight":.25},"RIG_WEIGHT 3 0.25"),
            ({"op":"rig.weight.inspect","vertex":3},"RIG_WEIGHT_INSPECT 3"),
            ({"op":"rig.parent.inspect","child":2},"RIG_PARENT_INSPECT 2"),
            ({"op":"rig.cross","target":1,"parent":0,"child":2,"stiffness":0.8,"enabled":True},
             "RIG_CROSS 1 0 2 0.8 1"),
            ({"op":"rig.cross.inspect"},"RIG_CROSS_INSPECT"),
            ({"op":"rig.brush","radius":1.25,"weight":0.4},"RIG_BRUSH 1.25 0.4"),
            ({"op":"layer.topology.scan"},"LAYER_TOPOLOGY_SCAN"),
            ({"op":"layer.component.inspect","component":0},"LAYER_COMPONENT_INSPECT 0"),
            ({"op":"layer.component.adopt","component":0},"LAYER_COMPONENT_ADOPT 0"),
            ({"op":"studio.status"},"STUDIO_STATUS"),
            ({"op":"studio.play","enabled":False},"STUDIO_PLAY 0"),
            ({"op":"studio.step"},"STUDIO_STEP"),
            ({"op":"studio.expression","index":5},"STUDIO_EXPRESSION 5"),
            ({"op":"studio.toggle","feature":"blink","enabled":True},"STUDIO_TOGGLE blink 1"),
            ({"op":"studio.setting","name":"motion_speed","value":1.2},"STUDIO_SETTING motion_speed 1.2"),
            ({"op":"studio.camera","mode":"fit"},"STUDIO_CAMERA fit"),
            ({"op":"studio.zoom","value":1.6},"STUDIO_ZOOM 1.6"),
            ({"op":"studio.pan","x":12,"y":-5},"STUDIO_PAN 12 -5"),
            ({"op":"studio.reset"},"STUDIO_RESET"),
            ({"op":"studio.preset","action":"save"},"STUDIO_PRESET save"),
            ({"op":"studio.capture","kind":"viewport"},"STUDIO_CAPTURE viewport"),
            ({"op":"param.set","index":3,"value":.6},"PARAM_SET 3 0.6"),
            ({"op":"param.clear","index":3},"PARAM_CLEAR 3"),
            ({"op":"param.reset"},"PARAM_RESET"),
            ({"op":"part.opacity","index":1,"value":.5},"PART_OPACITY 1 0.5"),
            ({"op":"rig.vertex","index":2},"RIG_VERTEX 2"),
            ({"op":"rig.store","action":"save"},"RIG_STORE save"),
            ({"op":"xpbd.set","name":"bending","value":.001},"XPBD_SET bending 0.001"),
            ({"op":"xpbd.select","first":0,"second":-1,"joint":False},"XPBD_SELECT 0 -1 0"),
            ({"op":"xpbd.apply"},"XPBD_APPLY"),
            ({"op":"xpbd.disable"},"XPBD_DISABLE"),
            ({"op":"xpbd.inspect"},"XPBD_INSPECT"),
            ({"op":"studio.metrics"},"STUDIO_METRICS"),
        ]
        for command,wire in cases:
            with self.subTest(op=command["op"]):
                self.assertEqual(validate_command(command),wire)
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
