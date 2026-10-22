"""Local-only deterministic XPBD runner. No screenshots or remote transport."""
from __future__ import annotations
import argparse, json, os, re, subprocess, sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from moc3_local_replay import _private_manifest

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frames", type=int, default=125)
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    if not (110 <= args.frames <= 1000):
        parser.error("frames must be between 110 and 1000")
    if os.name != "nt":
        parser.error("Windows physical runner only")
    report = ROOT / "build/studio-user" / ("xpbd-auto-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
    report.mkdir(parents=True, exist_ok=False)
    summary = {"test": "native_studio_xpbd", "result": "FAIL", "frames_requested": args.frames}
    native_log = report / "native.log"
    try:
        model = _private_manifest()
        if not model or not model.is_file():
            raise RuntimeError("No local private fixture; no remote lookup")
        if not args.skip_build:
            build = subprocess.run(
                ["cmake", "--build", str(ROOT/"build/avatar-studio"), "--config", "Release",
                 "--target", "avatar_studio", "--parallel", "2"],
                cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=180)
            (report / "build.log").write_text(build.stdout, encoding="utf-8")
            if build.returncode:
                raise RuntimeError("Native build failed; inspect local build.log")
        binary = ROOT/"build/avatar-studio/Release/avatar_studio.exe"
        if not binary.is_file():
            raise RuntimeError("Native executable unavailable")
        with native_log.open("wb") as output:
            run = subprocess.run([str(binary), "--model", str(model), "--evidence", str(report),
                                  "--frames", str(args.frames), "--xpbd-smoke"],
                                 cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, timeout=120)
        contents = native_log.read_text(encoding="utf-8", errors="replace")
        match = re.search(r"XPBD_SMOKE_RESULT frames=(\d+) accepted=(\d+) checks=(\d+) failures=(\d+) active=(\d+)", contents)
        if not match:
            raise RuntimeError("Missing native XPBD receipt; inspect local native.log")
        frames, accepted, checks, failures, active = map(int, match.groups())
        state = json.loads((report/"state.json").read_text(encoding="utf-8"))
        summary.update(exit_code=run.returncode, frames=frames, accepted=accepted,
                       checks=checks, failures=failures, final_active=bool(active))
        assert run.returncode == 0, "Native exit non-zero"
        assert frames == args.frames == state["frame"], "Frame identity mismatch"
        assert accepted >= 20, "XPBD did not accept enough rendered frames"
        assert checks >= 3 and failures == 0, "Native XPBD oracle failed"
        assert active == 0, "XPBD failed to disable"
        summary["result"] = "PASS"
    except Exception as exc:
        summary["reason"] = str(exc)
    finally:
        (report/"summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"XPBD_AUTO_{summary['result']}: frames={summary.get('frames',0)} accepted={summary.get('accepted',0)} failures={summary.get('failures','?')}")
    print(f"LOCAL_REPORT={report.relative_to(ROOT)}")
    return 0 if summary["result"] == "PASS" else 1

if __name__ == "__main__":
    raise SystemExit(main())
