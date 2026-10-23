"""G5 opt-in schema semantic oracles. No Native writer or private model access.

python -B -m pytest -q tests/moc3/g5_schema_redfirst.py
"""
import json
from pathlib import Path
import pytest
from jsonschema import Draft202012Validator

SCHEMA_PATH=Path("docs/in-progress/studio-beta-20261023/G5_LOCAL_CAS_ENVELOPE_PROPOSED_20261023.schema.json")
SCHEMA=json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
Draft202012Validator.check_schema(SCHEMA)
VALIDATOR=Draft202012Validator(SCHEMA)

def valid(document):
    return VALIDATOR.is_valid(document)

def request(operation="draft.begin",mode="stage",args=None,revision=7):
    if args is None:
        args={"drawable":0} if operation=="draft.begin" else {}
    return {"api_version":2,"request_id":"req-g5-schema-1",
            "session_id":"session-g5-fixture-1",
            "expected_model_generation":55,
            "expected_edit_revision":revision,
            "operation":operation,"arguments":args,"mode":mode}

def response(state,result,error=None):
    return {"api_version":2,"request_id":"req-g5-schema-1",
            "state":state,"result":result,"error":error}

def test_control_valid_begin_and_recovery_request():
    assert valid(request())
    status=request("draft.status","read",{"request_id":"req-g5-original"},revision=None)
    assert valid(status)

@pytest.mark.parametrize("mutation",[
    {"operation":"draft.begin","mode":"edit"},
    {"expected_edit_revision":None},
    {"expected_model_generation":True},
    {"session_id":None},
    {"arguments":{"drawable":0,"private_model_path":"synthetic-private-path"}},
    {"operation":"draft.begin","unexpected_field":True}
])
def test_control_closed_cas_request_rejects_invalid(mutation):
    case=request()
    case.update(mutation)
    assert not valid(case),mutation

def test_control_committed_is_not_applied():
    assert valid(response("COMMITTED",{"document_revision":8,"applied":False,
                                       "accepted_frame_generation":None}))
    assert valid(response("APPLIED",{"document_revision":8,"applied":True,
                                     "accepted_frame_generation":43,"solver_revision":9}))
    assert not valid(response("APPLIED",{"document_revision":8,"applied":False,
                                         "accepted_frame_generation":43,"solver_revision":9}))

def test_red_committed_must_not_claim_accepted_frame():
    assert not valid(response("COMMITTED",{"document_revision":8,
                                           "applied":False,
                                           "accepted_frame_generation":43}))

def test_red_unknown_outcome_must_retain_timeout_uncertainty():
    assert valid(response("UNKNOWN_OUTCOME",None,{"code":"TIMEOUT_UNKNOWN"}))
    assert not valid(response("UNKNOWN_OUTCOME",None,{"code":"COMMIT_FAILED"}))

def test_red_recovery_outcome_must_use_bounded_enum():
    assert not valid(response("COMPLETED_READ",{
        "original_request_id":"req-g5-original",
        "outcome":"CERTAIN_NOT_APPLIED",
        "document_revision":8,
        "applied":False,
        "applied_document_revision":None,
        "accepted_frame_generation":None}))
