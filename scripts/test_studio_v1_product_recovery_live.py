"""Real Windows Studio owner-thread: V1 duplicate prevention and read-only recovery.

The model and raw Studio responses never leave ignored private workspace.
Product recovery is volatile. Restarted receipt queries must return UNKNOWN.
"""
from __future__ import annotations
import secrets
import shutil
import time
from datetime import datetime
from scripts.test_studio_g4_native_live import ROOT, EXE, studio
from scripts.moc3_local_replay import _private_manifest
from scripts.studio_agent_tools import AgentStudioTools

def native_call(api, token, opcode, deadline=6):
    request=api/"request.txt"
    response=api/"response.txt"
    with request.open("x",encoding="ascii") as f:
        f.write(token+" "+opcode+"\n")
    until=time.monotonic()+deadline
    while time.monotonic()<until:
        if response.is_file():
            raw=response.read_bytes()
            if len(raw)>512:raise AssertionError("oversized native ack")
            try:frame=raw.decode("ascii")
            except UnicodeError as e:raise AssertionError("non ascii ACK") from e
            if frame.startswith(token+" "):
                response.unlink()
                parts=frame.strip().split(" ",2)
                assert len(parts)==3
                return parts[1]
        time.sleep(0.02)
    raise AssertionError("V1_PRODUCT_ACK_TIMEOUT")

def main():
    model=_private_manifest()
    if model is None or not model.is_file() or not EXE.is_file():
        print("V1_PRODUCT_RECOVERY=BLOCKED_LOCAL_FIXTURE")
        return 3
    folder=ROOT/"build/studio-user"/("g5-product-v1-recovery-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    folder.mkdir(parents=True,exist_ok=False)
    previous=None
    try:
        for index in range(2):
            session=folder/("run-"+str(index))
            with studio(session,model) as (bridge,epoch,drawable):
                tools=AgentStudioTools(bridge)
                recovery=bridge.recover(previous) if previous else None
                if previous:assert recovery=={"state":"UNKNOWN"}
                before=tools.g5_status()
                assert before["model_generation"]==epoch
                original=secrets.token_hex(12)
                assert bridge.recover(original)=={"state":"UNKNOWN"}
                status=native_call(session/"api",original,"RIG_ANCHOR 0 1")
                assert status=="OK"
                read=tools.recover_v1(original)
                assert read=={"state":"FOUND","ok":True}
                after=tools.g5_status()
                assert after["legacy_rig_revision"]>=before["legacy_rig_revision"]
                unchanged=after["legacy_rig_revision"]
                # A duplicate native token MUST NOT reapply its conflicting edit.
                second=native_call(session/"api",original,"RIG_ANCHOR 0 0")
                assert second=="ERR"
                assert tools.recover_v1(original)=={"state":"FOUND","ok":True}
                assert tools.g5_status()["legacy_rig_revision"]==unchanged
                assert tools.g5_status()["g5_writer_enabled"] is False
                # Simulate post-effect ACK lost to the Agent: Native publishes
                # a reply but the client does not consume it as success.
                lost=secrets.token_hex(12)
                api=session/"api"
                (api/"uncertain_outcome").write_text(lost+"\n",encoding="ascii")
                with (api/"request.txt").open("x",encoding="ascii") as f:
                    f.write(lost+" RIG_ANCHOR 1 1\n")
                until=time.monotonic()+6
                while not (api/"response.txt").is_file():
                    if time.monotonic()>until:
                        raise AssertionError("V1_LOST_ACK_TIMEOUT")
                    time.sleep(.02)
                (api/"response.txt").unlink()  # deliberate lost client ACK
                assert tools.recover_v1(lost)=={"state":"FOUND","ok":True}
                try:
                    tools.g5_status()
                except RuntimeError:
                    pass
                else:
                    raise AssertionError("unknown-outcome lane unexpectedly writable")
                try:
                    bridge.reconcile(original)
                except RuntimeError:
                    pass
                else:
                    raise AssertionError("wrong token resolved quarantine")
                assert (api/"uncertain_outcome").exists()
                assert bridge.reconcile(lost)=={"state":"RECONCILED","ok":True}
                assert not (api/"uncertain_outcome").exists()
                assert tools.g5_status()["g5_writer_enabled"] is False
                previous=original
        print("V1_REAL_STUDIO_RECOVERY_PASS sessions=2 "
              "duplicate=no_replay owner=shared_commit recovery=read_only "
              "restart=UNKNOWN writer=HOLD")
        return 0
    finally:
        shutil.rmtree(folder,ignore_errors=True)

if __name__=="__main__":
    raise SystemExit(main())
