"""RED-first controls for MOC3 editing adapter (no model assets needed)."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "moc3_viewer.py"
spec = importlib.util.spec_from_file_location("lps_moc3_viewer", SOURCE)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class FakeBridge:
    def __init__(self, messages):
        self.messages = list(messages)
        self.sent = []
    def send(self, command):
        self.sent.append(command)
    def response(self):
        return self.messages.pop(0)


class Moc3EditorContracts(unittest.TestCase):
    def test_manifest_required(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "model.moc3").write_bytes(b"MOC3")
            with self.assertRaises(ValueError):
                module.validate_model3(root / "model.moc3")
            (root / "avatar.model3.json").write_text("{}", encoding="utf-8")
            self.assertEqual(module.validate_model3(root / "avatar.model3.json").name,
                             "avatar.model3.json")

    def test_drawable_inventory(self):
        bridge = FakeBridge([
            ["LPS_LIST", "2"],
            ["LPS_DRAWABLE", "0", "18", "486169725f4c"],  # Hair_L
            ["LPS_DRAWABLE", "1", "6", "436c6f7468"],   # Cloth
        ])
        self.assertEqual(module.decode_drawables(bridge),
                         [(0, 18, "Hair_L"), (1, 6, "Cloth")])
        self.assertEqual(bridge.sent, ["LPS_LIST"])

    def test_rejects_spoofed_inventory(self):
        for rows in (
            [["LPS_ERROR", "reject"]],
            [["LPS_LIST", "500000"]],
            [["LPS_LIST", "1"], ["LPS_DRAWABLE", "99", "2", "41"]],
            [["LPS_LIST", "1"], ["LPS_DRAWABLE", "0", "-1", "41"]],
            [["LPS_LIST", "1"], ["LPS_DRAWABLE", "0", "1", "zz"]],
        ):
            with self.subTest(rows=rows), self.assertRaises((RuntimeError, ValueError)):
                module.decode_drawables(FakeBridge(rows))


if __name__ == "__main__":
    unittest.main()
