"""Public fabricated records only; no licensed model data in this test."""

import importlib.util
import math
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / "moc3_compare_snapshots.py"
SPEC = importlib.util.spec_from_file_location("moc3_compare_snapshots", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def snapshot():
    return {
        "parameters": [{"id": "Angle", "value": 0.0}],
        "drawables": [{
            "id": "Quad", "vertices": [
                {"pos": [0.0, 0.0], "uv": [0.0, 0.0]},
                {"pos": [1.0, 0.0], "uv": [1.0, 0.0]},
                {"pos": [0.0, 1.0], "uv": [0.0, 1.0]},
            ],
            "indices": [0, 1, 2], "opacity": 1.0,
            "texture_index": 0, "render_order": 0, "masks": [],
        }],
    }


class CompareSnapshotsTest(unittest.TestCase):
    def test_equal_and_id_order_independent(self):
        a = snapshot()
        a["parameters"].append({"id": "Eye", "value": 0.5})
        b = snapshot()
        b["parameters"] = list(reversed(a["parameters"]))
        self.assertEqual(MODULE.compare(a, b)["status"], "MATCH")

    def test_position_only_difference_identifies_vertex(self):
        a, b = snapshot(), snapshot()
        b["drawables"][0]["vertices"][1]["pos"][0] += 0.25
        result = MODULE.compare(a, b)
        self.assertEqual(result["first_difference"],
                         {"stage": "position", "id": "Quad", "vertex": 1})
        self.assertEqual(result["position_mismatches"], 1)
        self.assertEqual(result["uv_mismatches"], 0)
        self.assertEqual(result["topology_mismatches"], 0)

    def test_uv_and_index_changes_are_not_called_deformation_only(self):
        a, b = snapshot(), snapshot()
        b["drawables"][0]["indices"] = [0, 2, 1]
        b["drawables"][0]["vertices"][1]["uv"][0] = 0.75
        result = MODULE.compare(a, b)
        self.assertEqual(result["first_difference"]["stage"], "topology")
        self.assertEqual(result["topology_mismatches"], 1)
        self.assertEqual(result["uv_mismatches"], 1)

    def test_truncated_or_invalid_exports_rejected(self):
        a = snapshot()
        a["drawables"][0]["indices"] = {"truncated": True, "data": [0, 1]}
        with self.assertRaisesRegex(ValueError, "full arrays"):
            MODULE.compare(a, snapshot())
        a = snapshot()
        a["drawables"][0]["vertices"][0]["pos"][0] = math.nan
        with self.assertRaisesRegex(ValueError, "finite number"):
            MODULE.compare(a, snapshot())
        a = snapshot()
        a["parameters"].append({"id": "Angle", "value": 1.0})
        with self.assertRaisesRegex(ValueError, "duplicate id"):
            MODULE.compare(a, snapshot())

    def test_missing_id_is_mismatch(self):
        a, b = snapshot(), snapshot()
        b["drawables"][0]["id"] = "Other"
        result = MODULE.compare(a, b)
        self.assertEqual(result["first_difference"]["stage"], "identity")
        self.assertEqual(result["drawable_id_missing_left"], 1)
        self.assertEqual(result["drawable_id_missing_right"], 1)

    def test_explicit_uv_and_winding_conventions(self):
        a, b = snapshot(), snapshot()
        b["drawables"][0]["vertices"][2]["uv"][1] = 0.0
        b["drawables"][0]["indices"] = [0, 2, 1]
        self.assertEqual(MODULE.compare(a, b)["status"], "MISMATCH")
        result = MODULE.compare(a, b, right_uv_v_flip=True, right_reverse_winding=True)
        self.assertEqual(result["status"], "MISMATCH")  # Other V coordinates still differ.
        b["drawables"][0]["vertices"][0]["uv"][1] = 1.0
        b["drawables"][0]["vertices"][1]["uv"][1] = 1.0
        self.assertEqual(MODULE.compare(a, b, right_uv_v_flip=True,
                                        right_reverse_winding=True)["status"], "MATCH")

    def test_triangle_reordering_preserves_multiplicity(self):
        a, b = snapshot(), snapshot()
        a["drawables"][0]["indices"] = [0, 1, 2, 0, 2, 1]
        b["drawables"][0]["indices"] = [0, 2, 1, 0, 1, 2]
        self.assertEqual(MODULE.compare(a, b)["topology_mismatches"], 1)
        self.assertEqual(MODULE.compare(a, b, triangle_order_insensitive=True)["status"], "MATCH")
        b["drawables"][0]["indices"] = [0, 2, 1]
        self.assertEqual(MODULE.compare(a, b, triangle_order_insensitive=True)["topology_mismatches"], 1)

    def test_normalization_does_not_hide_position_or_render_state_difference(self):
        a, b = snapshot(), snapshot()
        b["drawables"][0]["vertices"][0]["pos"][0] += 0.1
        b["drawables"][0]["render_order"] = 2
        result = MODULE.compare(a, b, right_uv_v_flip=True,
                                right_reverse_winding=True,
                                triangle_order_insensitive=True)
        self.assertEqual(result["position_mismatches"], 1)
        self.assertEqual(result["state_mismatches"], 1)
        self.assertEqual(result["status"], "MISMATCH")


if __name__ == "__main__":
    unittest.main()
