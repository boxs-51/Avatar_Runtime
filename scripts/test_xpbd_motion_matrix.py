"""Run physical motion/parameter-change cases locally; no model data in reports."""
from __future__ import annotations
import json, re, subprocess, sys
from datetime import datetime
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from moc3_local_replay import _private_manifest

def main()->int:
    model=_private_manifest()
    if model is None or not model.is_file():
        print("STOP: no trusted local fixture");return 2
    exe=ROOT/"build/avatar-studio/Release/avatar_studio.exe"
    matrix=ROOT/"build/studio-user"/("xpbd-motion-matrix-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    matrix.mkdir(parents=True)
    cases=[]
    for case,name in [(1,"gravity-step"),(2,"wind-reversal"),(3,"damping-change")]:
        out=matrix/name
        out.mkdir()
        with (out/"native.log").open("wb") as log:
            try:
                run=subprocess.run([str(exe),"--model",str(model),"--evidence",str(out),
                    "--frames","125","--xpbd-motion-case",str(case)],
                    cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=45)
            except subprocess.TimeoutExpired:
                cases.append({"case":name,"result":"FAIL","reason":"timeout"});continue
        raw=(out/"native.log").read_text(encoding="utf-8",errors="replace")
        m=re.search(r"XPBD_SMOKE_RESULT frames=(\d+) accepted=(\d+) checks=(\d+) failures=(\d+) active=(\d+)",raw)
        result={"case":name,"exit":run.returncode,"result":"FAIL"}
        if m:
            frames,accepted,checks,failures,active=map(int,m.groups())
            result.update(frames=frames,accepted=accepted,checks=checks,failures=failures,active=active)
            if (run.returncode==0 and frames==125 and accepted>=20
                    and checks>=4 and failures==0 and active==0):
                result["result"]="PASS"
        cases.append(result)
        print(f"{name}: {result['result']} accepted={result.get('accepted',0)}")
    summary={"result":"PASS" if len(cases)==3 and all(c["result"]=="PASS" for c in cases)
             else "FAIL","cases":cases}
    (matrix/"summary.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
    print("RESULT="+summary["result"]+" LOCAL_REPORT="+str(matrix.relative_to(ROOT)))
    return 0 if summary["result"]=="PASS" else 1

if __name__=="__main__":raise SystemExit(main())
