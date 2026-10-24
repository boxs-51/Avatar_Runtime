"""Check whether Git-staged or committed artifacts could disclose private models.

Usage: python scripts/check_model_publication.py --staged
       python scripts/check_model_publication.py --ref HEAD

Only scans Git blobs. Never prints file names, paths, content or hashes.
"""
from __future__ import annotations
import argparse
import collections
import json
import subprocess
import sys
from pathlib import Path, PurePosixPath

MAX_BLOB = 25_000_000
MAX_JSON = 2_000_000
EXTENSIONS = (
 ".moc3", ".model3.json", ".physics3.json", ".exp3.json",
 ".motion3.json", ".pose3.json", ".userdata3.json",
 ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif",
 ".dds", ".ktx", ".ktx2", ".tga", ".glb", ".gltf", ".fbx",
 ".zip", ".7z", ".rar", ".exe", ".dll", ".pdb",
)
PRIVATE_DIRS = frozenset({
 "private-models", "private-assets", "captures-private",
 "local-assets", "model-private", "build", ".venv",
})
SIGNATURES = (b"MOC3", b"glTF", b"\x89PNG\r\n\x1a\n",
              b"\xff\xd8\xff", b"GIF87a", b"GIF89a", b"PK\x03\x04")


def classify(path: str, data: bytes) -> set[str]:
    """Use only non-identifying category names in results."""
    normalized = path.replace("\\", "/").casefold()
    categories = set()
    if any(part in PRIVATE_DIRS for part in PurePosixPath(normalized).parts):
        categories.add("restricted-directory")
    if normalized.endswith(EXTENSIONS):
        categories.add("model-or-media-extension")
    if len(data) > MAX_BLOB:
        categories.add("oversized-unknown-blob")
    if data.startswith(SIGNATURES):
        categories.add("renamed-model-or-image")
    text_is_utf16 = False
    if (normalized.endswith((".txt", ".md", ".json", ".jsonl"))
            and data.startswith((b"\xff\xfe", b"\xfe\xff"))
            and len(data) % 2 == 0):
        try:
            data.decode("utf-16")
            text_is_utf16 = True
        except UnicodeError:
            pass
    if b"\0" in data[:4096] and not text_is_utf16:
        categories.add("binary-content")
    if normalized.endswith((".json", ".jsonl", ".txt")) and len(data) <= MAX_JSON:
        try:
            doc = json.loads(data.decode("utf-16" if text_is_utf16 else "utf-8-sig"))
        except (UnicodeError, ValueError):
            doc = None
        if isinstance(doc, dict):
            keys = {str(k).casefold() for k in doc}
            refs = next((v for k, v in doc.items()
                         if str(k).casefold() == "filereferences"), None)
            if isinstance(refs, dict) and (
                {"moc", "textures", "motions", "physics", "expressions"} &
                {str(k).casefold() for k in refs}
            ):
                categories.add("model-manifest-json")
            if any(str(key).casefold() in {"vertices", "indices", "textures"}
                   and isinstance(value, list) and len(value) >= 3
                   for key, value in doc.items()):
                categories.add("model-geometry-json")
    return categories


def git(*args: str) -> bytes:
    result = subprocess.run(("git", *args), capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError("Git inspection unsuccessful")
    return result.stdout


def entries(staged: bool, ref: str | None) -> list[tuple[bytes, str]]:
    if staged:
        raw = git("ls-files", "-s", "-z")
    else:
        raw = git("ls-tree", "-r", "-z", ref or "HEAD")
    records = []
    for item in raw.split(b"\0"):
        if not item:
            continue
        info, path = item.split(b"\t", 1)
        fields = info.split()
        if staged:
            _, oid, stage = fields
            if stage != b"0":
                raise RuntimeError("Unmerged index")
        else:
            _, kind, oid = fields
            if kind != b"blob":
                continue
        records.append((oid, path.decode("utf-8", "surrogateescape")))
    return records


def scan(records: list[tuple[bytes, str]]) -> tuple[int, collections.Counter]:
    counts = collections.Counter()
    if not records:
        return 0, counts
    proc = subprocess.Popen(("git", "cat-file", "--batch"),
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL)
    assert proc.stdin is not None and proc.stdout is not None
    try:
        for oid, path in records:
            # Windows pipe buffers can be small: request one blob at a time
            # so the producer never deadlocks while its output pipe is full.
            proc.stdin.write(oid + b"\n")
            proc.stdin.flush()
            head = proc.stdout.readline().split()
            if len(head) != 3 or head[0] != oid or head[1] != b"blob":
                raise RuntimeError("Unexpected Git blob record")
            size = int(head[2])
            if size > MAX_BLOB:
                counts["oversized-unknown-blob"] += 1
                remaining = size
                while remaining:
                    part = proc.stdout.read(min(65536, remaining))
                    if not part:
                        raise RuntimeError("Incomplete Git blob")
                    remaining -= len(part)
            else:
                data = proc.stdout.read(size)
                if len(data) != size:
                    raise RuntimeError("Incomplete Git blob")
                counts.update(classify(path, data))
            if proc.stdout.read(1) != b"\n":
                raise RuntimeError("Malformed Git blob separator")
    finally:
        proc.stdin.close()
        proc.stdout.close()
        proc.wait(timeout=10)
    if proc.returncode:
        raise RuntimeError("Git blob scan failed")
    return len(records), counts


def scan_package(root: Path) -> tuple[int, collections.Counter]:
    """Inspect an installed SDK/archive extraction without printing paths."""
    if not root.is_dir():
        raise RuntimeError("SDK package root unavailable")
    counts = collections.Counter()
    checked = 0
    for path in root.rglob("*"):
        if path.is_symlink():
            counts["symbolic-link-in-package"] += 1
            continue
        if not path.is_file():
            continue
        checked += 1
        rel = path.relative_to(root).as_posix()
        size = path.stat().st_size
        if size > MAX_BLOB:
            counts["oversized-unknown-blob"] += 1
            continue
        counts.update(classify(rel, path.read_bytes()))
    return checked, counts


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--staged", action="store_true")
    source.add_argument("--ref", metavar="REV")
    source.add_argument("--package", metavar="SDK_DIR",
                        help="scan all files in an installed SDK directory")
    args = parser.parse_args(argv)
    try:
        if args.package is not None:
            count, categories = scan_package(Path(args.package))
        else:
            count, categories = scan(entries(args.staged, args.ref))
    except (OSError, RuntimeError, ValueError):
        print("MODEL_PUBLICATION_SCAN BLOCKED: cannot validate Git input",
              file=sys.stderr)
        return 2
    if categories:
        # Do not reveal the number of private assets or identifying filenames.
        print("MODEL_PUBLICATION_SCAN BLOCKED private-artifact-categories:")
        for category in sorted(categories):
            print(f"  {category}")
        return 1
    scope = "sdk-directory" if args.package is not None else "git-blobs"
    print(f"MODEL_PUBLICATION_SCAN PASS inspected={count} scope={scope}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
