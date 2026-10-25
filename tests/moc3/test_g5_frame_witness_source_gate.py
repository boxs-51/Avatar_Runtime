"""G5 document-to-frame safety: legacy frame witness is not G5 APPLIED."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
STUDIO=ROOT/"tools/avatar-studio/studio.c"
TXN=ROOT/"tools/avatar-studio/studio_g5_txn.c"

def test_frame_witness_only_after_real_solver_and_gpu_accept():
    s=STUDIO.read_text(encoding="utf-8")
    frame=s[s.index("static void frame(void){"):s.index("int main(",s.index("static void frame(void){"))]
    assert "studio_xpbd_prepare(s.xpbd,s.core.model)" in frame
    assert "glFinish();" in frame
    assert "!studio_xpbd_accept(s.xpbd)" in frame
    assert "studio_rig_frame_accept(&s.legacy_frame_fence" in frame
    assert frame.index("glFinish();")<frame.index("!studio_xpbd_accept(s.xpbd)")<frame.index(
        "studio_rig_frame_accept(&s.legacy_frame_fence")
    assert "studio_rig_frame_unbind(&s.legacy_frame_fence)" in frame

def test_native_g5_applied_is_not_set_by_legacy_frame_witness():
    studio=STUDIO.read_text(encoding="utf-8")
    assert "g5_txn_apply(" not in studio
    assert "G5_LEGACY_FRAME_STATUS" in (
        ROOT/"tools/avatar-studio/studio_agent_extended.inc").read_text(encoding="utf-8")
    assert "frame=LEGACY_ONLY" in (
        ROOT/"tools/avatar-studio/studio_rig_frame_fence.c").read_text(encoding="utf-8")
    assert "g5_txn_apply(" in TXN.read_text(encoding="utf-8")


def test_agent_xpbd_replacement_invalidates_old_rig_frame():
    # A GPU-accepted frame belongs to the previous Rig/solver binding.
    # Switching to generic XPBD or disabling the solver cannot retain it.
    source=(ROOT/"tools/avatar-studio/studio_agent_extended.inc").read_text(encoding="utf-8")
    for op,next_op in (("XPBD_APPLY","XPBD_DISABLE"),("XPBD_DISABLE","XPBD_INSPECT")):
        start=source.index('if(!strcmp(op,"'+op+'"))')
        end=source.index('if(!strcmp(op,"'+next_op+'"))',start)
        section=source[start:end]
        assert "studio_xpbd_disable(s.xpbd)" in section
        assert "studio_rig_frame_unbind(&s.legacy_frame_fence)" in section
        assert section.index("studio_xpbd_disable(s.xpbd)") < section.index(
            "studio_rig_frame_unbind(&s.legacy_frame_fence)")
