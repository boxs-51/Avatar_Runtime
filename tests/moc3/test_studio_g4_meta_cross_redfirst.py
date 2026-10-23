"""G4-META-05 / CROSS-06 / TEST-07: explicit RED-first native-contract oracles."""
import json
import sys
import unittest
from pathlib import Path
from scripts.avatar_toolkit_v2 import execute_request
from scripts.studio_agent_api import validate_command

class Bridge:
    def __init__(self, *, topology="valid", target_vertices=5, target_reply=1):
        self.topology=topology
        self.target_vertices=target_vertices
        self.target_reply=target_reply
        self.calls=[]
    def call(self, command):
        op=command["op"]
        self.calls.append(dict(command))
        if op=="studio.epoch":
            return {"ok":True,"receipt":"generation=55"}
        if op=="studio.status":
            return {"ok":True,"receipt":"frame=1 playing=0 expression=0 tab=3 xpbd=0 params=1 parts=1 drawables=2"}
        if command.get("generation")!=55:
            return {"ok":False,"receipt":"wrong generation"}
        if op=="g4.rig.revision":
            return {"ok":True,"receipt":"revision=7"}
        if op=="g4.rig.meta":
            return {"ok":True,"receipt":"drawable=0 vertices=3 role=0 anchors=0 beams=0 parents=0 weights=0 cross=1"}
        if op=="g4.rig.entry":
            return {"ok":True,"receipt":"kind=cross index=0 parent=1 target=1 child=4 stiffness=.5"}
        if op=="g4.mesh.vertex_count":
            return {"ok":True,"receipt":f"drawable={self.target_reply} vertices={self.target_vertices}"}
        if op=="g4.mesh.meta":
            if command["drawable"]==1 and self.topology=="unavailable":
                return {"ok":False,"receipt":"no triangle topology"}
            cases={
                "valid":"vertices=5 triangles=1 islands=1 used=3 loose=2 largest=3",
                "islands":"vertices=5 triangles=1 islands=9000 used=3 loose=2 largest=99",
                "largest":"vertices=5 triangles=1 islands=1 used=3 loose=2 largest=99",
                "no_islands":"vertices=5 triangles=1 islands=0 used=3 loose=2 largest=3",
                "triangle_islands":"vertices=5 triangles=1 islands=2 used=5 loose=0 largest=3",
                "unavailable":"vertices=5 triangles=1 islands=1 used=3 loose=2 largest=3",
            }
            return {"ok":True,"receipt":f"drawable={command['drawable']} "+cases[self.topology]}
        raise AssertionError(f"unexpected Native opcode {op}")

def read(op, args, bridge):
    data={"api_version":2,"request_id":"req-g4-meta-05","session_id":None,
          "expected_model_generation":55,"expected_edit_revision":None,
          "operation":op,"arguments":args,"mode":"read"}
    return json.loads(execute_request(json.dumps(data).encode(),
                local_bridge=bridge,allow_local_private=True))

class G4NewDefects(unittest.TestCase):
    def test_topology_invalid_fails_closed(self):
        for scenario in ("islands","largest","no_islands","triangle_islands"):
            with self.subTest(scenario=scenario):
                result=read("mesh.topology.inspect",{"drawable":0},Bridge(topology=scenario))
                self.assertEqual(result["state"],"REJECTED")
                self.assertEqual(result["error"]["code"],"INVALID_DRAFT")
                self.assertIsNone(result["result"])

    def test_valid_topology_control(self):
        response=read("mesh.topology.inspect",{"drawable":0},Bridge())
        self.assertEqual(response["state"],"COMPLETED_READ")
        self.assertEqual(response["result"]["topology"]["largest"],3)

    def test_cross_reads_target_vertex_count_only(self):
        probe=Bridge(topology="unavailable")
        result=read("rig.entries.page",{"drawable":0,"kind":"cross","start":0,"limit":1},probe)
        self.assertEqual(result["state"],"COMPLETED_READ")
        self.assertEqual(result["result"]["items"][0]["child"],4)
        self.assertEqual([x["op"] for x in probe.calls if x["op"].startswith("g4.mesh.")],
                         ["g4.mesh.vertex_count"])

    def test_target_identity_must_match(self):
        probe=Bridge(target_reply=0)
        result=read("rig.entries.page",{"drawable":0,"kind":"cross","start":0,"limit":1},probe)
        self.assertEqual(result["state"],"REJECTED")
        self.assertEqual(result["error"]["code"],"STALE_MODEL")
        self.assertIsNone(result["result"])

    def test_target_child_bound_still_enforced(self):
        probe=Bridge(target_vertices=4)
        result=read("rig.entries.page",{"drawable":0,"kind":"cross","start":0,"limit":1},probe)
        self.assertEqual(result["state"],"REJECTED")
        self.assertEqual(result["error"]["code"],"INVALID_DRAFT")
        self.assertIsNone(result["result"])

    def test_strict_native_vertex_count_wire(self):
        self.assertEqual(validate_command({"op":"g4.mesh.vertex_count","generation":55,"drawable":1}),
                         "G4_MESH_VERTEX_COUNT 55 1")
        for malformed in (
            {"op":"g4.mesh.vertex_count","generation":55,"drawable":-1},
            {"op":"g4.mesh.vertex_count","generation":55,"drawable":True},
            {"op":"g4.mesh.vertex_count","generation":0,"drawable":1},
            {"op":"g4.mesh.vertex_count","generation":55,"drawable":1,"extra":"invalid"}):
            with self.subTest(malformed=malformed):
                with self.assertRaises(ValueError):
                    validate_command(malformed)

    def test_physical_e2e_requires_strictly_larger_target(self):
        sys.path.insert(0,str(Path("scripts").resolve()))
        import test_studio_g4_cross_live as native
        self.assertIsNone(native.select_strict_target(6,[(1,2),(2,6)]))
        self.assertEqual(native.select_strict_target(6,[(1,2),(2,9)]),(2,9))
        self.assertIsNone(native.select_strict_target(6,[]))

if __name__=="__main__":
    unittest.main()
