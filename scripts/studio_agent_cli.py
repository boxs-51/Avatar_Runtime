"""Public local Agent CLI for Native Studio. Requires a running Studio window."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from studio_agent_api import AgentBridge
ROOT=Path(__file__).resolve().parents[1]
def main()->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command",help='JSON command, for example {"op":"rig.select","drawable":0}')
    parser.add_argument("--timeout",type=float,default=4.)
    args=parser.parse_args()
    if not .1<=args.timeout<=30:parser.error("timeout must be 0.1..30 seconds")
    try:
        command=json.loads(args.command)
        result=AgentBridge(ROOT/"build/studio-user/api",timeout=args.timeout).call(command)
    except (OSError,ValueError,TimeoutError,RuntimeError) as exc:
        print(json.dumps({"ok":False,"error":str(exc)}))
        return 1
    print(json.dumps(result,ensure_ascii=False))
    return 0 if result["ok"] else 1
if __name__=="__main__":raise SystemExit(main())
