"""Static regression oracle: UI and V1 Rig edits use one owner-thread commit."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[2]
STUDIO=ROOT/"tools/avatar-studio/studio.c"
EXT=ROOT/"tools/avatar-studio/studio_agent_extended.inc"

def test_live_rig_mutations_have_no_direct_bypass():
    studio=STUDIO.read_text(encoding="utf-8")
    extra=EXT.read_text(encoding="utf-8")
    for content in (studio,extra):
        assert not re.search(r"\bs\.rig\s*=",content)
        for function in ("studio_rig_toggle_anchor","studio_rig_toggle_beam",
                         "studio_rig_parent","studio_rig_weight",
                         "studio_rig_brush","studio_rig_cross",
                         "studio_rig_set_role","studio_rig_load",
                         "studio_rig_bank_select"):
            assert not re.search(re.escape(function)+r"\s*\(\s*&s\.rig",content)
    assert "studio_rig_dispatch(&s.rig,&s.rig_revision" in studio
    assert "studio_rig_dispatch.c" in (
        ROOT/"tools/avatar-studio/CMakeLists.txt").read_text(encoding="utf-8")


def test_product_recovery_is_polled_before_mutation_ack_queue():
    studio=STUDIO.read_text(encoding="utf-8")
    start=studio.index("static void process_agent_request(void){")
    body=studio[start:studio.index("static void frame(void){",start)]
    assert body.index("studio_v1_recovery_poll(")<body.index("if(pending_ack)")
    assert "studio_v1_recovery_store(&s.v1_recovery,token,ok)" in body
    assert "studio_v1_recovery_lookup(&s.v1_recovery,token)" in body
    assert "memset(&s.v1_recovery,0" in studio


def test_rig_history_and_bank_are_live_product_wiring():
    studio=STUDIO.read_text(encoding="utf-8")
    cm=(ROOT/"tools/avatar-studio/CMakeLists.txt").read_text(encoding="utf-8")
    api=(ROOT/"scripts/studio_agent_api.py").read_text(encoding="utf-8")
    assert "studio_rig_dispatch_select(" in studio
    assert "studio_rig_history_record(&s.rig_history" in studio
    assert "studio_rig_history_step(&s.rig_history" in studio
    assert 'RIG_HISTORY' in studio and 'RIG_UNDO' in studio and 'RIG_REDO' in studio
    assert "studio_rig_history.c" in cm.split("add_executable(avatar_studio",1)[1].split(")",1)[0]
    for name in ("rig.history","rig.undo","rig.redo"):
        assert name in api
    # Do not reactivate the separate unverified G5 writer as a side effect.
    assert "g5_bus_submit_trusted(" not in studio
    assert "g5_txn_apply(" not in studio
