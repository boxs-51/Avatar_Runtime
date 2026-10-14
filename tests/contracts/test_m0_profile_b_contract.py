#!/usr/bin/env python3
"""M0 Profile B cross-file design-contract tests (not an asset decoder)."""
from __future__ import annotations
import copy, json, math, unittest
from pathlib import Path
from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[2]
def source(path):
    return json.loads((ROOT / path).read_text(encoding="utf-8"))

class BadAsset(ValueError):
    pass

class Unsupported(ValueError):
    pass

def require(value):
    if not value: raise BadAsset("ARS_E_BAD_ASSET")

def verify_design(manifest, rig):
    """Independent cross-file reference oracle; G02 actual PNG/ZIP/GLB remains HOLD."""
    def finite(node):
        if type(node) is float: require(math.isfinite(node))
        if isinstance(node, list):
            for v in node: finite(v)
        elif isinstance(node, dict):
            for v in node.values(): finite(v)
    finite(manifest); finite(rig)
    for schema, payload in (("schemas/character_manifest_v0.1.schema.json", manifest),
                            ("schemas/ars2d_rig_v0.1.schema.json", rig)):
        validator = Draft202012Validator(source(schema))
        require(not list(validator.iter_errors(payload)))
    layers = manifest["visual_layers"]
    layer_ids = [x["id"] for x in layers]
    parts = rig["parts"]
    part_ids = [p["id"] for p in parts]
    require(len(layer_ids) == len(set(layer_ids)))
    require(len(part_ids) == len(set(part_ids)))
    by_layer = {x["id"]: x for x in layers}
    for layer in layers:
        seen = set()
        current = layer["id"]
        while current:
            require(current not in seen)
            seen.add(current)
            current = by_layer[current].get("parent", "") if current in by_layer else None
            require(current is not None)
    representation = manifest["representations"]["2d"]
    bindings = representation["layer_bindings"]
    seen_layers = set(); seen_parts = set()
    for binding in bindings:
        lid = binding["layer_id"]
        require(lid in by_layer and lid not in seen_layers)
        require(by_layer[lid]["mode_scope"] in ("2d", "both"))
        seen_layers.add(lid)
        require(len(binding["part_ids"]) >= 1)
        for part in binding["part_ids"]:
            require(part in part_ids and part not in seen_parts)
            seen_parts.add(part)
    # Each drawable 2D layer and each rig part has one unambiguous owner.
    require(seen_layers == {x["id"] for x in layers if x["mode_scope"] in ("2d", "both")})
    require(seen_parts == set(part_ids))
    # Semantic BAD_ASSET checks precede disabled-feature reporting.
    disabled = any(x["mode_scope"] != "2d" or x.get("mask") is not None
                   or x.get("blend_mode", "normal") != "normal" for x in layers)
    disabled |= manifest["representations"].get("3d", {}).get("format") == "vrm1"
    if disabled: raise Unsupported("ARS_E_UNSUPPORTED")
    return True

class ProfileBContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m = source("examples/character_dual_mode.json")
        cls.r = source("examples/character2d.ars2d.json")
    def verify_mutation(self, change):
        m, r = copy.deepcopy(self.m), copy.deepcopy(self.r)
        change(m, r)
        with self.assertRaises(BadAsset): verify_design(m, r)
    def test_reference_linked(self):
        self.assertTrue(verify_design(self.m, self.r))
    def test_missing_layer_binding(self):
        self.verify_mutation(lambda m,r: m["representations"]["2d"]["layer_bindings"].pop())
    def test_unknown_layer(self):
        self.verify_mutation(lambda m,r: m["representations"]["2d"]["layer_bindings"][0].update(layer_id="missing"))
    def test_unknown_part(self):
        self.verify_mutation(lambda m,r: m["representations"]["2d"]["layer_bindings"][0].update(part_ids=["missing"]))
    def test_duplicate_part_cross_layer(self):
        self.verify_mutation(lambda m,r: m["representations"]["2d"]["layer_bindings"][1].update(part_ids=["base.body"]))
    def test_duplicate_layer_binding(self):
        self.verify_mutation(lambda m,r: m["representations"]["2d"]["layer_bindings"].append(copy.deepcopy(m["representations"]["2d"]["layer_bindings"][0])))
    def test_unbound_rig_part(self):
        self.verify_mutation(lambda m,r: m["representations"]["2d"]["layer_bindings"][1].update(part_ids=["base.body"]))
    def test_parent_cycle(self):
        def change(m,r):
            m["visual_layers"][0]["parent"] = m["visual_layers"][1]["id"]
            m["visual_layers"][1]["parent"] = m["visual_layers"][0]["id"]
        self.verify_mutation(change)
    def test_nonfinite_layer_opacity(self):
        self.verify_mutation(lambda m,r: m["visual_layers"][0].update(opacity=float("nan")))
    def test_disallow_shared_or_unmapped_part(self):
        self.verify_mutation(lambda m,r: m["representations"]["2d"]["layer_bindings"][0].update(part_ids=["base.body","face.mouth"]))
    def test_no_silent_both_mode_scope(self):
        m = copy.deepcopy(self.m); m["visual_layers"][0]["mode_scope"]="both"
        with self.assertRaises(Unsupported): verify_design(m, self.r)
    def test_wellformed_mask_is_unsupported(self):
        m = copy.deepcopy(self.m); m["visual_layers"][0]["mask"]="face.mouth"
        with self.assertRaises(Unsupported): verify_design(m, self.r)
    def test_invalid_binding_precedes_disabled_mask(self):
        m = copy.deepcopy(self.m); m["visual_layers"][0]["mask"]="face.mouth"
        m["representations"]["2d"]["layer_bindings"][0]["part_ids"]=["does.not.exist"]
        with self.assertRaises(BadAsset): verify_design(m, self.r)
    def test_vrm1_wellformed_but_disabled(self):
        m = copy.deepcopy(self.m); m["representations"]["3d"]["format"]="vrm1"
        with self.assertRaises(Unsupported): verify_design(m, self.r)

if __name__ == "__main__": unittest.main()
