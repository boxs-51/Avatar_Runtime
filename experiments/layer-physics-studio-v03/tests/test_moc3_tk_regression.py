"""RED-first tests for MOC3 viewer Tk binary PPM and error survivability.

Only a generated two-by-two RGB image is used. No user model data are accessed.
"""
from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest


VIEWER = Path(__file__).resolve().parents[1] / "moc3_viewer.py"
spec = importlib.util.spec_from_file_location("lps_moc3_regression", VIEWER)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
PIXELS = b"P6\n2 2\n255\n" + bytes((
    255, 0, 0, 0, 255, 0,
    0, 0, 255, 255, 255, 255,
))


class FakeValue:
    def __init__(self, value):
        self.value = value

    def get(self):
        return self.value

    def set(self, value):
        self.value = value


class FakeRoot:
    def __init__(self):
        self.closed = False
        self.scheduled = []

    def after(self, delay, callback):
        self.scheduled.append((delay, callback))

    def destroy(self):
        self.closed = True

    def winfo_exists(self):
        return not self.closed


class FailingBridge:
    def frame(self, parameters):
        raise RuntimeError("synthetic failure in native MOC3 frame")


class TkViewerRegression(unittest.TestCase):
    def test_generated_binary_ppm_is_not_base64_encoded(self):
        class TkDouble:
            def PhotoImage(self, **kw):
                self.kw = kw
                return self

        t = TkDouble()
        self.assertIs(module.photoimage_from_ppm(t, PIXELS), t)
        self.assertEqual(t.kw["data"], PIXELS)
        self.assertEqual(t.kw["format"], "PPM")

    def test_render_error_does_not_destroy_window_and_is_logged_locally(self):
        with tempfile.TemporaryDirectory() as directory:
            editor = module.Editor.__new__(module.Editor)
            editor.output = Path(directory)
            editor.root = FakeRoot()
            editor.status = FakeValue("")
            editor.running = True
            editor.busy = False
            editor.paused = FakeValue(False)
            editor.physics = None
            editor.angle_id = None
            editor.bridge = FailingBridge()
            editor.tick()
            self.assertFalse(editor.running)
            self.assertFalse(editor.root.closed, "The user needs to read the fault")
            self.assertIn("HOLD", editor.status.value)
            self.assertTrue((editor.output / "viewer_error.log").is_file())
            self.assertIn("synthetic failure", (editor.output / "viewer_error.log").read_text())
            self.assertFalse(any(getattr(fn, "__name__", "") == "close"
                                 for _, fn in editor.root.scheduled))

    @unittest.skipUnless(sys.platform == "win32" or os.environ.get("DISPLAY"),
                         "No native Tk display; run this test under xvfb-run on Linux")
    def test_real_tk_ppm_bytes_render_a_2x2_rgb_image(self):
        import tkinter as tk
        root = tk.Tk()
        root.withdraw()
        try:
            image = module.photoimage_from_ppm(tk, PIXELS)
            self.assertEqual((image.width(), image.height()), (2, 2))
            self.assertEqual(tuple(image.get(0, 0)), (255, 0, 0))
            self.assertEqual(tuple(image.get(1, 0)), (0, 255, 0))
        finally:
            root.destroy()


if __name__ == "__main__":
    unittest.main()
