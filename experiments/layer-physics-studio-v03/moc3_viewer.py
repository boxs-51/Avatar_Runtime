"""Experimental MOC3 XPBD desktop editor.

Uses Avatar_Runtime's private isolated/preflight MOC3 package admission and
D3D11 WARP bridge. Only the optional LPS bridge applies XPBD deformation.
Never loads .moc3 bytes as an ARSCHAR or substitutes flattened sprites.
"""
from __future__ import annotations

import argparse
import base64
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from moc3_package_admission import admit_package, AdmissionError  # noqa: E402
from moc3_local_physics import Bridge  # noqa: E402
from moc3_physics import Physics, PhysicsError, STEP_NS  # noqa: E402


def validate_model3(path: Path) -> Path:
    path = path.expanduser().resolve(strict=True)
    if not path.is_file() or not path.name.lower().endswith(".model3.json"):
        raise ValueError("Provide the Live2D *.model3.json manifest, not a loose .moc3 file")
    return path


def decode_drawables(bridge: Bridge) -> list[tuple[int, int, str]]:
    bridge.send("LPS_LIST")
    header = bridge.response()
    if len(header) != 2 or header[0] != "LPS_LIST":
        raise RuntimeError("LPS bridge missing DRAWABLE listing: " + repr(header))
    count = int(header[1])
    if not 0 <= count <= 4096:
        raise RuntimeError("Invalid drawable count")
    result: list[tuple[int, int, str]] = []
    for expected in range(count):
        row = bridge.response()
        if len(row) < 3 or row[0] != "LPS_DRAWABLE" or int(row[1]) != expected:
            raise RuntimeError("Malformed drawable response")
        num = int(row[2])
        name = bytes.fromhex(row[3]).decode("utf-8", errors="replace") if len(row) > 3 else ""
        if num < 0 or num > 2000000:
            raise RuntimeError("Invalid drawable vertex count")
        result.append((expected, num, name or ("drawable-" + str(expected))))
    return result


def native_bridge_path() -> Path:
    return ROOT / "build" / "m2-d-private" / "bin" / "Release" / "ars_lps_moc3_bridge.exe"


class Editor:
    def __init__(self, model3: Path, output: Path, hardware: bool = False):
        import tkinter as tk
        from tkinter import ttk
        self.tk = tk
        self.output = output
        output.mkdir(parents=True, exist_ok=True)
        binary = native_bridge_path()
        if not binary.is_file():
            raise FileNotFoundError("Missing MOC3 XPBD bridge: " + str(binary) +
                                   ". Build ars_lps_moc3_bridge with pinned PurismCore.")
        package = admit_package(model3.parent, model3.name)
        self.bridge = Bridge(package, output, hardware, binary=binary.name)
        self.physics = None
        self.physics_hold = None
        if package.linked_physics_bytes:
            try:
                self.physics = Physics(package.linked_physics_bytes, self.bridge.catalog)
            except PhysicsError as exc:
                # Unsupported optional Physics3 must not block the original MOC3 or XPBD.
                self.physics_hold = str(exc)
        self.clock = 0
        self.index = decode_drawables(self.bridge)
        self.running = True
        self.busy = False
        self.previous_photo = None
        self.root = tk.Tk()
        self.root.title("Avatar Runtime - MOC3 XPBD native D3D11 viewer (experimental)")
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        note = "HOLD Physics3: " + self.physics_hold if self.physics_hold else (
            "MOC3 D3D11 WARP; select up to 2 drawable meshes for XPBD")
        self.status = tk.StringVar(value=note)
        self.gravity = tk.DoubleVar(value=-12.0)
        self.wind = tk.DoubleVar(value=3.0)
        self.compliance = tk.DoubleVar(value=0.0000001)
        self.radius = tk.DoubleVar(value=0.0)
        self.joint = tk.BooleanVar(value=False)
        self.paused = tk.BooleanVar(value=False)
        self.physics_on = tk.BooleanVar(value=self.physics is not None)
        self.angle = tk.DoubleVar(value=0.0)
        root_frame = ttk.Frame(self.root, padding=10)
        root_frame.pack(fill="both", expand=True)
        self.picture = tk.Label(root_frame)
        self.picture.grid(row=0, column=0, rowspan=18, sticky="n")
        right = ttk.Frame(root_frame, padding=(10, 0))
        right.grid(row=0, column=1, sticky="n")
        ttk.Label(right, text="MOC3 drawables (Ctrl+click: 2)").pack(anchor="w")
        self.listbox = tk.Listbox(right, height=15, width=44, selectmode="extended", exportselection=False)
        self.listbox.pack()
        for idx, count, name in self.index:
            suffix = " [>2048: HOLD]" if count > 2048 else ""
            self.listbox.insert("end", f"{idx}: {name[:30]} ({count}){suffix}")
        ttk.Button(right, text="Apply selected XPBD meshes", command=self.select).pack(fill="x")
        ttk.Button(right, text="Disable XPBD (original MOC3)", command=self.clear).pack(fill="x")
        for label, value, lower, upper, steps in (
            ("Gravity (MOC Y up)", self.gravity, -80, 80, 5),
            ("Wind", self.wind, -60, 60, 3),
            ("Compliance", self.compliance, 0, 0.0002, 0.00001),
            ("Collision radius", self.radius, 0, 0.15, 0.005)):
            ttk.Label(right, text=label).pack(anchor="w")
            ttk.Scale(right, from_=lower, to=upper, variable=value).pack(fill="x")
        ttk.Checkbutton(right, text="Two-way joint between selected meshes", variable=self.joint).pack(anchor="w")
        ttk.Button(right, text="Apply physics parameters / reset XPBD", command=self.apply).pack(fill="x")
        ttk.Checkbutton(right, text="Pause", variable=self.paused).pack(anchor="w")
        ttk.Checkbutton(right, text="Linked Physics3 (if provided)", variable=self.physics_on,
                        state="normal" if self.physics else "disabled").pack(anchor="w")
        control = next((item for item in ("ParamAngleX", "ParamBodyAngleX", "ParamAngleY")
                        if item in self.bridge.catalog), None)
        self.angle_id = control
        if control:
            low, default, high = self.bridge.catalog[control]
            self.angle.set(default)
            ttk.Label(right, text="Native MOC3 parameter: " + control).pack(anchor="w")
            ttk.Scale(right, from_=low, to=high, variable=self.angle).pack(fill="x")
        ttk.Label(self.root, textvariable=self.status).pack(fill="x")
        self.root.after(80, self.tick)

    def command(self, line: str, expected: str):
        self.bridge.send(line)
        result = self.bridge.response()
        if not result or result[0] != expected:
            raise RuntimeError("MOC3 XPBD control failed: " + repr(result))

    def apply(self):
        self.command(f"LPS_PARAMS {self.gravity.get():.9g} {self.wind.get():.9g} "
                     f"{self.compliance.get():.9g} {self.radius.get():.9g} "
                     f"{int(self.joint.get())}", "LPS_PARAMS_OK")
        self.status.set("XPBD parameters committed; dynamic state reset")

    def select(self):
        chosen = tuple(self.listbox.curselection())
        if not 1 <= len(chosen) <= 2:
            raise ValueError("Select one or two drawable meshes only")
        if any(self.index[i][1] > 2048 for i in chosen):
            raise ValueError("Selected mesh exceeds the experimental 2048 particle budget")
        self.command("LPS_SELECT " + " ".join(str(self.index[i][0]) for i in chosen), "LPS_SELECTED")
        self.status.set("XPBD selected: " + ", ".join(self.index[i][2] for i in chosen))

    def clear(self):
        self.command("LPS_CLEAR", "LPS_CLEARED")
        self.status.set("Original MOC3 drawable output restored (XPBD disabled)")

    def tick(self):
        if not self.running or self.busy:
            return
        if not self.paused.get():
            self.busy = True
            try:
                manual = {self.angle_id: float(self.angle.get())} if self.angle_id else {}
                candidate = None
                if self.physics and self.physics_on.get():
                    self.clock += 6 * STEP_NS
                    candidate = self.physics.prepare(self.clock, manual)
                    manual = dict(candidate.parameters)
                self.bridge.frame(manual)
                if candidate is not None:
                    self.physics.commit(candidate)
                meta = json.loads((self.output / "latest.json").read_text(encoding="utf-8"))
                if meta["width"] != 512 or meta["height"] != 512:
                    raise RuntimeError("Unexpected rendered frame size")
                frame = (self.output / meta["image"]).read_bytes()
                photo = self.tk.PhotoImage(data=base64.b64encode(frame), format="PPM")
                self.picture.configure(image=photo)
                self.previous_photo = photo
                self.status.set(f"Native D3D11 {meta['backend']} | frame {meta['frame_generation']} "
                                f"| target epoch {meta['target_epoch']}")
            except Exception as exc:
                self.running = False
                self.status.set("HOLD: " + str(exc))
                self.root.after(50, self.close)
            finally:
                self.busy = False
        if self.running:
            self.root.after(100, self.tick)

    def run(self):
        try:
            self.root.mainloop()
        finally:
            self.close()

    def close(self):
        if not self.running and self.bridge is None:
            return
        self.running = False
        try:
            self.bridge.close()
        finally:
            self.bridge = None
            if self.root.winfo_exists():
                self.root.destroy()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model3", type=Path, help="Path to your *.model3.json manifest")
    parser.add_argument("--out", type=Path, default=ROOT / "build/lps-moc3-preview")
    parser.add_argument("--hardware", action="store_true", help="Opt-in hardware D3D11; WARP default")
    args = parser.parse_args(argv)
    model = validate_model3(args.model3)
    Editor(model, args.out.resolve(), args.hardware).run()


if __name__ == "__main__":
    main()
