"""Generated public fixtures for the trusted-local F1 CPU preflight."""

import json
import os
from pathlib import Path
import shutil
import struct
import sys
import unittest
import uuid
import zlib


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from moc3_package_admission import (  # noqa: E402
    AdmissionError, BudgetPolicy, admit_package, decode_png_subset,
)


def chunk(kind: bytes, data: bytes) -> bytes:
    return (struct.pack(">I", len(data)) + kind + data +
            struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))


def png(width=2, height=1, color=6, filter_type=0) -> bytes:
    channels = 4 if color == 6 else 3
    pixels = bytes((15, 40, 80, 255, 90, 30, 5, 128)) if color == 6 else \
        bytes((15, 40, 80, 90, 30, 5))
    assert width == 2 and height == 1
    row = bytearray(pixels)
    if filter_type == 1:
        for index in range(len(row) - 1, channels - 1, -1):
            row[index] = (row[index] - pixels[index - channels]) & 0xff
    elif filter_type == 3:
        for index in range(len(row) - 1, channels - 1, -1):
            row[index] = (row[index] - pixels[index - channels] // 2) & 0xff
    elif filter_type == 4:
        for index in range(len(row) - 1, channels - 1, -1):
            row[index] = (row[index] - pixels[index - channels]) & 0xff
    header = struct.pack(">IIBBBBB", width, height, 8, color, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
            chunk(b"IDAT", zlib.compress(bytes((filter_type,)) + row)) +
            chunk(b"IEND", b""))


class PackageAdmissionTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / "build" / "moc3-admission-tests"
        scratch.mkdir(parents=True, exist_ok=True)
        self.root = scratch / uuid.uuid4().hex
        self.root.mkdir()
        self.addCleanup(self.cleanup_root, scratch)
        (self.root / "model.moc3").write_bytes(b"MOC3" + bytes(64))
        (self.root / "a.png").write_bytes(png())
        self.manifest = {"Version": 3, "FileReferences": {
            "Moc": "model.moc3", "Textures": ["a.png"]}}
        self.write_manifest()

    def cleanup_root(self, scratch):
        resolved = self.root.resolve()
        if not resolved.is_relative_to(scratch.resolve()) or resolved == scratch.resolve():
            raise RuntimeError("Refusing test cleanup outside scratch root")
        shutil.rmtree(resolved)

    def write_manifest(self):
        (self.root / "model.model3.json").write_text(json.dumps(self.manifest),
                                                       encoding="utf-8")

    def rejects(self, code):
        with self.assertRaises(AdmissionError) as caught:
            admit_package(self.root, "model.model3.json")
        self.assertEqual(caught.exception.code, code)

    def test_minimal_package_and_texture_order(self):
        (self.root / "b.png").write_bytes(png(color=2))
        self.manifest["FileReferences"]["Textures"] = ["b.png", "a.png"]
        self.write_manifest()
        package = admit_package(self.root, "model.model3.json")
        self.assertEqual(len(package.textures), 2)
        self.assertEqual(package.textures[0].rgba,
                         bytes((15, 40, 80, 255, 90, 30, 5, 255)))
        self.assertEqual(package.textures[1].rgba,
                         bytes((15, 40, 80, 255, 90, 30, 5, 128)))
        self.assertFalse(package.moc_evaluated)

    def test_png_filters_and_crc(self):
        expected = bytes((15, 40, 80, 255, 90, 30, 5, 128))
        for filter_type in range(5):
            with self.subTest(filter=filter_type):
                self.assertEqual(decode_png_subset(png(filter_type=filter_type),
                                                   BudgetPolicy()).rgba, expected)
        damaged = bytearray(png())
        damaged[-5] ^= 1
        with self.assertRaises(AdmissionError) as caught:
            decode_png_subset(bytes(damaged), BudgetPolicy())
        self.assertEqual(caught.exception.code, "INVALID_PNG_CRC")

    def test_path_duplicate_and_missing_rejected(self):
        self.manifest["FileReferences"]["Textures"] = ["../a.png"]
        self.write_manifest()
        self.rejects("INVALID_REFERENCE")
        self.manifest["FileReferences"]["Textures"] = ["a.png", "A.png"]
        self.write_manifest()
        self.rejects("DUPLICATE_REFERENCE")
        self.manifest["FileReferences"]["Textures"] = ["missing.png"]
        self.write_manifest()
        self.rejects("MISSING_REFERENCE")

    def test_budget_and_moc_header_fail_closed(self):
        with self.assertRaises(AdmissionError) as caught:
            admit_package(self.root, "model.model3.json",
                          BudgetPolicy(max_decoded_texture_bytes=4))
        self.assertEqual(caught.exception.code, "OUT_OF_BUDGET")
        (self.root / "model.moc3").write_bytes(b"FAKE" + bytes(64))
        self.rejects("INVALID_MOC_HEADER")

    def test_unreferenced_neighbor_and_linked_physics_descriptor(self):
        (self.root / "unused.png").write_bytes(b"not a png")
        (self.root / "linked.physics3.json").write_text("{}", encoding="utf-8")
        self.manifest["FileReferences"]["Physics"] = "linked.physics3.json"
        self.write_manifest()
        package = admit_package(self.root, "model.model3.json")
        self.assertEqual(package.linked_physics, "linked.physics3.json")
        self.assertEqual(len(package.textures), 1)

    def test_multirow_png_filters_have_frozen_pixel_oracle(self):
        expected = bytes((10, 20, 30, 40, 50, 60, 70, 80,
                          15, 25, 35, 45, 55, 65, 75, 85))
        rows = (
            (expected[:8], expected[8:]),
            (bytes((10,20,30,40,40,40,40,40)), bytes((15,25,35,45,40,40,40,40))),
            (expected[:8], bytes((5,)*8)),
            (bytes((10,20,30,40,45,50,55,60)), bytes((10,15,20,25,23,23,23,23))),
            (bytes((10,20,30,40,40,40,40,40)), bytes((5,)*8)),
        )
        for filter_type, pair in enumerate(rows):
            encoded = bytes((filter_type,)) + pair[0] + bytes((filter_type,)) + pair[1]
            image = (b"\x89PNG\r\n\x1a\n" +
                     chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(encoded)) + chunk(b"IEND", b""))
            self.assertEqual(decode_png_subset(image, BudgetPolicy()).rgba, expected)

    def test_disabled_features_and_explicit_requests(self):
        self.manifest["FileReferences"]["Expressions"] = [{"Name":"ignored", "File":"unused.exp3.json"}]
        self.write_manifest()
        package = admit_package(self.root, "model.model3.json")
        self.assertEqual(package.disabled_linked_features, ("Expressions",))
        self.assertEqual(package.color_policy, "RAW_ENCODED_RGBA8_NO_COLOR_TRANSFORM")
        with self.assertRaises(AdmissionError) as caught:
            admit_package(self.root, "model.model3.json", requested_features=("expressions",))
        self.assertEqual(caught.exception.code, "UNSUPPORTED_REQUESTED_FEATURE")

    def test_all_declared_allocation_checkpoints_fail_without_mutating_prior(self):
        stages = []
        prior = admit_package(self.root, "model.model3.json", fault=stages.append)
        for stage in set(stages):
            def inject(current):
                if current == stage:
                    raise MemoryError("injected")
            with self.subTest(stage=stage), self.assertRaises(AdmissionError) as caught:
                admit_package(self.root, "model.model3.json", fault=inject)
            self.assertEqual(caught.exception.code, "ALLOCATION_FAILED")
            self.assertEqual(prior.textures[0].rgba, bytes((15,40,80,255,90,30,5,128)))

    def test_working_budget_includes_retained_package_and_decode_copies(self):
        with self.assertRaises(AdmissionError) as caught:
            admit_package(self.root, "model.model3.json", BudgetPolicy(max_working_bytes=1024))
        self.assertEqual(caught.exception.code, "OUT_OF_BUDGET")
        with self.assertRaises(AdmissionError) as caught:
            admit_package(self.root, "model.model3.json", retained_bytes=768*1024*1024)
        self.assertEqual(caught.exception.code, "OUT_OF_BUDGET")

    def test_windows_ambiguous_alias_and_hardlink_duplicate_rejected(self):
        for reference in ("folder./a.png", "folder /a.png", "NUL/a.png", "a\x01.png"):
            self.manifest["FileReferences"]["Textures"] = [reference]
            self.write_manifest()
            self.rejects("INVALID_REFERENCE")
        os.link(self.root / "a.png", self.root / "alias.png")
        self.manifest["FileReferences"]["Textures"] = ["a.png", "alias.png"]
        self.write_manifest()
        self.rejects("DUPLICATE_REFERENCE")

    def test_nested_manifest_references_are_relative_to_manifest_parent(self):
        nested = self.root / "nested"
        nested.mkdir()
        (nested / "model.model3.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        (nested / "model.moc3").write_bytes(b"MOC3nested")
        (nested / "a.png").write_bytes(png(color=2))
        package = admit_package(self.root, "nested/model.model3.json")
        self.assertEqual(package.moc_bytes, b"MOC3nested")
        self.assertEqual(package.textures[0].rgba[-1], 255)
        (nested / "a.png").unlink()
        with self.assertRaises(AdmissionError) as caught:
            admit_package(self.root, "nested/model.model3.json")
        self.assertEqual(caught.exception.code, "MISSING_REFERENCE")


if __name__ == "__main__":
    unittest.main()
