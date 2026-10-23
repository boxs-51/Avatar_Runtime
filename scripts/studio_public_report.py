"""Strict, model-free public Studio test verdict encoder.

The caller must explicitly provide only the classification fields.
Raw receipts/logs/model details are rejected, not redacted by guessing.
"""
from __future__ import annotations
import json
import re

_STATUSES={"PASS","FAIL","HOLD","NOT_RUN","REVISE"}
_CLASSES={"none","invalid-input","timeout","race","persistence",
          "privacy","crash","regression","unverified"}
_KEYS={"case_id","status","error_class"}

def safe_case(entry:dict)->dict:
    if type(entry) is not dict or set(entry)!=_KEYS:
        raise ValueError("unsafe public report fields")
    case,status,kind=(entry["case_id"],entry["status"],entry["error_class"])
    if type(case) is not str or not re.fullmatch(r"[A-Z][A-Z0-9_.-]{1,39}",case):
        raise ValueError("unsafe public case identifier")
    if type(status) is not str or status not in _STATUSES:
        raise ValueError("unsafe public status")
    if type(kind) is not str or kind not in _CLASSES:
        raise ValueError("unsafe public error class")
    if status=="PASS" and kind!="none":
        raise ValueError("contradictory success report")
    return {"case_id":case,"status":status,"error_class":kind}

def safe_json(cases:list[dict])->str:
    if type(cases) is not list or len(cases)>512:
        raise ValueError("invalid report")
    checked=[safe_case(x) for x in cases]
    ids=[x["case_id"] for x in checked]
    if len(set(ids))!=len(ids):
        raise ValueError("duplicate case identifier")
    return json.dumps({"schema":"studio-public-safe-v1","cases":checked},
                      ensure_ascii=True,sort_keys=True,indent=2)+"\n"
