"""Local Cubism model3 reference and budget preflight; never validates MOC3 internals."""

import argparse
import json
import pathlib
import struct
import zlib


MIB = 1024 * 1024
MAX_MODEL_JSON = 1 * MIB
MAX_MOC = 64 * MIB
MAX_TEXTURE_FILE = 32 * MIB
MAX_LINKED_JSON = 8 * MIB
MAX_RGBA = 512 * MIB  # Private preflight guard, not a public runtime budget.
ARS_V01_PIXEL_LIMIT = 128 * MIB


class PreflightError(ValueError):
    def __init__(self, code: str):
        self.code = code
        super().__init__(code)


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise PreflightError("DUPLICATE_JSON_KEY")
        result[key] = value
    return result


def reject_constant(_):
    raise PreflightError("INVALID_JSON")


def read_json(path: pathlib.Path, limit: int):
    if path.stat().st_size > limit:
        raise PreflightError("JSON_TOO_LARGE")
    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_pairs,
                          parse_constant=reject_constant)
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise PreflightError("INVALID_JSON") from error


def reference(root: pathlib.Path, value, seen: set[str], suffix: str, limit: int) -> pathlib.Path:
    if not isinstance(value, str) or not value or "\\" in value or ":" in value:
        raise PreflightError("INVALID_REFERENCE")
    parts = value.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise PreflightError("INVALID_REFERENCE")
    candidate = root.joinpath(*parts).resolve()
    if not candidate.is_relative_to(root) or not candidate.is_file():
        raise PreflightError("MISSING_OR_ESCAPING_REFERENCE")
    canonical = str(candidate).casefold()
    if canonical in seen:
        raise PreflightError("DUPLICATE_REFERENCE")
    seen.add(canonical)
    if candidate.suffix.lower() != suffix:
        raise PreflightError("WRONG_REFERENCE_TYPE")
    if candidate.stat().st_size > limit:
        raise PreflightError("REFERENCE_TOO_LARGE")
    return candidate


def png_size(path: pathlib.Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(33)
    if (len(header) != 33 or header[:8] != b"\x89PNG\r\n\x1a\n"
            or header[8:16] != b"\x00\x00\x00\x0dIHDR"
            or zlib.crc32(header[12:29]) != struct.unpack_from(">I", header, 29)[0]):
        raise PreflightError("INVALID_PNG_HEADER")
    width, height, depth, color, compression, filtering, interlace = struct.unpack_from(">IIBBBBB", header, 16)
    if not (1 <= width <= 8192 and 1 <= height <= 8192 and depth == 8
            and color in (2, 6) and compression == filtering == interlace == 0):
        raise PreflightError("UNSUPPORTED_PNG_HEADER")
    return width, height


def inspect(model_path: pathlib.Path) -> dict:
    model_path = model_path.resolve()
    if not model_path.is_file() or not model_path.name.endswith(".model3.json"):
        raise PreflightError("INVALID_MODEL_PATH")
    root = model_path.parent
    model = read_json(model_path, MAX_MODEL_JSON)
    if not isinstance(model, dict) or type(model.get("Version")) is not int or model["Version"] != 3:
        raise PreflightError("UNSUPPORTED_MODEL_VERSION")
    refs = model.get("FileReferences")
    if not isinstance(refs, dict):
        raise PreflightError("INVALID_FILE_REFERENCES")
    allowed = {"Moc", "Textures", "Physics", "DisplayInfo", "Pose", "UserData"}
    if set(refs) - allowed:
        raise PreflightError("UNSUPPORTED_REFERENCE_KIND")
    seen: set[str] = set()
    moc = reference(root, refs.get("Moc"), seen, ".moc3", MAX_MOC)
    with moc.open("rb") as source:
        if source.read(4) != b"MOC3":
            raise PreflightError("INVALID_MOC_HEADER")
    textures = refs.get("Textures")
    if not isinstance(textures, list) or not 1 <= len(textures) <= 64:
        raise PreflightError("INVALID_TEXTURE_LIST")
    rgba = encoded = 0
    dimensions = []
    for value in textures:
        texture = reference(root, value, seen, ".png", MAX_TEXTURE_FILE)
        width, height = png_size(texture)
        dimensions.append([width, height])
        rgba += width * height * 4
        encoded += texture.stat().st_size
        if rgba > MAX_RGBA:
            raise PreflightError("TEXTURE_BUDGET_EXCEEDED")
    linked = []
    for key in ("Physics", "DisplayInfo", "Pose", "UserData"):
        if key in refs:
            path = reference(root, refs[key], seen, ".json", MAX_LINKED_JSON)
            if not isinstance(read_json(path, MAX_LINKED_JSON), dict):
                raise PreflightError("INVALID_LINKED_JSON")
            linked.append(key)
    animation = root / "animation"
    sidecars = sum(1 for p in animation.glob("*.exp3.json") if p.is_file()) if animation.is_dir() else 0
    return {"model3_version": 3, "moc_bytes": moc.stat().st_size,
            "moc_core_validation": "NOT_RUN", "png_validation": "HEADER_ONLY",
            "texture_count": len(textures),
            "texture_dimensions": dimensions, "encoded_texture_bytes": encoded,
            "decoded_rgba_bytes": rgba, "ars_v01_pixel_limit_bytes": ARS_V01_PIXEL_LIMIT,
            "exceeds_ars_v01_pixel_limit": rgba > ARS_V01_PIXEL_LIMIT,
            "linked_json_kinds": linked, "declared_expressions": 0,
            "unreferenced_exp3_sidecars": sidecars,
            "dynamic_readiness": "HOLD_SDK_AND_BUDGET" if rgba > ARS_V01_PIXEL_LIMIT else "HOLD_SDK"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model3", type=pathlib.Path)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.model3), separators=(",", ":")))
    except (OSError, PreflightError) as error:
        print(json.dumps({"status": "REJECT", "code": error.code if isinstance(error, PreflightError) else "IO_ERROR"}))
        raise SystemExit(2) from None


if __name__ == "__main__":
    main()
