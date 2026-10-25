"""Isolated product Studio Rig edit/history/bank E2E; never export model data."""
from __future__ import annotations
import re
import shutil
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT,EXE,studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def history(tools,epoch):
    answer=tools.call("rig.history",generation=epoch)
    assert answer["ok"],"HISTORY_NOT_AVAILABLE"
    result=re.fullmatch(r"revision=([0-9]+) undo=([0-9]+) redo=([0-9]+)",answer["receipt"])
    assert result,"HISTORY_SCHEMA"
    return tuple(map(int,result.groups()))

def main():
    model=_private_manifest()
    if model is None or not model.is_file() or not EXE.is_file():
        print("RIG_LOCAL_E2E=BLOCKED_LOCAL_FIXTURE")
        return 3
    root=ROOT/"build/studio-user"/("rig-local-e2e-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    root.mkdir(parents=True,exist_ok=False)
    try:
        with studio(root/"trial",model) as (bridge,epoch,drawable):
            tools=AgentStudioTools(bridge)
            first=history(tools,epoch)
            inspect=tools.call("rig.inspect")
            assert inspect["ok"] and "anchors=0" in inspect["receipt"],"BASE_RIG_NOT_EMPTY"
            edit=tools.call("rig.anchor",vertex=0,enabled=True)
            assert edit["ok"],"ANCHOR_EDIT"
            second=history(tools,epoch)
            assert second[0]==first[0]+1 and second[1]==1 and second[2]==0,"HISTORY_RECORD"
            stale=tools.call("rig.undo",generation=epoch,revision=first[0])
            assert not stale["ok"] and history(tools,epoch)==second,"STALE_DENIAL"
            undo=tools.call("rig.undo",generation=epoch,revision=second[0])
            assert undo["ok"],"UNDO_FAILURE"
            third=history(tools,epoch)
            assert third[0]==second[0]+1 and third[1]==0 and third[2]==1,"UNDO_STATE"
            assert "anchors=0" in tools.call("rig.inspect")["receipt"],"UNDO_DID_NOT_RESTORE"
            redo=tools.call("rig.redo",generation=epoch,revision=third[0])
            assert redo["ok"],"REDO_FAILURE"
            fourth=history(tools,epoch)
            assert fourth[0]==third[0]+1 and fourth[1]==1 and fourth[2]==0,"REDO_STATE"
            assert "anchors=1" in tools.call("rig.inspect")["receipt"],"REDO_DID_NOT_RESTORE"
            before=tools.call("studio.status")
            assert before["ok"],"STATUS"
            m=re.search(r"drawables=([0-9]+)",before["receipt"])
            assert m,"DRAWABLE_TOTAL"
            changed=False
            for d in range(min(int(m.group(1)),64)):
                if d==drawable:continue
                if not tools.call("g4.mesh.meta",generation=epoch,drawable=d)["ok"]:continue
                select=tools.call("rig.select",drawable=d)
                if not select["ok"]:continue
                changed=True
                alt=history(tools,epoch)
                assert alt[1]==0 and alt[2]==0,"HISTORY_CROSSED_DRAWABLE"
                assert tools.call("rig.select",drawable=drawable)["ok"],"BANK_RETURN"
                assert "anchors=1" in tools.call("rig.inspect")["receipt"],"BANK_LOST_RIG"
                assert history(tools,epoch)[1]==0,"BANK_HISTORY_NOT_RESET"
                break
            print("RIG_LOCAL_E2E_PASS edit_undo_redo=1 stale_cas=1 "+
                  "bank_swap="+("1" if changed else "NOT_AVAILABLE")+
                  " model_private=1")
            return 0
    finally:
        shutil.rmtree(root,ignore_errors=True)

if __name__=="__main__":
    raise SystemExit(main())
