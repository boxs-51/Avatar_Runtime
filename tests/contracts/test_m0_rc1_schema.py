#!/usr/bin/env python3
"""M0 RC1 DESIGN-ONLY schema/semantic checks; NOT a ZIP/GLB runtime decoder."""
from __future__ import annotations

import copy
import json
import math
from pathlib import Path
import unittest

from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_MANIFEST = ROOT / "schemas/character_manifest_v0.1.schema.json"
SCHEMA_RIG = ROOT / "schemas/ars2d_rig_v0.1.schema.json"
MANIFEST = ROOT / "examples/character_dual_mode.json"
RIG = ROOT / "examples/character2d.ars2d.json"

class AssetError(Exception):
    def __init__(self, status: str):
        self.status = status
        super().__init__(status)

def strict_json(path: Path):
    def unique_pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise AssetError("ARS_E_BAD_ASSET")
            result[key] = value
        return result
    def reject_constant(value):
        raise AssetError("ARS_E_BAD_ASSET")
    return json.loads(path.read_text(encoding="utf-8"),
                      object_pairs_hook=unique_pairs,
                      parse_constant=reject_constant)

def require(cond, status="ARS_E_BAD_ASSET"):
    if not cond:
        raise AssetError(status)

def unique_ids(rows):
    ids = [row["id"] for row in rows]
    require(len(ids) == len(set(ids)))
    return set(ids)

def check_numeric(value):
    require(type(value) in (int, float) and math.isfinite(value))

def check_semantics(manifest: dict, rig: dict):
    # This is a DESIGN FIXTURE CHECK ONLY: referenced atlas/GLB bytes do not
    # exist in this repository. Do not confuse with full asset validation.
    require(manifest["default_mode"] in manifest["representations"])
    sem = unique_ids(manifest["rig_semantics"])
    layers = unique_ids(manifest["visual_layers"])
    layer_dict = {x["id"]: x for x in manifest["visual_layers"]}
    for layer in manifest["visual_layers"]:
        parent = layer.get("parent")
        if parent is not None:
            require(parent in layers and parent != layer["id"])
        if "mask" in layer:
            raise AssetError("ARS_E_UNSUPPORTED")  # RC1 Profile B
    for layer in manifest["visual_layers"]:
        seen = set()
        cur = layer["id"]
        while cur:
            require(cur not in seen)
            seen.add(cur)
            cur = layer_dict[cur].get("parent")
    for value in manifest["rig_semantics"]:
        typ, default = value["type"], value.get("default")
        if typ == "float":
            check_numeric(default)
            if "min" in value:
                check_numeric(value["min"])
                require(default >= value["min"])
            if "max" in value:
                check_numeric(value["max"])
                require(default <= value["max"])
            if "min" in value and "max" in value:
                require(value["min"] <= value["max"])
        elif typ == "bool":
            require(type(default) is bool)
        elif typ in ("vec2", "vec3", "quat"):
            size = {"vec2":2, "vec3":3, "quat":4}[typ]
            require(isinstance(default, list) and len(default) == size)
            for component in default:
                check_numeric(component)
        elif typ == "trigger":
            require(default is None)
        else:
            raise AssetError("ARS_E_BAD_ASSET")

    representations = manifest["representations"]
    for mode, rep in representations.items():
        for mapping in rep["mappings"]:
            require(mapping["semantic"] in sem)
        if mode == "3d":
            if rep["format"] == "vrm1":
                raise AssetError("ARS_E_UNSUPPORTED")
            require(rep["format"] == "glb-gltf2")
    if "2d" not in representations:
        return
    params = unique_ids(rig["parameters"])
    part_ids = unique_ids(rig["parts"])
    for repmap in representations["2d"]["mappings"]:
        require(repmap["target"] in params)
    for param in rig["parameters"]:
        for field in ("min", "max", "default"):
            check_numeric(param[field])
        require(param["min"] <= param["default"] <= param["max"])
    for part in rig["parts"]:
        x, y, width, height = part["atlas_rect_px"]
        require(width > 0 and height > 0)
        require(x + width <= rig["canvas"]["width"])
        require(y + height <= rig["canvas"]["height"])
        v = len(part["vertices_px"])
        require(len(part["uv"]) == v)
        for triangle in part["triangles"]:
            require(all(type(idx) is int and 0 <= idx < v for idx in triangle))
        for deform in part.get("deform_targets", []):
            require(deform["parameter"] in params)
            require(len(deform["vertex_deltas_px"]) == v)
    require(len(part_ids) > 0)

def validate_design_fixture(manifest: dict, rig: dict, manifest_schema, rig_schema, atlas_dimensions=None):
    # RED-first: dimensions are a trusted validation input; intentionally not
    # enforced until source semantics are fixed on this audit branch.
    # Structural validation precedes recognized-but-disabled features.
    if list(Draft202012Validator(manifest_schema).iter_errors(manifest)):
        raise AssetError("ARS_E_BAD_ASSET")
    if list(Draft202012Validator(rig_schema).iter_errors(rig)):
        raise AssetError("ARS_E_BAD_ASSET")
    check_semantics(manifest, rig)

class RC1SchemaContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest_schema = strict_json(SCHEMA_MANIFEST)
        cls.rig_schema = strict_json(SCHEMA_RIG)
        Draft202012Validator.check_schema(cls.manifest_schema)
        Draft202012Validator.check_schema(cls.rig_schema)
        cls.manifest = strict_json(MANIFEST)
        cls.rig = strict_json(RIG)
    def verify(self, manifest=None, rig=None, expected=None):
        if manifest is None: manifest = copy.deepcopy(self.manifest)
        if rig is None: rig = copy.deepcopy(self.rig)
        if expected:
            with self.assertRaises(AssetError) as ctx:
                validate_design_fixture(manifest, rig, self.manifest_schema, self.rig_schema)
            self.assertEqual(ctx.exception.status, expected)
        else:
            validate_design_fixture(manifest, rig, self.manifest_schema, self.rig_schema)
    def test_reference_json_schema_and_semantics(self):
        self.verify()
    def test_duplicate_semantic_id(self):
        m=copy.deepcopy(self.manifest);m["rig_semantics"].append(copy.deepcopy(m["rig_semantics"][0]));self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_float_default_boolean(self):
        m=copy.deepcopy(self.manifest);m["rig_semantics"][0]["default"]=True;self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_out_of_range_default(self):
        m=copy.deepcopy(self.manifest);m["rig_semantics"][0]["default"]=99;self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_invalid_atlas_width(self):
        rig=copy.deepcopy(self.rig);rig["parts"][0]["atlas_rect_px"][2]=0;self.verify(rig=rig,expected="ARS_E_BAD_ASSET")
    def test_uv_vertex_mismatch(self):
        rig=copy.deepcopy(self.rig);rig["parts"][0]["uv"].pop();self.verify(rig=rig,expected="ARS_E_BAD_ASSET")
    def test_triangle_index_out_of_bounds(self):
        rig=copy.deepcopy(self.rig);rig["parts"][0]["triangles"][0][2]=999;self.verify(rig=rig,expected="ARS_E_BAD_ASSET")
    def test_deform_delta_count_mismatch(self):
        rig=copy.deepcopy(self.rig);rig["parts"][1]["deform_targets"][0]["vertex_deltas_px"].pop();self.verify(rig=rig,expected="ARS_E_BAD_ASSET")
    def test_missing_semantic_mapping(self):
        m=copy.deepcopy(self.manifest);m["representations"]["2d"]["mappings"][0]["semantic"]="not.a.semantic";self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_missing_2d_parameter_target(self):
        m=copy.deepcopy(self.manifest);m["representations"]["2d"]["mappings"][0]["target"]="missing";self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_vrm_recognized_disabled(self):
        m=copy.deepcopy(self.manifest);m["representations"]["3d"]["format"]="vrm1";self.verify(m,expected="ARS_E_UNSUPPORTED")
    def test_mask_recognized_disabled(self):
        m=copy.deepcopy(self.manifest);m["visual_layers"][0]["mask"]="base.body";self.verify(m,expected="ARS_E_UNSUPPORTED")
    def test_duplicate_visual_layer(self):
        m=copy.deepcopy(self.manifest);m["visual_layers"].append(copy.deepcopy(m["visual_layers"][0]));self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_layer_parent_cycle(self):
        m=copy.deepcopy(self.manifest);m["visual_layers"][0]["parent"]="accessory.hat";m["visual_layers"][1]["parent"]="base.body";self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_default_mode_must_exist(self):
        m=copy.deepcopy(self.manifest);del m["representations"]["2d"];self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_unknown_format_is_malformed(self):
        m=copy.deepcopy(self.manifest);m["representations"]["3d"]["format"]="mystery";self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_invalid_traversal_asset_path(self):
        m=copy.deepcopy(self.manifest);m["representations"]["2d"]["asset"]="../bad";self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_malformed_and_vrm_returns_bad_asset_first(self):
        m=copy.deepcopy(self.manifest);m["representations"]["3d"]["format"]="vrm1";m["representations"]["2d"]["asset"]="../bad";self.verify(m,expected="ARS_E_BAD_ASSET")
    def test_bool_not_valid_for_numeric_2d_default(self):
        rig=copy.deepcopy(self.rig);rig["parameters"][0]["default"]=False;self.verify(rig=rig,expected="ARS_E_BAD_ASSET")
    def test_duplicate_json_key_is_rejected(self):
        def unique_pairs(pairs):
            result={}
            for k,v in pairs:
                if k in result:
                    raise AssetError("ARS_E_BAD_ASSET")
                result[k]=v
            return result
        with self.assertRaises(AssetError):
            json.loads('{"schema_version":"0.1.0","schema_version":"0.1.0"}',object_pairs_hook=unique_pairs)


    # F01: corrupted semantic cannot be masked by a disabled capability.
    def test_red_f01_bad_asset_precedes_recognized_unsupported(self):
        m = copy.deepcopy(self.manifest)
        m["visual_layers"][0]["mask"] = "base.body"
        m["rig_semantics"][0]["default"] = 99
        self.verify(m, expected="ARS_E_BAD_ASSET")

    # F03: JSON numeric exponent overflow is an invalid asset, not an infinity.
    def test_red_f03_json_exponent_overflow(self):
        import tempfile
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "over.json"
            path.write_text('{"value":1e400}', encoding="utf-8")
            with self.assertRaises(AssetError) as ctx:
                strict_json(path)
            self.assertEqual(ctx.exception.status, "ARS_E_BAD_ASSET")

    # F03: every mesh numeric field must reject non-finite values.
    def test_red_f03_uv_infinity(self):
        rig = copy.deepcopy(self.rig)
        rig["parts"][0]["uv"][0][0] = float("inf")
        self.verify(rig=rig, expected="ARS_E_BAD_ASSET")

    # F02: validated PNG actual pixels, not rig canvas, bound atlas rectangles.
    def test_red_f02_atlas_pixels_not_canvas(self):
        with self.assertRaises(AssetError) as ctx:
            validate_design_fixture(copy.deepcopy(self.manifest),
                                    copy.deepcopy(self.rig),
                                    self.manifest_schema, self.rig_schema,
                                    atlas_dimensions=(300, 300))
        self.assertEqual(ctx.exception.status, "ARS_E_BAD_ASSET")

    # F08: M0/G00 security DESIGN cannot demand a production registry/mixer.
    def test_red_f08_g00_p0_c_no_production_implementation_deadlock(self):
        gates = (ROOT / "docs/P0_SECURITY_REGRESSION_GATES_V0.1.md").read_text(encoding="utf-8")
        self.assertNotIn("before production M1, minimal registry/mixer implementation passes", gates)

if __name__ == "__main__":
    unittest.main()
