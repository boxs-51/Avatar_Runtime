"""Check whether Git-staged or committed artifacts could disclose private models.

Usage: python scripts/check_model_publication.py --staged
       python scripts/check_model_publication.py --ref HEAD

Only scans Git blobs. Never prints file names, paths, content or hashes.
"""
from __future__ import annotations
import argparse
import collections
import json
import os
import hashlib
import re
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
 ".armesh", ".armesh.bak", ".armesh.tmp",
)
PRIVATE_DIRS = frozenset({
 "private-models", "private-assets", "captures-private",
 "local-assets", "model-private", "build", ".venv",
})
SIGNATURES = (
 b"MOC3", b"glTF", b"STUDIO_GEOMETRY_", b"ARSMESH1", b"ARSMESH2",
 b"ARSMESH3", b"\x89PNG\r\n\x1a\n",
              b"\xff\xd8\xff", b"GIF87a", b"GIF89a", b"PK\x03\x04")


def classify(path: str, data: bytes) -> set[str]:
    """Use only non-identifying category names in results."""
    normalized = path.replace("\\", "/").casefold()
    categories = set()
    if any(part in PRIVATE_DIRS for part in PurePosixPath(normalized).parts):
        categories.add("restricted-directory")
    if normalized.endswith(EXTENSIONS):
        categories.add("model-or-media-extension")
    if PurePosixPath(normalized).name in {
        "layer_geometry.txt", "layer_geometry.txt.bak",
        "layer_geometry.tmp", "layer_geometry.bak", "model_runtime.tmp",
        "model_runtime.bak",
    }:
        categories.add("authored-model-output-name")
    if len(data) > MAX_BLOB:
        categories.add("oversized-unknown-blob")
    if data.startswith(SIGNATURES):
        categories.add("renamed-model-or-image")
    # Text sidecars may contain UTF-8 BOM/leading whitespace before
    # a valid STUDIO_GEOMETRY header. Refuse disguised copies too.
    stripped = data[:128].removeprefix(b"\xef\xbb\xbf").lstrip()
    if stripped.startswith(b"STUDIO_GEOMETRY_"):
        categories.add("renamed-authored-source")

    if data.lstrip().startswith(b"version https://git-lfs.github.com/spec/v1"):
        categories.add("git-lfs-pointer")
    if normalized.endswith(".gitattributes") and b"filter=lfs" in data:
        categories.add("git-lfs-attributes")
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        try:
            decoded=data.decode("utf-16")
        except UnicodeError:
            categories.add("invalid-utf16")
        else:
            if decoded.lstrip("\ufeff \t\r\n").startswith("STUDIO_GEOMETRY_"):
                categories.add("renamed-authored-source")
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
    # Content-based manifest detection must also work for renamed .dat
    # files; extension filtering alone can be bypassed accidentally.
    looks_json = data[:128].lstrip(b" \t\r\n\xef\xbb\xbf").startswith(b"{")
    if looks_json and len(data)>MAX_JSON:
        categories.add("uninspectable-structured-data")
    if (normalized.endswith((".json", ".jsonl", ".txt")) or looks_json) and len(data) <= MAX_JSON:
        try:
            doc = json.loads(data.decode("utf-16" if text_is_utf16 else "utf-8-sig"))
        except (UnicodeError, ValueError):
            doc = None
        if isinstance(doc,(dict,list)):
            todo=[doc]
            visited=0
            while todo:
                obj=todo.pop()
                visited+=1
                if visited>100000:
                    categories.add("structured-complexity-limit")
                    break
                if isinstance(obj,list):
                    todo.extend(v for v in obj if isinstance(v,(list,dict)))
                elif isinstance(obj,dict):
                    for key,val in obj.items():
                        k=str(key).casefold()
                        if k in {"vertices","indices","textures"} and (
                            isinstance(val,list) and len(val)>=3):
                            categories.add("model-geometry-json")
                        if k=="filereferences" and isinstance(val,dict):
                            names={str(x).casefold() for x in val}
                            if {"moc","textures","motions","physics","expressions"} & names:
                                categories.add("model-manifest-json")
                        if isinstance(val,(dict,list)):
                            todo.append(val)
    # Text receipts may expose reconstructed coordinates with no file signature.
    snippet=data
    if data.startswith((b"\xff\xfe",b"\xfe\xff")) and len(data)<=MAX_BLOB:
        try:
            snippet=data.decode("utf-16").encode("utf8")
        except UnicodeError:
            categories.add("invalid-utf16")
    # A receipt is a data line, not a C/Python string literal in a
    # synthetic test. Require start-of-line, but examine the WHOLE blob.
    if re.search(
        rb"(?m)^[ \t]*(?:layer|id)=\d+\b[^\r\n]{0,180}"
        rb"\bx=[+-]?(?:\d|\.\d)[^\r\n]{0,100}\by=[+-]?(?:\d|\.\d)",
        snippet,
    ):
        categories.add("model-ipc-receipt-data")
    if re.search(
        rb"(?m)^[ \t]*layer=\d+\s+index=\d+\s+a=\d+\s+b=\d+\s+c=\d+",
        snippet,
    ):
        categories.add("model-ipc-receipt-data")
    return categories


def protected_git_env() -> dict[str, str]:
    # Prevent local replace refs from making inspection see objects that
    # differ from the original objects sent to a public Git remote.
    return {**os.environ, "GIT_NO_REPLACE_OBJECTS": "1"}


def git(*args: str) -> bytes:
    result = subprocess.run(("git", *args), capture_output=True, check=False,
                            env=protected_git_env())
    if result.returncode:
        raise RuntimeError("Git inspection unsuccessful")
    return result.stdout


def validate_git_mode(mode: str) -> None:
    """Reject Git symlinks/submodules and unknown object modes by default."""
    if mode not in {"100644","100755"}:
        raise RuntimeError("Unapproved Git mode")


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
            mode, oid, stage=fields
            if stage!=b"0":
                raise RuntimeError("Unmerged Git index")
        else:
            mode, kind, oid=fields
            if kind!=b"blob":
                raise RuntimeError("Unapproved Git kind")
        validate_git_mode(mode.decode("ascii"))
        records.append((oid, path.decode("utf-8", "surrogateescape")))
    return records


def push_range_records(spec: str, manifest: str):
    """One reviewed commit on top of an immutable already-public base."""
    parts=spec.split("..")
    if len(parts)!=2 or not all(re.fullmatch(r"[0-9a-fA-F]{40,64}",x) for x in parts):
        raise RuntimeError("Expected immutable SHA..SHA range")
    base,head=parts
    if not manifest:
        raise RuntimeError("External approval manifest is mandatory")
    commits=git("rev-list",head,"^"+base).splitlines()
    if commits!=[head.encode("ascii")]:
        raise RuntimeError("Only a direct one-commit publication is allowed")
    # Do not permit merge commits, rebasing to another base or history gaps.
    parents=git("rev-list","--parents","-n","1",head).split()
    if len(parents)!=2 or parents[1].lower()!=base.encode("ascii").lower():
        raise RuntimeError("Reviewed publication parent changed")
    final=entries(False,head)
    payload=compare_manifest(final,manifest)
    if payload.get("base_oid","").lower()!=base.lower():
        raise RuntimeError("Approved base differs")
    if payload.get("commit_oid","").lower()!=head.lower():
        raise RuntimeError("Approved commit differs")
    raw_commit=git("cat-file","commit",head)
    if b"\n\n" not in raw_commit:
        raise RuntimeError("Malformed original Git commit")
    header,message=raw_commit.split(b"\n\n",1)
    lines=header.split(b"\n")
    # Git history traversal can use local grafts; inspect original bytes.
    if len(lines)!=4 or [line.split(b" ",1)[0] for line in lines]!=[
        b"tree",b"parent",b"author",b"committer"
    ]:
        raise RuntimeError("Unexpected original Git commit headers")
    raw_parent=lines[1].split(b" ",1)[1]
    if raw_parent.lower()!=base.encode("ascii").lower():
        raise RuntimeError("Raw commit parent differs from reviewed public base")
    for field,line in zip((b"author",b"committer"),lines[2:]):
        if not re.fullmatch(
          field+rb" Studio Review <review@users.noreply.github.com> [0-9]{9,12} [+-][0-9]{4}",
          line
        ):
            raise RuntimeError("Unexpected commit identity metadata")

    if len(message)>180 or not re.fullmatch(
         rb"review\(studio\): [A-Za-z0-9 ._/-]{12,130}\n",message):
        raise RuntimeError("Public raw commit message not approved")
    expected=payload.get("commit_message_sha256")
    if expected!=hashlib.sha256(message).hexdigest():
        raise RuntimeError("Raw commit message differs from external approval")
    # Scan the only introduced tree; no historical blob can be introduced
    # then deleted in a later commit because multi-commit pushes are blocked.
    return final,final


def compare_manifest(records: list[tuple[bytes, str]],manifest: str) -> dict:
    """Require an externally approved exact immutable tree, no auto-approval."""
    payload=json.loads(Path(manifest).read_text(encoding="utf-8"))
    if not isinstance(payload,dict) or payload.get("format")!="ARS_PUBLIC_TREE_REVIEW_V1":
        raise RuntimeError("Invalid approval manifest")
    approved=payload.get("entries")
    if not isinstance(approved,list):
        raise RuntimeError("Missing immutable Git tree allowlist")
    expected=[]
    for item in approved:
        if not isinstance(item,dict) or set(item)!={"oid","path"}:
            raise RuntimeError("Malformed approved entry")
        oid=item["oid"]
        if not isinstance(oid,str) or not re.fullmatch(r"[0-9a-fA-F]{40,64}",oid):
            raise RuntimeError("Unapproved object identifier")
        if not isinstance(item["path"],str):
            raise RuntimeError("Unapproved object path")
        expected.append((oid.lower().encode("ascii"),item["path"]))
    actual=[(oid.lower(),path) for oid,path in records]
    if len(actual)!=len(expected) or sorted(actual,key=lambda x:x[1])!=sorted(
          expected,key=lambda x:x[1]):
        raise RuntimeError("Proposed tree differs from reviewed blob allowlist")
    return payload


def scan(records: list[tuple[bytes, str]]) -> tuple[int, collections.Counter]:
    counts = collections.Counter()
    if not records:
        return 0, counts
    proc = subprocess.Popen(("git", "cat-file", "--batch"),
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.DEVNULL,env=protected_git_env())
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
    source.add_argument("--push-range", metavar="BASE_SHA..HEAD_SHA",
                        help="scan every newly reachable commit tree")
    source.add_argument("--ref", metavar="REV")
    source.add_argument("--package", metavar="SDK_DIR",
                        help="scan all files in an installed SDK directory")
    parser.add_argument("--manifest",metavar="APPROVED_JSON",
                        help="require exact paths and blob OIDs in reviewed tree")
    args = parser.parse_args(argv)
    try:
        final_records=None
        if args.package is not None:
            if args.manifest:
                raise RuntimeError("Manifest not applicable to packages")
            count,categories=scan_package(Path(args.package))
        elif args.push_range:
            historical,final_records=push_range_records(args.push_range,args.manifest)
            count,categories=scan(historical)
        else:
            final_records=entries(args.staged,args.ref)
            count,categories=scan(final_records)
        if args.manifest:
            if final_records is None:
                raise RuntimeError("No Git tree for manifest")
            compare_manifest(final_records,args.manifest)
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
    if not args.push_range:
        print(f"MODEL_PUBLICATION_SCAN CLEAN_NOT_APPROVED inspected={count} scope={scope}")
    else:
        print(f"MODEL_PUBLICATION_SCAN PASS inspected={count} scope={scope}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
