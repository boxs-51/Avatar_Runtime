"""Physical model-private legacy XPBD frame witness — G5 writer always HOLD.

No private model paths, geometry or captures are printed or published.
Uses two isolated Studio processes and an already-local private fixture.
"""
from datetime import datetime
import shutil
import time
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def eventually(test,timeout=5):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        state=test()
        if state["legacy_frame_current"]:return state
        time.sleep(.05)
    raise AssertionError("NO_LEGACY_RENDER_FRAME_WITNESS")

def main():
    model=_private_manifest()
    if model is None or not model.is_file() or not EXE.is_file():
        print("LEGACY_FRAME_PHYSICAL_BLOCKED")
        return 3
    root=ROOT/"build/studio-user"/("g5-frame-witness-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    root.mkdir(parents=True,exist_ok=False)
    try:
        with studio(root/"run",model) as (bridge,epoch,drawable):
            tools=AgentStudioTools(bridge)
            initial=tools.legacy_frame_status()
            assert initial["g5_applied"] is False
            assert initial["legacy_frame_current"] is False
            assert initial["model_generation"]==epoch
            assert tools.call("rig.apply")["ok"] is True
            first=eventually(tools.legacy_frame_status)
            assert first["bound_rig_revision"]==first["legacy_rig_revision"]
            assert first["accepted_rig_revision"]==first["legacy_rig_revision"]
            assert first["g5_writer_enabled"] is False
            edit=tools.call("rig.anchor",vertex=0,enabled=True)
            assert edit["ok"] is True
            stale=tools.legacy_frame_status()
            assert stale["legacy_frame_current"] is False
            assert stale["legacy_rig_revision"]>=first["legacy_rig_revision"]
            assert stale["g5_applied"] is False
            assert tools.call("rig.apply")["ok"] is True
            rebound=eventually(tools.legacy_frame_status)
            assert rebound["legacy_frame_current"] is True
            assert rebound["accepted_frame_serial"]>=first["accepted_frame_serial"]
            assert rebound["g5_applied"] is False
        print("G5_LEGACY_FRAME_PHYSICAL_PASS gpu_accept=real "
              "stale_edit=blocked rebind=accepted g5_writer=HOLD "
              "no_model_publication=1")
        return 0
    finally:
        shutil.rmtree(root,ignore_errors=True)

if __name__=="__main__":
    raise SystemExit(main())
