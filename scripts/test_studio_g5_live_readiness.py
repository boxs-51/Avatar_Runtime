"""Physical Windows read-only G5 readiness through the REAL Studio V1 owner.

Loads an existing local/private E2E model fixture without logging model data.
No G5 mutations, model export, public network service or user Studio takeover.
"""
from __future__ import annotations

import shutil
from datetime import datetime
from pathlib import Path

from scripts.studio_agent_tools import AgentStudioTools
from scripts.test_studio_g4_native_live import ROOT,EXE,studio,native_raw
from scripts.moc3_local_replay import _private_manifest


def main():
    model=_private_manifest()
    if model is None or not model.is_file() or not EXE.is_file():
        print("G5_STUDIO_READINESS=BLOCKED_LOCAL_FIXTURE_OR_BINARY")
        return 3
    folder=ROOT/"build/studio-user"/("g5-readiness-private-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    folder.mkdir(parents=True,exist_ok=False)
    try:
        previous=None
        for index in range(2):
            with studio(folder/("run-"+str(index)),model) as (bridge,epoch,_):
                tools=AgentStudioTools(bridge)
                state=tools.g5_status()
                assert state["classification"]=="LOCAL_PRIVATE"
                assert state["model_generation"]==epoch
                assert state["g5_writer_enabled"] is False
                assert state["g5_transaction_connected"] is False
                assert state["frame_acceptance_verified"] is False
                assert state["legacy_rig_revision"]>=0
                # Existing V1 Agent edit still flows through the actual Studio
                # owner thread. Its rig revision must not turn into G5 APPLY.
                changed = bridge.call({"op":"rig.anchor","vertex":0,"enabled":True})
                assert changed["ok"] is True
                updated = tools.g5_status()
                assert updated["legacy_rig_revision"] > state["legacy_rig_revision"]
                assert updated["model_generation"] == epoch
                assert updated["g5_writer_enabled"] is False
                assert updated["frame_acceptance_verified"] is False
                assert native_raw(folder/("run-"+str(index)),
                                  "G5_LOCAL_STATUS trailing")=="ERR"
                assert native_raw(folder/("run-"+str(index)),
                                  "G5_LOCAL_STATUS")=="OK"
                v2=tools.read_v2("draft.commit")
                assert v2["state"]=="REJECTED"
                if previous is not None:assert epoch!=previous
                previous=epoch
        print("G5_REAL_STUDIO_STATUS_PASS sessions=2 read_only=1 "
              "writer_hold=1 generation_rotated=1 "
              "legacy_revision_observed=1 no_model_publication=1")
        return 0
    finally:
        # Private subprocess logs/assets are not pushed or displayed.
        shutil.rmtree(folder,ignore_errors=True)


if __name__=="__main__":
    raise SystemExit(main())
