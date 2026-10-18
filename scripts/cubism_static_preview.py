"""Convert a Cubism model's supplied portrait PNG to a static ARSCHAR preview.

This deliberately does not decode .moc3 or preserve its drawable animation.
"""

import argparse
import json
import pathlib
import struct
import zipfile


def source_file(root: pathlib.Path, name: str) -> pathlib.Path:
    if not isinstance(name, str) or not name or "\\" in name:
        raise ValueError("invalid Cubism reference")
    relative = pathlib.PurePosixPath(name)
    if relative.is_absolute() or any(p in ("", ".", "..") for p in relative.parts):
        raise ValueError("unsafe Cubism reference")
    resolved = (root / name).resolve()
    if not resolved.is_relative_to(root.resolve()) or not resolved.is_file():
        raise ValueError(f"missing or escaping Cubism reference: {name}")
    return resolved


def png_dimensions(data: bytes) -> tuple[int, int]:
    if len(data) < 33 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError("preview is not a PNG")
    width, height = struct.unpack_from(">II", data, 16)
    if not 1 <= width <= 8192 or not 1 <= height <= 8192:
        raise ValueError("preview dimensions exceed ARS2D limits")
    return width, height


def convert(model_path: pathlib.Path, preview_path: pathlib.Path, output: pathlib.Path) -> None:
    root = model_path.resolve().parent
    model = json.loads(model_path.read_text(encoding="utf-8"))
    if model.get("Version") != 3 or not isinstance(model.get("FileReferences"), dict):
        raise ValueError("expected Cubism model3 version 3")
    references = model["FileReferences"]
    moc = source_file(root, references.get("Moc"))
    with moc.open("rb") as source:
        valid_moc = source.read(4) == b"MOC3"
    if moc.suffix.lower() != ".moc3" or not valid_moc:
        raise ValueError("invalid MOC3 reference")
    textures = references.get("Textures")
    if not isinstance(textures, list) or not textures or len(textures) > 64:
        raise ValueError("invalid Cubism texture list")
    for name in textures:
        if source_file(root, name).suffix.lower() != ".png":
            raise ValueError("Cubism texture must be PNG")
    for key in ("Physics", "DisplayInfo", "Pose", "UserData"):
        if key in references:
            source_file(root, references[key])
    preview = preview_path.resolve()
    if not preview.is_file() or preview.stat().st_size > 32 * 1024 * 1024:
        raise ValueError("missing or oversized portrait PNG")
    image = preview.read_bytes()
    width, height = png_dimensions(image)
    manifest = {
        "schema_version": "0.1.0",
        "bundle_id": "org.ars.cubism-static-preview",
        "display_name": model_path.name.removesuffix(".model3.json") + " static preview",
        "default_mode": "2d", "units": "meters",
        "representations": {"2d": {"format": "ars2d-rig-v0.1",
            "asset": "assets/character2d.ars2d.json", "atlas": "assets/atlas.png",
            "mappings": [], "layer_bindings": [{"layer_id": "preview", "part_ids": ["preview"]}]}},
        "rig_semantics": [],
        "visual_layers": [{"id": "preview", "mode_scope": "2d", "z_index": 0,
                           "visible": True, "opacity": 1, "blend_mode": "normal"}],
    }
    rig = {"schema_version": "0.1.0", "coordinate_space": "canvas_px",
           "canvas": {"width": width, "height": height}, "parameters": [],
           "parts": [{"id": "preview", "z_index": 0,
                      "atlas_rect_px": [0, 0, width, height],
                      "vertices_px": [[0, 0], [width, 0], [width, height], [0, height]],
                      "uv": [[0, 0], [1, 0], [1, 1], [0, 1]],
                      "triangles": [[0, 1, 2], [0, 2, 3]]}]}
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as archive:
        for name, content in (("character.json", json.dumps(manifest, separators=(",", ":")).encode()),
                              ("assets/character2d.ars2d.json", json.dumps(rig, separators=(",", ":")).encode()),
                              ("assets/atlas.png", image)):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, content)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model3", type=pathlib.Path)
    parser.add_argument("portrait_png", type=pathlib.Path,
                        help="A rendered portrait, not a Cubism texture atlas")
    parser.add_argument("output_arschar", type=pathlib.Path)
    args = parser.parse_args()
    convert(args.model3, args.portrait_png, args.output_arschar)
    print(args.output_arschar)


if __name__ == "__main__":
    main()
