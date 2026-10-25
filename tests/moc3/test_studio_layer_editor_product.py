"""Product-gated Layer Editor: shared Native state, read-only geometry."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/"tools/avatar-studio/studio.c").read_text(encoding="utf-8")
CMAKE=(ROOT/"tools/avatar-studio/CMakeLists.txt").read_text(encoding="utf-8")
API=(ROOT/"scripts/studio_agent_api.py").read_text(encoding="utf-8")

def test_layer_editor_built_into_real_studio():
    product=CMAKE.split("add_executable(avatar_studio ",1)[1].split(")",1)[0]
    assert "studio_layer_editor.c" in product
    assert "studio_layer_editor_tests" in CMAKE
    assert "studio_layer_editor_cursor" in CMAKE

def test_viewport_and_layer_panel_share_layer_editor_cursor():
    assert "layer_overlay_ui();" in SOURCE
    assert "studio_layer_editor_pick(projected" in SOURCE
    assert "studio_layer_editor_select_vertex(&s.layer_editor" in SOURCE
    assert "studio_layer_editor_select_triangle(&s.layer_editor" in SOURCE
    assert "DrawLineV(" in SOURCE
    assert 's.layer_editor.isolate?"Isolate ON"' in SOURCE
    assert "s.opt.onlyDrawable=s.layer_editor.isolate?drawable:-1" in SOURCE
    assert "studio_layer_editor_reset(&s.layer_editor,s.model_generation,drawable)" in SOURCE

def test_native_agent_layer_editor_api_is_bounded_and_no_product_g5_writer():
    for operation in ("LAYER_CURSOR","LAYER_VERTEX_SELECT","LAYER_TRIANGLE_SELECT",
                      "LAYER_OVERLAY","LAYER_ISOLATE"):
        assert operation in SOURCE
    for name in ("layer.cursor","layer.vertex.select","layer.triangle.select",
                 "layer.overlay","layer.isolate"):
        assert name in API
    assert "g5_txn_apply(" not in SOURCE
    assert "g5_bus_submit_trusted(" not in SOURCE
