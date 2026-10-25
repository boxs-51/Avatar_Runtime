"""Native Studio local sidecar geometry editing contract."""
from pathlib import Path
import pytest
from scripts.studio_agent_api import validate_command,operation_fields

ROOT=Path(__file__).resolve().parents[2]
STUDIO=(ROOT/"tools/avatar-studio/studio.c").read_text(encoding="utf-8")
HANDLER=(ROOT/"tools/avatar-studio/studio_geometry_agent.inc").read_text(encoding="utf-8")
CORE=(ROOT/"tools/avatar-studio/studio_geometry_document.c").read_text(encoding="utf-8")
UI=(ROOT/"tools/avatar-studio/studio_geometry_ui.inc").read_text(encoding="utf-8")

def test_geometry_authoring_product_wiring():
    cm=(ROOT/"tools/avatar-studio/CMakeLists.txt").read_text(encoding="utf-8")
    shipping=cm.split("add_executable(avatar_studio ",1)[1].split(")",1)[0]
    assert "studio_geometry_document.c" in shipping
    assert '#include "studio_geometry_agent.inc"' in STUDIO
    assert '#include "studio_geometry_ui.inc"' in STUDIO
    assert "geometry_overlay_ui();" in STUDIO
    assert "studio_geometry_create(&geometry" in HANDLER
    assert "studio_geometry_import_with_uv(&geometry" in STUDIO
    assert "studio_geometry_triangle_add(&geometry" in UI
    assert "studio_geometry_vertex_move(&geometry" in UI
    assert "g5_bus_submit_trusted(" not in STUDIO

def test_geometry_sidecar_fixed_private_path_and_model_fingerprint():
    assert 'layer_geometry.txt' in HANDLER
    assert 'layer_geometry.tmp' in HANDLER
    assert 'layer_geometry.bak' in HANDLER
    assert "studio_replace_file(temp,path)" in HANDLER
    assert "geometry.signature!=geometry_signature()" in HANDLER
    assert "geometry_file_fingerprint(input.moc)" in STUDIO
    assert "studio_geometry_load(candidate,geometry_signature(),f)" in HANDLER
    assert "studio_geometry_save(&geometry,geometry_signature(),f)" in HANDLER

def test_geometry_ops_require_exact_bounded_arguments():
    accepted=[
       ({"op":"layer.project.status","generation":5},"GEOM_STATUS 5"),
        ({"op":"layer.rig.links","generation":5},"GEOM_RIG_LINKS 5"),
        ({"op":"layer.rig.link.inspect","generation":5,"revision":9,"index":0},
         "GEOM_RIG_LINK_INSPECT 5 9 0"),
        ({"op":"layer.physics.point_by_id","generation":5,"id":91,
          "vertex_id":3},"GEOM_PHYSICS_POINT_BY_ID 5 91 3"),
        ({"op":"layer.rig.link","generation":5,"revision":4,
          "parent_id":91,"parent_vertex_id":1,
          "child_id":92,"child_vertex_id":2,"stiffness":.8},
          "GEOM_RIG_LINK 5 4 91 1 92 2 0.8"),
        ({"op":"layer.rig.unlink","generation":5,"revision":5,
          "parent_id":91,"parent_vertex_id":1,
          "child_id":92,"child_vertex_id":2},
          "GEOM_RIG_UNLINK 5 5 91 1 92 2"),
        ({"op":"layer.project.vertex_by_id","generation":5,"revision":6,
          "id":91,"vertex_id":12},"GEOM_VERTEX_BY_ID 5 6 91 12"),
       ({"op":"layer.project.by_id","generation":5,"revision":8,"id":26},
         "GEOM_LAYER_BY_ID 5 8 26"),
       ({"op":"layer.active.id","generation":5,"revision":8,"id":26},
         "GEOM_ACTIVE_ID 5 8 26"),
       ({"op":"layer.rig.weight","generation":5,"revision":2,"layer":0,
         "vertex":1,"weight":.3},"GEOM_RIG_WEIGHT 5 2 0 1 0.3"),
       ({"op":"layer.rig.parent","generation":5,"revision":3,"layer":0,
         "vertex":2,"parent":0,"stiffness":.8},"GEOM_RIG_PARENT 5 3 0 2 0 0.8"),
       ({"op":"layer.model.compile","generation":5,"revision":3},
         "GEOM_COMPILE 5 3"),
       ({"op":"layer.model.open","generation":5,"revision":3},
         "GEOM_OPEN_COMPILED 5 3"),
       ({"op":"layer.uv.set","generation":5,"revision":1,"layer":0,"vertex":2,
         "u":.2,"v":.8},"GEOM_UV_SET 5 1 0 2 0.2 0.8"),
       ({"op":"layer.material.set","generation":5,"revision":2,"layer":0,
         "r":.2,"g":.3,"b":.4,"opacity":.8},"GEOM_MATERIAL_SET 5 2 0 0.2 0.3 0.4 0.8"),
       ({"op":"layer.rig.pin","generation":5,"revision":3,"layer":0,
         "vertex":-1},"GEOM_PIN_SET 5 3 0 -1"),
       ({"op":"layer.physics.status","generation":5},"GEOM_PHYSICS_STATUS 5"),
       ({"op":"layer.physics.point","generation":5,"vertex":2},"GEOM_PHYSICS_POINT 5 2"),
       ({"op":"layer.physics.bind","generation":5,"revision":4,
         "layer":0},"GEOM_PHYSICS_BIND 5 4 0"),

       ({"op":"layer.project.layer","generation":5,"layer":0},"GEOM_LAYER 5 0"),
       ({"op":"layer.project.point","generation":5,"layer":0,"vertex":2},
        "GEOM_POINT 5 0 2"),
       ({"op":"layer.project.face","generation":5,"layer":0,"triangle":1},
        "GEOM_FACE 5 0 1"),
       ({"op":"layer.create","generation":5,"revision":0,"id":1},"GEOM_CREATE 5 0 1"),
       ({"op":"layer.copy.drawable","generation":5,"revision":2,"drawable":1,"id":3},
        "GEOM_COPY_DRAWABLE 5 2 1 3"),
       ({"op":"layer.duplicate","generation":5,"revision":3,"layer":0,"id":9},
        "GEOM_DUPLICATE 5 3 0 9"),
       ({"op":"layer.reorder","generation":5,"revision":4,"layer":1,"target":0},
        "GEOM_REORDER 5 4 1 0"),
       ({"op":"layer.point.move","generation":5,"revision":7,"layer":0,
         "vertex":1,"x":.5,"y":-.5},"GEOM_VERTEX_MOVE 5 7 0 1 0.5 -0.5"),
    ]
    for data,expected in accepted:
        assert validate_command(data)==expected
    denied=[
       {"op":"layer.create","generation":True,"revision":0,"id":1},
       {"op":"layer.create","generation":5,"revision":-1,"id":1},
       {"op":"layer.point.add","generation":5,"revision":2,"layer":0,"x":float("nan"),"y":0},
       {"op":"layer.point.add","generation":5,"revision":2,"layer":0,"x":10001,"y":0},
       {"op":"layer.delete","generation":5,"revision":1,"layer":-1},
       {"op":"layer.create","generation":5,"revision":0,"id":1,"path":"outside"},
        {"op":"layer.rig.link","generation":5,"revision":4,
          "parent_id":91,"parent_vertex_id":1,
          "child_id":91,"child_vertex_id":2,"stiffness":.8},
        {"op":"layer.rig.link","generation":5,"revision":4,
          "parent_id":91,"parent_vertex_id":1,
          "child_id":92,"child_vertex_id":2,"stiffness":float("nan")},
        {"op":"layer.project.vertex_by_id","generation":5,"revision":6,
          "id":91,"vertex_id":0},
       {"op":"layer.rig.weight","generation":5,"revision":3,"layer":0,
         "vertex":1,"weight":float("inf")},
       {"op":"layer.rig.parent","generation":5,"revision":3,"layer":0,
         "vertex":1,"parent":True,"stiffness":.9},
       {"op":"layer.model.compile","generation":5,"revision":3,"path":"out.bin"},
       {"op":"layer.uv.set","generation":5,"revision":1,"layer":0,"vertex":0,
         "u":float("inf"),"v":.1},
       {"op":"layer.material.set","generation":5,"revision":1,"layer":0,
         "r":.5,"g":.4,"b":.3,"opacity":1.5},
       {"op":"layer.rig.pin","generation":5,"revision":1,"layer":0,"vertex":True},

    ]
    for request in denied:
        with pytest.raises(ValueError):validate_command(request)

def test_gui_script_strict_window_target():
    script=(ROOT/"scripts/test_studio_geometry_gui_win32.py").read_text(encoding="utf-8")
    for check in ("window_by_pid(process.pid)","u.GetForegroundWindow()==hwnd",
                  "WINDOW_PID_CHANGED","WINDOW_AT_POINT_MISMATCH",
                  "TARGET_WINDOW_NOT_UNIQUE"):
        assert check in script
