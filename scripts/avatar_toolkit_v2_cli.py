"""Standalone local V2 discovery CLI, deliberately not connected to Studio."""
from __future__ import annotations
import json
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from avatar_toolkit_v2 import execute_request,MAX_REQUEST_BYTES

def main(argv=None)->int:
    args=list(sys.argv[1:] if argv is None else argv)
    request={"api_version":2,"request_id":"req-cli-0001",
       "session_id":None,"expected_model_generation":None,
       "expected_edit_revision":None,"mode":"read",
       "operation":"avatar.capabilities","arguments":{}}
    if args==["capabilities"]:
        pass
    elif len(args)==2 and args[0]=="schema":
        request["operation"]="avatar.schema"
        request["arguments"]={"operation":args[1]}
    elif args==["--stdin"]:
        wire=sys.stdin.buffer.read(MAX_REQUEST_BYTES+1)
        outcome=execute_request(wire)
        sys.stdout.write(outcome)
        return 0 if '"state":"COMPLETED_READ"' in outcome else 2
    else:
        sys.stdout.write(json.dumps({"api_version":2,"request_id":None,
          "state":"REJECTED","result":None,
          "error":{"code":"INVALID_ARGUMENT"}},separators=(",",":"))+"\n")
        return 2
    reply=execute_request(json.dumps(request).encode("utf-8"))
    sys.stdout.write(reply)
    return 0 if '"state":"COMPLETED_READ"' in reply else 2

if __name__=="__main__":
    raise SystemExit(main())
