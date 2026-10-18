"""Local-only I0 probe checks; the private model path is an explicit argument."""

import argparse
import json
import shutil
import subprocess
import uuid
from pathlib import Path


def run_probe(executable: Path, candidate: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(executable), str(candidate)],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )


def rejected(executable: Path, candidate: Path, reason: str) -> None:
    result = run_probe(executable, candidate)
    assert result.returncode == 2, (reason, result.returncode, result.stderr)
    assert "I0_REJECT:" in result.stderr, reason
    assert not result.stdout.strip(), reason


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--private-moc", type=Path)
    parser.add_argument("--temp-root", type=Path, default=Path("build/moc3-i0"))
    args = parser.parse_args()
    if not args.executable.is_file():
        parser.error("probe executable does not exist")

    args.temp_root.mkdir(parents=True, exist_ok=True)
    root = args.temp_root / f"ars-moc3-i0-{uuid.uuid4().hex}"
    root.mkdir()
    try:
        wrong_magic = root / "wrong-magic.moc3"
        wrong_magic.write_bytes(b"NOPE" + bytes(64))
        rejected(args.executable, wrong_magic, "wrong magic")

        too_large = root / "too-large.moc3"
        with too_large.open("wb") as stream:
            stream.seek(16 * 1024 * 1024)
            stream.write(b"\0")
        rejected(args.executable, too_large, "oversized MOC")

        if args.private_moc:
            assert args.private_moc.stat().st_size <= 16 * 1024 * 1024
            source = args.private_moc.read_bytes()
            baseline = run_probe(args.executable, args.private_moc)
            assert baseline.returncode == 0, (baseline.returncode, baseline.stderr)
            again = run_probe(args.executable, args.private_moc)
            assert again.returncode == 0 and again.stdout == baseline.stdout
            inventory = json.loads(baseline.stdout)
            hashes = [inventory["baseline_geometry_hash"],
                      inventory["pose_1_geometry_hash"],
                      inventory["pose_2_geometry_hash"]]
            assert inventory["drawables"] > 0 and len(set(hashes)) == 3
            assert inventory["pose_1_index"] != inventory["pose_2_index"]

            truncated = root / "truncated.moc3"
            truncated.write_bytes(source[:128])
            rejected(args.executable, truncated, "truncated MOC")

            unsupported = root / "unsupported-version.moc3"
            altered = bytearray(source)
            altered[4] = 255
            unsupported.write_bytes(altered)
            rejected(args.executable, unsupported, "unsupported version")

            print("I0_PASS: private MOC parsed; 3 distinct geometry hashes; "
                  "repeat stable; 4 negative cases rejected")
        else:
            print("I0_PARTIAL: 2 synthetic negative cases rejected; private MOC not supplied")
    finally:
        if root.resolve().is_relative_to(args.temp_root.resolve()):
            shutil.rmtree(root)


if __name__ == "__main__":
    main()
