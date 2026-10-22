import importlib.util
import json
import pathlib
import shutil
import struct
import unittest
import uuid
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("cubism_preflight", ROOT / "scripts/cubism_preflight.py")
PREFLIGHT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREFLIGHT)


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def png(width=1, height=1):
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(b"\0\xff\0\0\xff")) + chunk(b"IEND", b""))


class PreflightTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / "build" / "cubism-preflight-tmp"
        scratch.mkdir(parents=True, exist_ok=True)
        self.root = scratch / uuid.uuid4().hex
        self.root.mkdir()
        self.addCleanup(shutil.rmtree, self.root)
        (self.root / "sample.moc3").write_bytes(b"MOC3sample")
        (self.root / "atlas.png").write_bytes(png())
        self.model = self.root / "sample.model3.json"
        self.payload = {"Version": 3, "FileReferences": {"Moc": "sample.moc3", "Textures": ["atlas.png"]}}
        self.write_model()

    def write_model(self):
        self.model.write_text(json.dumps(self.payload), encoding="utf-8")

    def rejected(self, code):
        with self.assertRaises(PREFLIGHT.PreflightError) as error:
            PREFLIGHT.inspect(self.model)
        self.assertEqual(error.exception.code, code)

    def test_safe_inventory_is_explicitly_not_core_validation(self):
        result = PREFLIGHT.inspect(self.model)
        self.assertEqual(result["texture_dimensions"], [[1, 1]])
        self.assertEqual(result["decoded_rgba_bytes"], 4)
        self.assertEqual(result["moc_core_validation"], "NOT_RUN")
        self.assertEqual(result["png_validation"], "HEADER_ONLY")
        self.assertEqual(result["dynamic_readiness"], "HOLD_SDK")

    def test_path_escape_missing_and_duplicate_texture(self):
        for path, code in (("../outside.moc3", "INVALID_REFERENCE"),
                           ("missing.moc3", "MISSING_OR_ESCAPING_REFERENCE")):
            with self.subTest(path=path):
                self.payload["FileReferences"]["Moc"] = path
                self.write_model()
                self.rejected(code)
        self.payload["FileReferences"]["Moc"] = "sample.moc3"
        self.payload["FileReferences"]["Textures"] = ["atlas.png", "atlas.png"]
        self.write_model()
        self.rejected("DUPLICATE_REFERENCE")

    def test_bad_moc_and_png_headers(self):
        (self.root / "sample.moc3").write_bytes(b"bad")
        self.rejected("INVALID_MOC_HEADER")
        (self.root / "sample.moc3").write_bytes(b"MOC3sample")
        image = bytearray(png())
        image[29] ^= 1
        (self.root / "atlas.png").write_bytes(image)
        self.rejected("INVALID_PNG_HEADER")

    def test_duplicate_json_key_and_bad_optional_json(self):
        self.model.write_text('{"Version":3,"Version":3,"FileReferences":{}}', encoding="utf-8")
        self.rejected("DUPLICATE_JSON_KEY")
        self.payload["FileReferences"]["Physics"] = "physics.json"
        self.write_model()
        (self.root / "physics.json").write_text("[]", encoding="utf-8")
        self.rejected("INVALID_LINKED_JSON")

    def test_rejects_unsupported_reference_kind(self):
        self.payload["FileReferences"]["Unknown"] = "ignored.bin"
        self.write_model()
        self.rejected("UNSUPPORTED_REFERENCE_KIND")

    def test_declared_decode_budget_is_checked_before_loading_pixels(self):
        for i in range(3):
            (self.root / f"atlas{i}.png").write_bytes(png(8192, 8192))
        self.payload["FileReferences"]["Textures"] = [f"atlas{i}.png" for i in range(3)]
        self.write_model()
        self.rejected("TEXTURE_BUDGET_EXCEEDED")


if __name__ == "__main__":
    unittest.main()
