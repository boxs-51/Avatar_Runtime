"""Source proof that UI, Agent and Layer selection share Native RigBank."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=(ROOT/"tools/avatar-studio/studio.c")

def test_rig_ui_keeps_navigation_available_on_unsupported_mesh():
    s=SOURCE.read_text(encoding="utf-8")
    body=s.split("static void rig_ui(float x){",1)[1].split("static void xpbd_ui(",1)[0]
    assert "vc[d]<1||vc[d]>2048" in body
    assert "rig_select_relative(-1)" in body
    assert "rig_select_relative(1)" in body

def test_agent_and_layer_commands_route_to_single_selector():
    s=SOURCE.read_text(encoding="utf-8")
    assert "rig_select(drawable,signature(),counts[drawable])" in s
    assert 'RIG_SELECT_AT' in s and 'LAYER_SELECT' in s
    assert 'RIG_SELECTION' in s and 'LAYER_INSPECT' in s
    assert "studio_rig_dispatch_select(" in s
    assert "g5_bus_submit_trusted(" not in s
