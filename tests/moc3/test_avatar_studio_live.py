"""Opt-in visible Windows controls test; interacts only with its own Studio HWND."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from avatar_studio import BINARY
from moc3_local_replay import _private_manifest


@unittest.skipUnless(os.name == "nt" and os.environ.get("ARS_STUDIO_LIVE") == "1",
                     "Explicit visible Studio test opt-in required")
class VisibleStudio(unittest.TestCase):
    def test_mouse_keyboard_presets_inspector_and_close(self):
        model = _private_manifest()
        self.assertIsNotNone(model, "Authorized local fixture required")
        self.assertTrue(BINARY.is_file())
        output = ROOT / "build/studio-user" / ("controls-" + uuid.uuid4().hex)
        output.mkdir(parents=True)
        root_screenshots = set(ROOT.glob("screenshot*.png"))
        user = ctypes.WinDLL("user32", use_last_error=True)
        # Match GLFW's per-monitor awareness: Win32 coordinates must stay physical.
        user.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
        callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
        user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
        user.GetForegroundWindow.restype = wintypes.HWND
        user.SetForegroundWindow.argtypes = [wintypes.HWND]
        user.BringWindowToTop.argtypes = [wintypes.HWND]
        user.IsWindowVisible.argtypes = [wintypes.HWND]
        user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
        user.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
        user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        log = (output / "native.log").open("wb")
        process = subprocess.Popen([str(BINARY), "--model", str(model), "--evidence",
                                    str(output), "--frames", "4000"], cwd=ROOT,
                                   stdout=log, stderr=subprocess.STDOUT)
        window = None
        cases = []

        def wait_for(predicate, timeout=5):
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                if process.poll() is not None:
                    self.fail("Studio exited before control verification")
                try:
                    state = json.loads((output / "state.json").read_text())
                    if predicate(state):
                        return state
                except (OSError, ValueError):
                    pass
                time.sleep(.04)
            self.fail("Timed out waiting for Studio state")

        def focus():
            self.assertIsNone(process.poll(), "Studio exited before focus/input")
            user.SetForegroundWindow(window)
            if user.GetForegroundWindow() != window:
                foreground = user.GetForegroundWindow()
                foreground_thread = user.GetWindowThreadProcessId(foreground, None)
                current_thread = ctypes.windll.kernel32.GetCurrentThreadId()
                attached = user.AttachThreadInput(current_thread, foreground_thread, True)
                try:
                    user.BringWindowToTop(window)
                    user.SetForegroundWindow(window)
                finally:
                    if attached:
                        user.AttachThreadInput(current_thread, foreground_thread, False)
            time.sleep(.1)
            self.assertEqual(user.GetForegroundWindow(), window,
                             "Refuse input when the owned Studio window is not foreground")

        def click(x, y, right=False):
            focus()
            p = wintypes.POINT(x, y)
            self.assertTrue(user.ClientToScreen(window, ctypes.byref(p)))
            user.SetCursorPos(p.x, p.y)
            time.sleep(.12)  # Let GLFW consume the new cursor location before the click.
            user.mouse_event(8 if right else 2, 0, 0, 0, 0)
            time.sleep(.06)
            user.mouse_event(16 if right else 4, 0, 0, 0, 0)
            time.sleep(.1)

        def key(vk):
            focus()
            user.keybd_event(vk, 0, 0, 0)
            time.sleep(.06)
            user.keybd_event(vk, 0, 2, 0)

        try:
            wait_for(lambda state: state["frame"] >= 30)
            found = []

            @callback_type
            def collect(hwnd, _):
                pid = wintypes.DWORD()
                user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
                if pid.value == process.pid and user.IsWindowVisible(hwnd):
                    found.append(hwnd)
                return True

            user.EnumWindows(collect, 0)
            self.assertEqual(len(found), 1)
            window = found[0]
            key(0x20)  # Space: pause.
            paused = wait_for(lambda state: state["playing"] == 0)
            later = wait_for(lambda state: state["frame"] > paused["frame"] + 15)
            self.assertEqual(paused["time"], later["time"])
            click(538, 108)  # Step.
            stepped = wait_for(lambda state: state["time"] > later["time"])
            self.assertAlmostEqual(stepped["time"] - later["time"], 1 / 60, places=5)
            cases.append("keyboard_pause_and_mouse_single_step")

            click(200, 227)  # Happy preset.
            wait_for(lambda state: state["expression"] == 1)
            click(238, 451)  # Linked physics toggle.
            wait_for(lambda state: state["physics_enabled"] == 0)
            key(ord("P"))
            wait_for(lambda state: state["physics_enabled"] == 1)
            cases.append("expression_and_physics_toggle")

            click(1210, 195)  # Search field, type "ParamAngleX".
            for char in "ParamAngleX":
                vk = user.VkKeyScanW(ord(char))
                if vk & 0x100:
                    user.keybd_event(0x10, 0, 0, 0)
                user.keybd_event(vk & 0xff, 0, 0, 0)
                time.sleep(.02)
                user.keybd_event(vk & 0xff, 0, 2, 0)
                if vk & 0x100:
                    user.keybd_event(0x10, 0, 2, 0)
                time.sleep(.02)
            click(1320, 260)
            wait_for(lambda state: state["pins"] == 1)
            click(1200, 247, right=True)
            wait_for(lambda state: state["pins"] == 0)
            click(1350, 150)  # Layers tab.
            wait_for(lambda state: state["tab"] == 1)
            cases.append("search_parameter_pin_clear_and_layers")

            click(550, 914)  # Ocean background.
            wait_for(lambda state: state["background"] == 2)
            click(80, 917)  # Save preset.
            self.assertTrue((output / "preset.txt").is_file())
            click(450, 914)  # Light background.
            wait_for(lambda state: state["background"] == 1)
            click(190, 917)  # Load preset.
            wait_for(lambda state: state["background"] == 2)
            before_bad = wait_for(lambda state: True)
            (output / "preset.txt").write_text("AVATAR_STUDIO_1 invalid\n")
            click(190, 917)
            state = wait_for(lambda state: state["frame"] > before_bad["frame"] + 15)
            self.assertEqual(state["background"], 2)
            cases.append("preset_roundtrip_and_corrupt_preset_rollback")

            click(390, 108)  # Face camera.
            wait_for(lambda state: state["zoom"] > 1.5)
            key(ord("R"))
            wait_for(lambda state: state["zoom"] == 1)
            before_capture = wait_for(lambda state: True)
            key(0x7B)  # F12.
            wait_for(lambda state: state["frame"] > before_capture["frame"] + 15)
            self.assertTrue((output / "viewport.png").is_file())
            cases.append("camera_fit_and_capture")
            user.PostMessageW(window, 0x10, 0, 0)  # WM_CLOSE.
            self.assertEqual(process.wait(timeout=5), 0)
            self.assertEqual(set(ROOT.glob("screenshot*.png")), root_screenshots,
                             "F12 must not create a second capture in repository root")
            cases.append("clean_window_close")
            (output / "controls.json").write_text(json.dumps({"result": "PASS", "cases": cases}, indent=2))
            print(f"Visible controls evidence: {output.relative_to(ROOT)}")
        finally:
            if process.poll() is None:
                if window:
                    from PIL import ImageGrab
                    rect = wintypes.RECT()
                    user.GetWindowRect(window, ctypes.byref(rect))
                    ImageGrab.grab(bbox=(rect.left, rect.top, rect.right, rect.bottom)).save(output / "controls-window.png")
                    user.PostMessageW(window, 0x10, 0, 0)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
            log.close()


if __name__ == "__main__":
    unittest.main()
