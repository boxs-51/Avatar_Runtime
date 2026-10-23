"""Codex PR81 RED-first: failed UI verdict and malformed source-mesh rig counts."""
import json
import tempfile
from pathlib import Path

from scripts.avatar_toolkit_v2 import execute_request


class RigMetaBridge:
    def __init__(self, **changes):
        self.values=dict(drawable=0,vertices=3,role=0,anchors=2,beams=0,parents=0,weights=0,cross=0)
        self.values.update(changes)
    def call(self, command):
        op=command["op"]
        if op=="studio.epoch":return {"ok":True,"receipt":"generation=55"}
        if op=="studio.status":return {"ok":True,"receipt":"frame=1 playing=0 expression=0 tab=3 xpbd=0 params=1 parts=1 drawables=1"}
        if op=="g4.rig.revision":return {"ok":True,"receipt":"revision=7"}
        if op=="g4.rig.meta":
            return {"ok":True,"receipt":" ".join(f"{k}={v}" for k,v in self.values.items())}
        raise AssertionError(op)


def rig_meta(bridge):
    request={"api_version":2,"request_id":"req-review-81","session_id":None,
             "expected_model_generation":55,"expected_edit_revision":None,
             "operation":"rig.local.inspect","arguments":{"drawable":0},"mode":"read"}
    return json.loads(execute_request(json.dumps(request).encode(),local_bridge=bridge,allow_local_private=True))


def test_codex81_rig_counts_fail_closed():
    for fields in ({"anchors":4},{"weights":4},{"parents":3},{"beams":4},
                   {"anchors":64,"beams":128,"parents":64,"weights":256}):
        answer=rig_meta(RigMetaBridge(**fields))
        assert answer["state"]=="REJECTED",fields
        assert answer["error"]["code"]=="INVALID_DRAFT",fields
        assert answer["result"] is None


def test_codex81_valid_boundary_counts_pass():
    answer=rig_meta(RigMetaBridge(anchors=3,beams=3,parents=2,weights=3))
    assert answer["state"]=="COMPLETED_READ"


def test_codex81_finish_propagates_verdict():
    from scripts.test_studio_g4_ui_race_live import finish
    for verdict in ("SEMANTIC_RED_MIXED_SNAPSHOT","SEMANTIC_RED_ABA_ACCEPTED",
                    "HARNESS_OR_ENV_FAILURE","BLOCKED","UNEXPECTED_STATE"):
        with tempfile.TemporaryDirectory() as path:
            folder=Path(path)
            (folder/"result.json").write_text(json.dumps({"verdict":verdict,"actor":"UI_DESKTOP","edit_verified":False}))
            assert finish(folder)!=0,verdict
    for verdict in ("REJECTED_AFTER_EDITOR_CHANGE","REJECTED_ABA_STALE_REVISION"):
        with tempfile.TemporaryDirectory() as path:
            folder=Path(path)
            proof={"verdict":verdict,"actor":"UI_DESKTOP",
                   "edit_verified":True,"revision_increased":True,
                   "native_process":True,"agent_finished":True}
            (folder/"result.json").write_text(json.dumps(proof))
            assert finish(folder)==0,verdict
            proof["edit_verified"]=False
            (folder/"result.json").write_text(json.dumps(proof))
            assert finish(folder)!=0,verdict


def test_codex81_ui_revision_hook_covers_all_edit_surfaces():
    """Source-wiring regression; not a physical GUI/same-frame acceptance test."""
    c=(Path("tools/avatar-studio/studio.c")).read_text(encoding="utf-8")
    assert "ui_rig_checkpoint=&ui_rig_before;" in c
    assert "studio_note_ui_rig_edit();" in c
    assert "ui_rig_checkpoint=NULL;" in c
    ui=c[c.index("static void rig_overlay_ui("):c.index("static int studio_selected_topology(")]
    editor=c[c.index("static void rig_ui("):c.index("static int ",c.index("static void rig_ui(")+1)]
    for token in (
        "studio_rig_toggle_anchor(&s.rig,nearest)",
        "studio_rig_toggle_beam(&s.rig,s.rig_beam_start,nearest)",
        "studio_rig_bank_select(&s.rig_bank,&s.rig,d,signature(),n)",
    ):
        idx=ui.index(token)
        assert "studio_note_ui_rig_edit();" in ui[idx:idx+160],token
    for token in (
        "studio_rig_toggle_anchor(&s.rig,s.rig_selected)",
        "studio_rig_toggle_beam(&s.rig,s.rig_beam_start,s.rig_selected)",
        "studio_rig_load(&s.rig,path",
        "studio_rig_parent(&s.rig,s.rig_parent_start",
        "studio_rig_parent(&s.rig,s.rig.parents[i].parent",
        "studio_rig_weight(&s.rig,s.rig_selected",
        "studio_rig_brush(&s.rig,points",
        "studio_rig_cross(&s.rig,s.rig_selected",
        "toast(studio_adopt_proposal()?",
    ):
        idx=editor.index(token)
        assert "studio_note_ui_rig_edit();" in editor[idx:idx+350],token
