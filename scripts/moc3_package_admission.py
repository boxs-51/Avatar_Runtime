"""Bounded, trusted-local MOC3 manifest/atlas preflight for the private F1 track.

This module does not evaluate MOC geometry or admit arbitrary public inputs.
Only 8-bit non-interlaced RGB/RGBA PNG is supported; other required features
fail closed until the native asset path is connected.
"""

from __future__ import annotations

from dataclasses import dataclass, replace
from collections.abc import Callable
import json
import os
from pathlib import Path, PurePosixPath
import stat as stat_module
import struct
import zlib


MIB = 1024 * 1024
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class AdmissionError(Exception):
    def __init__(self, code: str):
        super().__init__(code)
        self.code = code


@dataclass(frozen=True)
class BudgetPolicy:
    max_manifest_bytes: int = 1 * MIB
    max_moc_bytes: int = 16 * MIB
    max_texture_count: int = 16
    max_encoded_texture_bytes: int = 128 * MIB
    max_decoded_texture_bytes: int = 512 * MIB
    max_png_side: int = 8192
    max_working_bytes: int = 768 * MIB


@dataclass(frozen=True)
class Texture:
    width: int
    height: int
    rgba: bytes
    working_bound: int = 0


@dataclass(frozen=True)
class ModelPackage:
    moc_bytes: bytes
    textures: tuple[Texture, ...]
    linked_physics: str | None
    encoded_texture_bytes: int
    decoded_texture_bytes: int
    # A MOC magic/header is not independent evaluator validation.
    moc_evaluated: bool = False
    linked_physics_bytes: bytes | None = None
    disabled_linked_features: tuple[str, ...] = ()
    color_policy: str = "RAW_ENCODED_RGBA8_NO_COLOR_TRANSFORM"
    working_bound: int = 0
    moc_reference: str = ""


def _fail(code: str) -> None:
    raise AdmissionError(code)


def _json_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            _fail("DUPLICATE_JSON_KEY")
        result[key] = value
    return result


def _reject_constant(_value: str) -> None:
    _fail("NONFINITE_JSON")


def _relative_reference(value: object, suffix: str) -> PurePosixPath:
    if not isinstance(value, str) or not value or len(value) > 512:
        _fail("INVALID_REFERENCE")
    if "\\" in value or ":" in value or "\x00" in value or value.startswith("/"):
        _fail("INVALID_REFERENCE")
    parts = value.split("/")
    if any(part in ("", ".", "..") or part.endswith((".", " ")) or
           any(ord(c) < 32 or c in '<>"|?*' for c in part) or
           part.split(".", 1)[0].upper() in
           ("CON", "PRN", "AUX", "NUL", *(f"COM{i}" for i in range(1, 10)),
            *(f"LPT{i}" for i in range(1, 10))) for part in parts):
        _fail("INVALID_REFERENCE")
    path = PurePosixPath(value)
    if not value.lower().endswith(suffix):
        _fail("INVALID_REFERENCE")
    return path


def _inside(root: Path, child: Path) -> bool:
    try:
        child.relative_to(root)
        return child != root
    except ValueError:
        return False


def _opened_path(stream, fallback: Path) -> Path:
    if os.name != "nt":
        return fallback.resolve(strict=True)
    import ctypes
    from ctypes import wintypes
    import msvcrt
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    function = kernel.GetFinalPathNameByHandleW
    function.argtypes = [wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD]
    function.restype = wintypes.DWORD
    buffer = ctypes.create_unicode_buffer(32768)
    length = function(msvcrt.get_osfhandle(stream.fileno()), buffer, len(buffer), 0)
    if not 0 < length < len(buffer):
        raise ctypes.WinError(ctypes.get_last_error())
    name = buffer.value
    if name.startswith("\\\\?\\UNC\\"):
        name = "\\\\" + name[8:]
    elif name.startswith("\\\\?\\"):
        name = name[4:]
    return Path(name)


def _read_contained(root: Path, relative: PurePosixPath, limit: int,
                    targets: set[tuple[int, int]] | None = None) -> bytes:
    candidate = root.joinpath(*relative.parts)
    # Reject reparse points, including Windows junctions, before resolving.
    current = root
    for part in relative.parts:
        current = current / part
        try:
            stat = current.lstat()
        except OSError:
            _fail("MISSING_REFERENCE")
        if current.is_symlink() or (getattr(stat, "st_file_attributes", 0)
                                    & getattr(stat_module, "FILE_ATTRIBUTE_REPARSE_POINT", 0)):
            _fail("REPARSE_REFERENCE")
    try:
        resolved = candidate.resolve(strict=True)
        if not _inside(root, resolved) or not resolved.is_file():
            _fail("INVALID_REFERENCE")
        if resolved.stat().st_size > limit:
            _fail("OUT_OF_BUDGET")
        with resolved.open("rb") as stream:
            if not _inside(root, _opened_path(stream, resolved)):
                _fail("INVALID_REFERENCE")
            opened = os.fstat(stream.fileno())
            if opened.st_size > limit:
                _fail("OUT_OF_BUDGET")
            identity = (opened.st_dev, opened.st_ino)
            if targets is not None:
                if identity in targets:
                    _fail("DUPLICATE_REFERENCE")
                targets.add(identity)
            data = stream.read(limit + 1)
    except OSError:
        _fail("MISSING_REFERENCE")
    if len(data) > limit:
        _fail("OUT_OF_BUDGET")
    return data


def _paeth(left: int, above: int, upper_left: int) -> int:
    prediction = left + above - upper_left
    distances = (abs(prediction - left), abs(prediction - above),
                 abs(prediction - upper_left))
    return (left, above, upper_left)[distances.index(min(distances))]


def decode_png_subset(data: bytes, budget: BudgetPolicy, *, retained_bytes: int = 0,
                      fault: Callable[[str], None] | None = None) -> Texture:
    if not data.startswith(PNG_SIGNATURE):
        _fail("INVALID_PNG")
    cursor = len(PNG_SIGNATURE)
    width = height = channels = 0
    seen_ihdr = seen_idat = seen_iend = False
    ended_idat = False
    compressed = bytearray()
    while cursor + 12 <= len(data):
        length = int.from_bytes(data[cursor:cursor + 4], "big")
        kind = data[cursor + 4:cursor + 8]
        end = cursor + 12 + length
        if end > len(data) or len(kind) != 4 or \
                not all(65 <= c <= 90 or 97 <= c <= 122 for c in kind) or kind[2] & 0x20:
            _fail("INVALID_PNG")
        payload = data[cursor + 8:cursor + 8 + length]
        crc = int.from_bytes(data[cursor + 8 + length:end], "big")
        if zlib.crc32(payload, zlib.crc32(kind)) & 0xffffffff != crc:
            _fail("INVALID_PNG_CRC")
        cursor = end
        if not seen_ihdr and kind != b"IHDR":
            _fail("INVALID_PNG")
        if kind == b"IHDR":
            if seen_ihdr or length != 13:
                _fail("INVALID_PNG")
            width, height, depth, color, compression, filtering, interlace = \
                struct.unpack(">IIBBBBB", payload)
            if not 0 < width <= budget.max_png_side or \
                    not 0 < height <= budget.max_png_side:
                _fail("OUT_OF_BUDGET")
            if depth != 8 or color not in (2, 6) or \
                    compression != 0 or filtering != 0 or interlace != 0:
                _fail("UNSUPPORTED_PNG")
            channels = 3 if color == 2 else 4
            if width * height * 4 > budget.max_decoded_texture_bytes:
                _fail("OUT_OF_BUDGET")
            # Conservative simultaneous buffers: encoded input/chunk/IDAT,
            # inflated rows, mutable and immutable RGBA, row scratch/overhead.
            working_bound = (retained_bytes + 4 * len(data) + 3 * width * height * 4
                             + 8 * width * 4 + height + MIB)
            if working_bound > budget.max_working_bytes:
                _fail("OUT_OF_BUDGET")
            seen_ihdr = True
        elif kind == b"IDAT":
            if ended_idat or seen_iend:
                _fail("INVALID_PNG")
            if len(compressed) + length > budget.max_encoded_texture_bytes:
                _fail("OUT_OF_BUDGET")
            compressed.extend(payload)
            seen_idat = True
        elif kind == b"IEND":
            if not seen_idat or seen_iend or length != 0 or cursor != len(data):
                _fail("INVALID_PNG")
            seen_iend = True
            break
        else:
            if seen_idat:
                ended_idat = True
            if kind in (b"tRNS", b"acTL", b"fcTL", b"fdAT") or \
                    not (kind[0] & 0x20):
                _fail("UNSUPPORTED_PNG")
    if not seen_iend:
        _fail("INVALID_PNG")
    row_size = width * channels
    expected = height * (row_size + 1)
    rgba_size = width * height * 4
    if expected + rgba_size + len(compressed) > budget.max_working_bytes:
        _fail("OUT_OF_BUDGET")
    try:
        if fault:
            fault("png_inflate")
        decoder = zlib.decompressobj()
        raw = decoder.decompress(compressed, expected + 1)
        if len(raw) != expected or not decoder.eof or decoder.unused_data or \
                decoder.unconsumed_tail or decoder.flush():
            _fail("INVALID_PNG")
    except zlib.error:
        _fail("INVALID_PNG")
    if fault:
        fault("png_rgba")
    rgba = bytearray(rgba_size)
    previous = bytearray(row_size)
    offset = 0
    for y in range(height):
        filter_type = raw[offset]
        offset += 1
        row = bytearray(raw[offset:offset + row_size])
        offset += row_size
        if filter_type > 4:
            _fail("INVALID_PNG")
        for x in range(row_size) if filter_type else ():
            left = row[x - channels] if x >= channels else 0
            above = previous[x]
            upper_left = previous[x - channels] if x >= channels else 0
            if filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                dl, da, du = abs(p - left), abs(p - above), abs(p - upper_left)
                predictor = left if dl <= da and dl <= du else above if da <= du else upper_left
            else:
                predictor = 0
            row[x] = (row[x] + predictor) & 0xff
        destination = y * width * 4
        if channels == 4:
            rgba[destination:destination + width * 4] = row
        else:
            for x in range(width):
                rgba[destination + x * 4:destination + x * 4 + 3] = \
                    row[x * 3:x * 3 + 3]
                rgba[destination + x * 4 + 3] = 255
        previous = row
    if fault:
        fault("png_publish")
    return Texture(width, height, bytes(rgba), working_bound)


def _admit_package(root: Path, relative_manifest: str, budget: BudgetPolicy,
                   requested_features: tuple[str, ...],
                   fault: Callable[[str], None] | None, retained_bytes: int) -> ModelPackage:
    if any(type(value) is not int or value <= 0 for value in vars(budget).values()) or \
            type(retained_bytes) is not int or retained_bytes < 0:
        _fail("INVALID_BUDGET")
    if any(feature not in ("moc", "textures") for feature in requested_features):
        _fail("UNSUPPORTED_REQUESTED_FEATURE")
    if fault:
        fault("manifest_read")
    try:
        root = root.resolve(strict=True)
    except OSError:
        _fail("INVALID_ROOT")
    if not root.is_dir():
        _fail("INVALID_ROOT")
    manifest_ref = _relative_reference(relative_manifest, ".model3.json")
    reference_base = manifest_ref.parent
    targets: set[tuple[int, int]] = set()
    manifest_bytes = _read_contained(root, manifest_ref, budget.max_manifest_bytes, targets)
    try:
        manifest = json.loads(manifest_bytes.decode("utf-8"),
                              object_pairs_hook=_json_object,
                              parse_constant=_reject_constant)
    except (ValueError, UnicodeDecodeError, RecursionError):
        _fail("INVALID_MANIFEST")
    if not isinstance(manifest, dict) or manifest.get("Version") != 3 or \
            not isinstance(manifest.get("FileReferences"), dict):
        _fail("UNSUPPORTED_MANIFEST")
    refs = manifest["FileReferences"]
    if any(key not in ("Moc", "Textures", "Physics", "Expressions", "Motions",
                       "Pose", "UserData", "DisplayInfo") for key in refs):
        _fail("UNSUPPORTED_MANIFEST_FEATURE")
    disabled = tuple(sorted(key for key in refs if key not in ("Moc", "Textures")))
    textures = refs.get("Textures")
    if not isinstance(textures, list) or not 1 <= len(textures) <= budget.max_texture_count:
        _fail("INVALID_TEXTURE_LIST")
    moc_ref = _relative_reference(refs.get("Moc"), ".moc3")
    seen = {str(manifest_ref).casefold(), str(moc_ref).casefold()}
    texture_refs = []
    for value in textures:
        reference = _relative_reference(value, ".png")
        key = str(reference).casefold()
        if key in seen:
            _fail("DUPLICATE_REFERENCE")
        seen.add(key)
        texture_refs.append(reference)
    physics_ref = None
    physics_bytes = None
    if "Physics" in refs:
        physics_ref = _relative_reference(refs["Physics"], ".physics3.json")
        if str(physics_ref).casefold() in seen:
            _fail("DUPLICATE_REFERENCE")
        seen.add(str(physics_ref).casefold())
        physics_bytes = _read_contained(root, reference_base / physics_ref,
                                         budget.max_manifest_bytes, targets)
    if fault:
        fault("moc_read")
    moc = _read_contained(root, reference_base / moc_ref, budget.max_moc_bytes, targets)
    if len(moc) < 4 or not moc.startswith(b"MOC3"):
        _fail("INVALID_MOC_HEADER")
    package_textures = []
    encoded_total = decoded_total = 0
    base_bytes = retained_bytes + len(manifest_bytes) + len(moc) + len(physics_bytes or b"")
    if base_bytes > budget.max_working_bytes:
        _fail("OUT_OF_BUDGET")
    working_bound = base_bytes
    for reference in texture_refs:
        remaining = budget.max_encoded_texture_bytes - encoded_total
        if remaining <= 0:
            _fail("OUT_OF_BUDGET")
        if fault:
            fault("texture_read")
        encoded = _read_contained(root, reference_base / reference,
                                   min(remaining, budget.max_working_bytes), targets)
        encoded_total += len(encoded)
        available = budget.max_decoded_texture_bytes - decoded_total
        if available <= 0:
            _fail("OUT_OF_BUDGET")
        texture = decode_png_subset(encoded, replace(budget, max_decoded_texture_bytes=available),
                                    retained_bytes=base_bytes + decoded_total, fault=fault)
        working_bound = max(working_bound, texture.working_bound)
        decoded_total += len(texture.rgba)
        if decoded_total > budget.max_decoded_texture_bytes:
            _fail("OUT_OF_BUDGET")
        package_textures.append(texture)
    if fault:
        fault("package_publish")
    return ModelPackage(moc, tuple(package_textures),
                        str(physics_ref) if physics_ref else None,
                        encoded_total, decoded_total, linked_physics_bytes=physics_bytes,
                        disabled_linked_features=disabled, working_bound=working_bound,
                        moc_reference=str(moc_ref))


def admit_package(root: Path, relative_manifest: str,
                  budget: BudgetPolicy = BudgetPolicy(), *,
                  requested_features: tuple[str, ...] = (),
                  fault: Callable[[str], None] | None = None,
                  retained_bytes: int = 0) -> ModelPackage:
    try:
        return _admit_package(root, relative_manifest, budget, requested_features, fault, retained_bytes)
    except MemoryError as error:
        raise AdmissionError("ALLOCATION_FAILED") from error
