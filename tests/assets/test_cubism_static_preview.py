import importlib.util
import json
import pathlib
import struct
import shutil
import unittest
import uuid
import zipfile
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("cubism_static_preview", ROOT / "scripts/cubism_static_preview.py")
CONVERTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONVERTER)


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def png():
    header = struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(b"\0\xff\0\0\xff")) + chunk(b"IEND", b""))


class ConverterTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / "build" / "cubism-preview-tmp"
        scratch.mkdir(parents=True, exist_ok=True)
        self.root = scratch / uuid.uuid4().hex
        self.root.mkdir()
        self.addCleanup(shutil.rmtree, self.root)
        (self.root / "model.moc3").write_bytes(b"MOC3test")
        (self.root / "texture.png").write_bytes(png())
        (self.root / "portrait.png").write_bytes(png())
        self.model = self.root / "sample.model3.json"
        self.payload = {"Version": 3, "FileReferences": {"Moc": "model.moc3", "Textures": ["texture.png"]}}
        self.write_model()

    def write_model(self):
        self.model.write_text(json.dumps(self.payload), encoding="utf-8")

    def test_deterministic_static_bundle(self):
        first, second = self.root / "first.arschar", self.root / "second.arschar"
        CONVERTER.convert(self.model, self.root / "portrait.png", first)
        CONVERTER.convert(self.model, self.root / "portrait.png", second)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        with zipfile.ZipFile(first) as archive:
            self.assertEqual(archive.namelist(), ["character.json", "assets/character2d.ars2d.json", "assets/atlas.png"])
            manifest = json.loads(archive.read("character.json"))
            rig = json.loads(archive.read("assets/character2d.ars2d.json"))
            self.assertEqual(manifest["representations"]["2d"]["mappings"], [])
            self.assertEqual(rig["parameters"], [])
            self.assertEqual(rig["canvas"], {"width": 1, "height": 1})
            self.assertEqual(archive.read("assets/atlas.png"), png())
            for kind, value in (("character_manifest_v0.1", manifest), ("ars2d_rig_v0.1", rig)):
                try:
                    import jsonschema
                except ImportError:
                    continue
                schema = json.loads((ROOT / "schemas" / f"{kind}.schema.json").read_text())
                jsonschema.Draft202012Validator(schema).validate(value)

    def test_rejects_missing_and_escaping_references(self):
        for name in ("../outside.moc3", "missing.moc3"):
            with self.subTest(name=name):
                self.payload["FileReferences"]["Moc"] = name
                self.write_model()
                with self.assertRaises(ValueError):
                    CONVERTER.convert(self.model, self.root / "portrait.png", self.root / "bad.arschar")

    def test_rejects_atlas_as_non_png_preview(self):
        (self.root / "portrait.png").write_bytes(b"invalid")
        with self.assertRaises(ValueError):
            CONVERTER.convert(self.model, self.root / "portrait.png", self.root / "bad.arschar")


if __name__ == "__main__":
    unittest.main()
