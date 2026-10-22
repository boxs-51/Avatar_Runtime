"""RED-first: Layer Physics Studio launches the native raylib Avatar Studio UI."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
LAUNCHER = ROOT / "launch_layer_physics_studio.bat"
ENTRY = ROOT / "scripts/layer_physics_studio.py"
LEGACY_LAUNCHER = ROOT / "launch_interactive_viewer.bat"


class NativeStudioEntrypoint(unittest.TestCase):
    def test_python_entrypoint_delegates_to_native_studio_without_tk(self):
        self.assertTrue(ENTRY.is_file())
        spec = importlib.util.spec_from_file_location("layer_physics_studio", ENTRY)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with patch.object(module, "run_native_studio", return_value=29) as runner:
            self.assertEqual(module.main(["--model", "local.model3.json", "--frames", "90"]), 29)
            runner.assert_called_once_with(["--model", "local.model3.json", "--frames", "90"])
        source = ENTRY.read_text(encoding="utf-8")
        self.assertNotIn("moc3_viewer.py", source)
        self.assertNotIn("tkinter", source)

    def test_both_launchers_use_the_same_native_entrypoint(self):
        self.assertTrue(LAUNCHER.is_file())
        for path in (LAUNCHER, LEGACY_LAUNCHER):
            content = path.read_text(encoding="utf-8").lower().replace("\\", "/")
            self.assertIn("scripts/layer_physics_studio.py", content)
            self.assertIn("%*", content)
            self.assertNotIn("moc3_viewer.py", content)

    def test_native_build_is_owned_by_avatar_studio(self):
        cmake = (ROOT / "tools/avatar-studio/CMakeLists.txt").read_text(encoding="utf-8")
        source = (ROOT / "tools/avatar-studio/studio.c").read_text(encoding="utf-8")
        self.assertIn("add_executable(avatar_studio", cmake)
        self.assertIn("InitWindow(", source)
        self.assertIn("RenderModel(", source)

    def test_help_runs_without_opening_or_loading_model(self):
        self.assertTrue(ENTRY.is_file())
        result = subprocess.run([sys.executable, str(ENTRY), "--help"],
                                cwd=ROOT, capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--model", result.stdout)
        self.assertIn("--legacy", result.stdout)


if __name__ == "__main__":
    unittest.main()
