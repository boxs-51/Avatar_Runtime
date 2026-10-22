"""Compare two *normalized* evaluator snapshots without loading model assets.

The caller is responsible for exporting evaluator data and keeping any private
snapshot and report under an ignored local directory. This module has no MOC3
parser and never prints drawable IDs or model-derived values to stdout.
"""

import argparse
import json
import math
from collections import Counter
from pathlib import Path

MAX_INPUT_BYTES = 64 * 1024 * 1024
MAX_DRAWABLES = 100_000
MAX_VERTICES = 4_000_000


def _number(value, label):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(f"{label}: expected finite number")
    return float(value)


def _vector(value, length, label):
    if not isinstance(value, list) or len(value) != length:
        raise ValueError(f"{label}: expected {length}-element array")
    return tuple(_number(component, label) for component in value)


def _records(value, label):
    if not isinstance(value, list):
        raise ValueError(f"{label}: expected array")
    found = {}
    for record in value:
        if not isinstance(record, dict) or not isinstance(record.get("id"), str) or not record["id"]:
            raise ValueError(f"{label}: record needs nonempty id")
        if record["id"] in found:
            raise ValueError(f"{label}: duplicate id")
        found[record["id"]] = record
    return found


def validate(snapshot):
    """Reject malformed/truncated exports rather than treating missing data as parity."""
    if not isinstance(snapshot, dict):
        raise ValueError("snapshot: expected object")
    parameters = _records(snapshot.get("parameters"), "parameters")
    drawables = _records(snapshot.get("drawables"), "drawables")
    if len(drawables) > MAX_DRAWABLES:
        raise ValueError("drawables: count exceeds limit")
    for item in parameters.values():
        _number(item.get("value"), "parameter value")
    total_vertices = 0
    for item in drawables.values():
        vertices = item.get("vertices")
        indices = item.get("indices")
        if not isinstance(vertices, list) or not isinstance(indices, list):
            raise ValueError("drawable: vertices and indices must be full arrays")
        if len(indices) % 3:
            raise ValueError("indices: expected complete triangles")
        total_vertices += len(vertices)
        if total_vertices > MAX_VERTICES:
            raise ValueError("vertices: count exceeds limit")
        for vertex in vertices:
            if not isinstance(vertex, dict):
                raise ValueError("vertex: expected object")
            _vector(vertex.get("pos"), 2, "vertex position")
            _vector(vertex.get("uv"), 2, "vertex UV")
        for index in indices:
            if isinstance(index, bool) or not isinstance(index, int) or index < 0 or index >= len(vertices):
                raise ValueError("index: outside vertex array")
        _number(item.get("opacity"), "opacity")
        if isinstance(item.get("texture_index"), bool) or not isinstance(item.get("texture_index"), int):
            raise ValueError("texture_index: expected integer")
        if isinstance(item.get("render_order"), bool) or not isinstance(item.get("render_order"), int):
            raise ValueError("render_order: expected integer")
        if not isinstance(item.get("masks"), list) or any(
            not isinstance(mask, str) for mask in item["masks"]
        ):
            raise ValueError("masks: expected drawable ID array")
    return parameters, drawables


def _triangle_counts(indices, reverse_winding, order_insensitive):
    triangles = []
    for offset in range(0, len(indices), 3):
        a, b, c = indices[offset:offset + 3]
        if reverse_winding:
            b, c = c, b
        triangles.append(min((a, b, c), (b, c, a), (c, a, b)))
    return Counter(triangles) if order_insensitive else triangles


def compare(left, right, tolerance=1e-4, *, right_uv_v_flip=False,
            right_reverse_winding=False, triangle_order_insensitive=False):
    if tolerance <= 0 or not math.isfinite(tolerance):
        raise ValueError("tolerance: expected positive finite number")
    left_params, left_drawables = validate(left)
    right_params, right_drawables = validate(right)
    result = {
        "parameter_id_missing_left": len(right_params.keys() - left_params.keys()),
        "parameter_id_missing_right": len(left_params.keys() - right_params.keys()),
        "drawable_id_missing_left": len(right_drawables.keys() - left_drawables.keys()),
        "drawable_id_missing_right": len(left_drawables.keys() - right_drawables.keys()),
        "parameter_value_mismatches": 0,
        "topology_mismatches": 0,
        "uv_mismatches": 0,
        "position_mismatches": 0,
        "state_mismatches": 0,
        "max_uv_delta": 0.0,
        "max_position_delta": 0.0,
        "first_difference": None,
        "conventions": {
            "right_uv_v_flip": right_uv_v_flip,
            "right_reverse_winding": right_reverse_winding,
            "triangle_order_insensitive": triangle_order_insensitive,
        },
    }

    def difference(stage, item_id=None, vertex=None):
        if result["first_difference"] is None:
            result["first_difference"] = {"stage": stage, "id": item_id, "vertex": vertex}

    for key in ("parameter_id_missing_left", "parameter_id_missing_right",
                "drawable_id_missing_left", "drawable_id_missing_right"):
        if result[key]:
            difference("identity")
            break
    for item_id in sorted(left_params.keys() & right_params.keys()):
        if abs(left_params[item_id]["value"] - right_params[item_id]["value"]) > tolerance:
            result["parameter_value_mismatches"] += 1
            difference("parameter_value", item_id)
    for item_id in sorted(left_drawables.keys() & right_drawables.keys()):
        a, b = left_drawables[item_id], right_drawables[item_id]
        av, bv = a["vertices"], b["vertices"]
        if len(av) != len(bv) or _triangle_counts(a["indices"], False, triangle_order_insensitive) != \
                _triangle_counts(b["indices"], right_reverse_winding, triangle_order_insensitive):
            result["topology_mismatches"] += 1
            difference("topology", item_id)
        for index, (vertex_a, vertex_b) in enumerate(zip(av, bv)):
            for field, counter, maximum, stage in (
                ("uv", "uv_mismatches", "max_uv_delta", "uv"),
                ("pos", "position_mismatches", "max_position_delta", "position"),
            ):
                right_value = vertex_b[field]
                if field == "uv" and right_uv_v_flip:
                    right_value = (right_value[0], 1.0 - right_value[1])
                delta = max(abs(x - y) for x, y in zip(vertex_a[field], right_value))
                result[maximum] = max(result[maximum], delta)
                if delta > tolerance:
                    result[counter] += 1
                    difference(stage, item_id, index)
        if any(a[field] != b[field] for field in ("texture_index", "render_order", "masks")) or \
                abs(a["opacity"] - b["opacity"]) > tolerance:
            result["state_mismatches"] += 1
            difference("render_state", item_id)
    result["status"] = "MATCH" if result["first_difference"] is None else "MISMATCH"
    return result


def _load(path):
    if path.stat().st_size > MAX_INPUT_BYTES:
        raise ValueError("input exceeds size limit")
    return json.loads(path.read_text(encoding="utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--tolerance", type=float, default=1e-4)
    parser.add_argument("--right-uv-v-flip", action="store_true")
    parser.add_argument("--right-reverse-winding", action="store_true")
    parser.add_argument("--triangle-order-insensitive", action="store_true")
    args = parser.parse_args()
    try:
        if args.out.resolve() in {args.left.resolve(), args.right.resolve()}:
            raise ValueError("output cannot overwrite input")
        result = compare(_load(args.left), _load(args.right), args.tolerance,
                         right_uv_v_flip=args.right_uv_v_flip,
                         right_reverse_winding=args.right_reverse_winding,
                         triangle_order_insensitive=args.triangle_order_insensitive)
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        parser.exit(2, f"comparison rejected: {exc}\n")
    print(f"COMPARISON_{result['status']}: report written")


if __name__ == "__main__":
    main()
