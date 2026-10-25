"""RED-first two-process G5 Agent task transport contract, no real Studio model."""
import pytest
from scripts.studio_g5_fixture_ipc import FixtureIpcBackend, parse_request, parse_response


def test_recovery_lane_is_read_only_and_mutations_use_distinct_channel(tmp_path):
    backend=FixtureIpcBackend(tmp_path)
    assert backend.lane_for("draft.status")=="recovery"
    assert backend.lane_for("draft.receipt.status")=="recovery"
    assert backend.lane_for("draft.commit")=="mutation"
    assert backend.lane_for("draft.undo")=="mutation"
    assert backend.lane_for("draft.reconcile")=="mutation"


@pytest.mark.parametrize("payload",[
 b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","operation":"draft.status","args":{"session_epoch":99,"model_generation":55},"evil":"x"}',
 b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","operation":"draft.begin","operation":"draft.status","args":{"session_epoch":99,"model_generation":55}}',
 b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","operation":"draft.commit","args":{"session_epoch":99,"model_generation":55,"draft_id":1,"request_id":700,"expected_revision":7,"agent_id":"other"}}',
 b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","operation":"agent.register","args":{}}',
 b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","operation":"draft.begin","args":{"expected_revision":true,"session_epoch":99,"model_generation":55}}',
 b'\x00',
])
def test_strict_parser_rejects_malformed_or_governance_payload(payload):
    with pytest.raises(ValueError):
        parse_request(payload)



def test_non_string_operation_fails_closed_without_type_exception():
    malformed = b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","operation":[],"args":{"session_epoch":99,"model_generation":55}}'
    with pytest.raises(ValueError):
        parse_request(malformed)


def test_parser_accepts_bounded_function_call():
    data=b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","operation":"draft.begin","args":{"expected_revision":7,"session_epoch":99,"model_generation":55}}'
    token,op,args=parse_request(data)
    assert len(token)==24 and op=="draft.begin"
    assert args["expected_revision"]==7


def test_response_must_match_token_and_cannot_embed_model_data():
    reply=b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","result":{"code":"OK","draft_id":1}}'
    assert parse_response(reply,"a"*24)["draft_id"]==1
    with pytest.raises(ValueError):
        parse_response(reply,"b"*24)
    with pytest.raises(ValueError):
        parse_response(b'{"token":"aaaaaaaaaaaaaaaaaaaaaaaa","result":{"code":"OK","model_path":"private"}}',"a"*24)


def test_fixture_backend_cannot_be_used_as_public_agent_registry(tmp_path):
    backend=FixtureIpcBackend(tmp_path)
    with pytest.raises(ValueError):
        backend.call_g5("agent.register",{})
