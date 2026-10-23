"""Native Windows cross-link readback smoke; private model, no save/apply."""
from datetime import datetime
import json
from pathlib import Path
from test_studio_g4_native_live import ROOT, EXE, studio, local, native_raw
from moc3_local_replay import _private_manifest

def select_strict_target(source_vertices,candidates):
    """Return only a target that can exercise child >= source vertex count."""
    for drawable,vertices in candidates:
        if type(drawable) is int and type(vertices) is int and (
                0<=drawable<=2047 and source_vertices<vertices<=2048):
            return drawable,vertices
    return None


def run():
    model=_private_manifest()
    if model is None or not model.is_file() or not EXE.is_file():
        print("G4_CROSS_NATIVE=BLOCKED_FIXTURE")
        return 3
    folder=ROOT/"build/studio-user"/("g4-cross-e2e-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    folder.mkdir(parents=True,exist_ok=False)
    try:
        with studio(folder/"native",model) as (bridge,epoch,source):
            status=bridge.call({"op":"studio.status"})
            if not status["ok"]:raise RuntimeError("STATUS")
            count=int(dict(x.split("=",1) for x in status["receipt"].split())["drawables"])
            source_meta=bridge.call({"op":"g4.mesh.vertex_count","generation":epoch,"drawable":source})
            if not source_meta["ok"]:raise RuntimeError("SOURCE_META")
            source_attrs=dict(x.split("=",1) for x in source_meta["receipt"].split())
            if int(source_attrs["drawable"])!=source:raise RuntimeError("SOURCE_ID")
            source_vertices=int(source_attrs["vertices"])
            candidates=[]
            for d in range(min(count,128)):
                if d==source:continue
                meta=bridge.call({"op":"g4.mesh.vertex_count","generation":epoch,"drawable":d})
                if meta["ok"]:
                    attrs=dict(x.split("=",1) for x in meta["receipt"].split())
                    if int(attrs["drawable"])!=d:raise RuntimeError("TARGET_ID")
                    candidates.append((d,int(attrs["vertices"])))
            target=select_strict_target(source_vertices,candidates)
            if target is None:
                print("G4_CROSS_NATIVE=BLOCKED_NO_SECOND_ELIGIBLE_DRAWABLE")
                return 3
            d,vertices=target
            # Native framing, epoch and drawable input negatives for the new opcode.
            for wire in (
                f"G4_MESH_VERTEX_COUNT {epoch} {d} trailing",
                f"G4_MESH_VERTEX_COUNT {epoch} -1",
                f"G4_MESH_VERTEX_COUNT {epoch} {count}",
                f"G4_MESH_VERTEX_COUNT {epoch+1} {d}",
            ):
                if native_raw(folder/"native",wire)!="ERR":
                    raise RuntimeError("COUNT_FENCE_BYPASS")
            child=vertices-1
            if child<source_vertices:raise RuntimeError("TARGET_NOT_STRICT")
            add=bridge.call({"op":"rig.cross","target":d,"parent":0,"child":child,
                             "stiffness":0.75,"enabled":True})
            if not add["ok"]:raise RuntimeError("ADD")
            result=local(bridge,"rig.entries.page",{"drawable":source,
                         "kind":"cross","start":0,"limit":1},epoch)
            if result["state"]!="COMPLETED_READ":raise RuntimeError("READ")
            item=result["result"]["items"][0]
            if item["target"]!=d or item["child"]!=child:
                raise RuntimeError("MISMATCH")
            before=bridge.call({"op":"g4.rig.revision","generation":epoch,"drawable":source})
            bad=bridge.call({"op":"rig.cross","target":d,"parent":0,
                             "child":vertices,"stiffness":0.75,"enabled":True})
            if bad["ok"]:raise RuntimeError("INVALID_ACCEPTED")
            after=bridge.call({"op":"g4.rig.revision","generation":epoch,"drawable":source})
            if not before["ok"] or not after["ok"] or before["receipt"]!=after["receipt"]:
                raise RuntimeError("FAILED_MUTATION_REVISION")
            check=local(bridge,"rig.entries.page",{"drawable":source,
                         "kind":"cross","start":0,"limit":1},epoch)
            if check["state"]!="COMPLETED_READ" or check["result"]["items"][0]["child"]!=child:
                raise RuntimeError("POST_NEGATIVE")
        summary={"result":"PASS","case_count":9,"type":"physical_native",
                 "private_model_no_save":True,"tested_child_exceeds_source":child>=source_vertices}
        (folder/"safe-summary.json").write_text(json.dumps(summary),encoding="utf-8")
        print("G4_CROSS_NATIVE=PASS cases=9 target_child_exceeds_source="+
              str(child>=source_vertices).lower())
        return 0
    except Exception:
        print("G4_CROSS_NATIVE=FAIL_PRIVATE_DIAGNOSTIC_WITHHELD")
        return 1

if __name__=="__main__":
    raise SystemExit(run())
