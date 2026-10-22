"""Local desktop model lab; private models and output stay out of Git."""
from __future__ import annotations

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/avatar-studio"
BINARY = BUILD / "Release/avatar_studio.exe"


def positive_frames(value: str) -> int:
    number = int(value)
    if not 1 <= number <= 1_000_000:
        raise argparse.ArgumentTypeError("frames must be between 1 and 1000000")
    return number


def select_model(explicit: str | None) -> Path | None:
    if explicit:
        model = Path(explicit).resolve(strict=True)
        if not model.is_file() or not model.name.lower().endswith(".model3.json"):
            raise ValueError("Choose a .model3.json manifest")
        return model
    if os.environ.get("MOC3_VIEWER_PROFILE"):
        from moc3_viewer_diagnostics import profile
        return Path(profile()["model"])
    from moc3_local_replay import _private_manifest
    model = _private_manifest()
    if model is not None:
        return model
    import tkinter as tk
    from tkinter import filedialog
    root = tk.Tk()
    root.withdraw()
    try:
        name = filedialog.askopenfilename(title="Choose a local model3 manifest",
                                          filetypes=[("Model3", "*.model3.json")])
    finally:
        root.destroy()
    return select_model(name) if name else None


def build_studio(force: bool = False) -> None:
    sources = [ROOT / "tools/avatar-studio", ROOT / "src/moc3", ROOT / "src/validation", ROOT / "include"]
    latest = max((p.stat().st_mtime for folder in sources for p in folder.rglob("*")
                  if p.is_file() and (p.suffix in {".c", ".cpp", ".h", ".hpp", ".cmake"}
                                      or p.name == "CMakeLists.txt")), default=0)
    if not force and BINARY.is_file() and BINARY.stat().st_mtime >= latest:
        return
    purism = ROOT / "build/moc3-i0/purismcore"
    raylib = ROOT / "build/moc3-render/viewer/_deps/raylib-src"
    utf8proc = ROOT / "build/c-deps/utf8proc"
    if not shutil.which("cmake") or not all((p / "CMakeLists.txt").is_file()
                                           for p in (purism, raylib, utf8proc)):
        raise RuntimeError("Build requires CMake, Visual Studio C++ and the existing "
                           "local Purism/raylib/utf8proc sources. No downloads are performed.")
    command = ["cmake", "-S", str(ROOT / "tools/avatar-studio"), "-B", str(BUILD),
               "-A", "x64", f"-DSTUDIO_PURISM_ROOT={purism.as_posix()}",
               f"-DSTUDIO_RAYLIB_ROOT={raylib.as_posix()}"]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run(["cmake", "--build", str(BUILD), "--config", "Release",
                    "--target", "avatar_studio", "--parallel", "4"], cwd=ROOT, check=True)
    if not BINARY.is_file():
        raise RuntimeError("Build finished without the Studio executable")


def output_directory(smoke: bool) -> Path:
    output = ROOT / "build/studio-user"
    if smoke:
        output /= "smoke-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    if not output.resolve().is_relative_to((ROOT / "build").resolve()):
        raise ValueError("Studio output must remain in ignored build storage")
    output.mkdir(parents=True, exist_ok=True)
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", help="local .model3.json manifest")
    parser.add_argument("--build", action="store_true", help="force incremental build")
    parser.add_argument("--legacy", action="store_true", help="open the original diagnostic viewer")
    parser.add_argument("--frames", type=positive_frames, help="close after this many visible frames")
    parser.add_argument("--smoke", action="store_true", help="visible scripted test; closes after 300 frames")
    args = parser.parse_args(argv)
    try:
        model = select_model(args.model)
        if model is None:
            return 0
        if args.legacy:
            from moc3_viewer_diagnostics import VIEWER
            if not VIEWER.is_file():
                raise RuntimeError("Original diagnostic viewer executable is unavailable")
            if args.smoke or args.frames:
                raise ValueError("--frames/--smoke apply to Studio")
            return subprocess.call([str(VIEWER), str(model)], cwd=ROOT)
        build_studio(args.build)
        output = output_directory(args.smoke)
        command = [str(BINARY), "--model", str(model), "--evidence", str(output)]
        frames = args.frames or (300 if args.smoke else None)
        if frames:
            command += ["--frames", str(frames)]
        if args.smoke:
            if frames < 270:
                raise ValueError("Smoke verification needs at least 270 visible frames")
            command.append("--smoke")
        print("Avatar Studio - local preview. Settings and PNGs: build/studio-user", flush=True)
        result = subprocess.run(command, cwd=ROOT)
        if args.smoke:
            state = json.loads((output / "state.json").read_text(encoding="utf-8"))
            checks = json.loads((output / "smoke.json").read_text(encoding="utf-8"))
            expected = ("01-idle", "02-happy", "03-dance", "04-paused", "05-zoom", "studio-ui")
            if result.returncode or checks["failures"] or checks["checks"] < 5 or state["frame"] != frames or not all(
                    (output / f"{name}.png").is_file() for name in expected):
                raise RuntimeError("Visible smoke run incomplete; inspect local output")
            print(f"Visible smoke complete: {output.relative_to(ROOT)}", flush=True)
        return result.returncode
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"Avatar Studio: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
