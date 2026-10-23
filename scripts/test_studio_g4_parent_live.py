"""G4 parent relation Windows Native E2E; private model never exported."""
from __future__ import annotations
import json
from datetime import datetime
from pathlib import Path

from test_studio_g4_native_live import EXE, ROOT, studio, local, native_raw
from moc3_local_replay import _private_manifest
from scripts.avatar_toolkit_v2 import execute_request


def schema(op, bridge):
    payload = {"api_version": 2, "request_id": "req-parent-schema",
        "session_id": None, "expected_model_generation": None,
        "expected_edit_revision": None, "operation": "avatar.schema",
        "arguments": {"operation": op}, "mode": "read"}
    return json.loads(execute_request(json.dumps(payload).encode(),
                      local_bridge=bridge, allow_local_private=True))


def main():
    model = _private_manifest()
    if model is None or not model.is_file() or not EXE.is_file():
        print("G4_PARENT_NATIVE=BLOCKED_FIXTURE")
        return 3
    root = ROOT / "build/studio-user" / (
        "g4-parent-e2e-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    root.mkdir(parents=True, exist_ok=False)
    checks = []
    try:
        with studio(root / "native", model) as (bridge, epoch, drawable):
            for op, value in (("rig.local.inspect", True),
                              ("rig.entries.page", True),
                              ("mesh.vertices.page", False)):
                info = schema(op, bridge)
                assert info["state"] == "COMPLETED_READ"
                assert info["result"]["editor_revision_fence"] is value
                checks.append("SCHEMA_" + op)
            detail = bridge.call({"op": "g4.mesh.meta",
                                  "generation": epoch, "drawable": drawable})
            assert detail["ok"]
            count = int(dict(part.split("=", 1) for part in
                             detail["receipt"].split())["vertices"])
            assert count >= 3
            applied = bridge.call({"op": "rig.parent", "parent": 0,
                        "child": 1, "stiffness": 0.8, "enabled": True})
            assert applied["ok"], "VALID_PARENT_WRITE_DENIED"
            checks.append("VALID_PARENT_NATIVE_IN_MEMORY")
            page = local(bridge, "rig.entries.page",
                         {"drawable": drawable, "kind": "parent",
                          "start": 0, "limit": 1}, epoch)
            assert page["state"] == "COMPLETED_READ"
            assert page["result"]["items"][0]["parent"] == 0
            assert page["result"]["items"][0]["child"] == 1
            assert page["result"]["editor_revision_available"] is True
            checks.append("VALID_PARENT_G4_READBACK")
            if count <= 2047:
                rejected = bridge.call({"op": "rig.parent", "parent": 0,
                            "child": count, "stiffness": 0.8, "enabled": True})
                assert not rejected["ok"]
                checks.append("OUT_OF_BOUNDS_PARENT_NATIVE_DENIED")
            after = local(bridge, "rig.entries.page",
                          {"drawable": drawable, "kind": "parent",
                           "start": 0, "limit": 1}, epoch)
            assert after["state"] == "COMPLETED_READ"
            assert after["result"]["items"][0]["child"] == 1
            checks.append("POST_NEGATIVE_UNCHANGED")
            assert native_raw(root / "native",
                   f"G4_RIG_ENTRY {epoch} {drawable} parent 2047") == "ERR"
            checks.append("OUT_OF_RANGE_READ_INDEX_NATIVE_DENIED")
        summary = {"status": "PASS", "checks": checks,
                   "model_private": True, "no_file_save": True}
        (root / "public-safe-summary.json").write_text(
            json.dumps(summary), encoding="utf-8")
        print(f"G4_PARENT_NATIVE=PASS cases={len(checks)}")
        return 0
    except Exception:
        # Private errors/receipts are deliberately not shown.
        print("G4_PARENT_NATIVE=FAIL_NON_PUBLIC")
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
