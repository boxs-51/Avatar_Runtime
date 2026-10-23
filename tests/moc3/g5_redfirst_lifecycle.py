"""G5 opt-in RED-first lifecycle specification, not runtime permission.

Run deliberately:
  python -m pytest -q tests/moc3/g5_redfirst_lifecycle.py --tb=short

Outside pytest's default test_*.py glob to preserve green G4 regression.
Each semantic RED below expresses a PROPOSED V2 editing contract. It must
not be implemented by bypassing current fail-closed read-only admission.
"""
import json
import unittest

from scripts.avatar_toolkit_v2 import execute_request


class NonMutatingProbe:
    """Pure synthetic Native double; records calls, never edits a real rig."""

    def __init__(self, *, scenario="baseline"):
        self.calls = []
        self.scenario = scenario

    def call(self, command):
        self.calls.append(dict(command))
        if command["op"] == "studio.epoch":
            return {"ok": True, "receipt": "generation=55"}
        if command["op"] == "studio.status":
            return {"ok": True, "receipt":
                    "frame=1 playing=0 expression=0 tab=3 xpbd=0 "
                    "params=1 parts=1 drawables=1"}
        if command["op"] == "g4.rig.revision":
            return {"ok": True, "receipt": "revision=7"}
        return {"ok": False, "receipt": "rejected"}


def issue(operation, arguments=None, *, request_id="req-g5-lifecycle",
          generation=55, revision=7, session="session-g5-synthetic",
          mode="stage", bridge=None, enabled=True):
    request = {
        "api_version": 2, "request_id": request_id,
        "session_id": session,
        "expected_model_generation": generation,
        "expected_edit_revision": revision,
        "operation": operation, "arguments": {} if arguments is None else arguments,
        "mode": mode,
    }
    response = execute_request(
        json.dumps(request, separators=(",", ":")).encode("utf-8"),
        local_bridge=bridge, allow_local_private=enabled)
    return json.loads(response)


def rejected_no_result(testcase, response, code):
    testcase.assertEqual(response["state"], "REJECTED")
    testcase.assertEqual(response["error"]["code"], code)
    testcase.assertIsNone(response["result"])


class G5LifecycleControl(unittest.TestCase):
    """These must remain GREEN until an authorized G5 writer is enabled."""

    def test_control_current_capabilities_do_not_advertise_edits(self):
        reply = issue("avatar.capabilities", {}, mode="read", generation=None,
                      revision=None, session=None)
        self.assertEqual(reply["state"], "COMPLETED_READ")
        self.assertFalse(reply["result"]["mutating_commands_enabled"])

    def test_control_denied_writer_never_reaches_native(self):
        probe = NonMutatingProbe()
        reply = issue("draft.begin", {"drawable": 0}, bridge=probe)
        self.assertEqual(reply["state"], "REJECTED")
        self.assertIsNone(reply["result"])
        self.assertEqual(probe.calls, [])

    def test_control_path_injection_fails_without_native_access(self):
        probe = NonMutatingProbe()
        reply = issue("draft.begin", {"drawable": 0, "source_path":
                      "synthetic-unapproved-model-path"}, bridge=probe)
        self.assertEqual(reply["state"], "REJECTED")
        self.assertIsNone(reply["result"])
        self.assertEqual(probe.calls, [])

    def test_control_invalid_json_duplicate_fields_is_rejected(self):
        source = b'{"api_version":2,"api_version":2}'
        response = json.loads(execute_request(source, local_bridge=NonMutatingProbe(),
                                              allow_local_private=True))
        self.assertEqual(response["state"], "REJECTED")
        self.assertIsNone(response["result"])


class G5LifecycleRed(unittest.TestCase):
    """Future expected states, deliberately FAIL on the read-only baseline."""

    def begin(self, bridge, *, revision=7):
        response = issue("draft.begin", {"drawable": 0}, revision=revision,
                         bridge=bridge)
        self.assertEqual(response["state"], "STAGED")
        self.assertTrue(response["result"]["draft_id"])
        self.assertEqual(response["result"]["base_revision"], revision)
        return response["result"]["draft_id"]

    def validated(self, bridge):
        draft = self.begin(bridge)
        verified = issue("draft.validate", {"draft_id": draft},
                         bridge=bridge, mode="stage")
        self.assertEqual(verified["state"], "VALIDATED")
        self.assertFalse(verified["result"].get("applied", False))
        return draft

    def committed(self, bridge):
        draft = self.validated(bridge)
        result = issue("draft.commit", {"draft_id": draft},
                       bridge=bridge, mode="edit")
        self.assertEqual(result["state"], "COMMITTED")
        self.assertEqual(result["result"]["document_revision"], 8)
        self.assertFalse(result["result"].get("applied", False))
        return result

    def test_red_begin_private_draft_with_cas(self):
        result = issue("draft.begin", {"drawable": 0}, bridge=NonMutatingProbe())
        self.assertEqual(result["state"], "STAGED")
        self.assertEqual(result["result"]["base_revision"], 7)
        self.assertTrue(result["result"]["draft_id"])
        self.assertFalse(result["result"].get("applied", False))

    def test_red_validate_accepted_candidate_never_claims_applied(self):
        bridge = NonMutatingProbe()
        draft = self.begin(bridge)
        result = issue("draft.validate", {"draft_id": draft},
                       bridge=bridge, mode="stage")
        self.assertEqual(result["state"], "VALIDATED")
        self.assertEqual(result["result"]["base_revision"], 7)
        self.assertFalse(result["result"].get("applied", False))

    def test_red_validate_unknown_draft_fails_without_effect(self):
        # Unknown draft must fail before editor/solver effects.
        probe = NonMutatingProbe(scenario="missing_draft")
        result = issue("draft.validate", {"draft_id": "draft-not-owned"},
                       bridge=probe, mode="stage")
        rejected_no_result(self, result, "INVALID_DRAFT")

    def test_red_invalid_constraint_does_not_touch_live_editor(self):
        bridge = NonMutatingProbe(scenario="beam_self_link")
        draft = self.begin(bridge)
        result = issue("rig.constraint.upsert",
                       {"draft_id": draft, "kind": "beam", "a": 1, "b": 1},
                       mode="stage", bridge=bridge)
        rejected_no_result(self, result, "INVALID_DRAFT")

    def test_red_commit_requires_validated_draft(self):
        result = issue("draft.commit",
                       {"draft_id": "draft-synthetic-unvalidated"},
                       mode="edit", bridge=NonMutatingProbe())
        rejected_no_result(self, result, "INVALID_DRAFT")

    def test_red_commit_ack_is_not_accepted_solver_frame(self):
        bridge = NonMutatingProbe(scenario="valid_commit")
        result = self.committed(bridge)
        self.assertFalse(result["result"].get("applied", False))
        self.assertIsNone(result["result"].get("accepted_frame_generation"))

    def test_red_abort_uncommitted_draft(self):
        bridge = NonMutatingProbe(scenario="abort")
        draft = self.begin(bridge)
        result = issue("draft.abort", {"draft_id": draft},
                       mode="edit", bridge=bridge)
        self.assertEqual(result["state"], "ABORTED")
        self.assertFalse(result["result"].get("applied", False))

    def test_red_commit_after_abort_is_invalid(self):
        bridge = NonMutatingProbe(scenario="aborted")
        draft = self.begin(bridge)
        abort = issue("draft.abort", {"draft_id": draft},
                      mode="edit", bridge=bridge)
        self.assertEqual(abort["state"], "ABORTED")
        result = issue("draft.commit", {"draft_id": draft},
                       mode="edit", bridge=bridge)
        rejected_no_result(self, result, "INVALID_DRAFT")

    def test_red_undo_is_new_revision_not_time_travel(self):
        bridge = NonMutatingProbe(scenario="undo")
        self.committed(bridge)
        result = issue("draft.undo", {}, mode="edit", revision=8, bridge=bridge)
        self.assertEqual(result["state"], "COMMITTED")
        self.assertEqual(result["result"]["document_revision"], 9)
        self.assertFalse(result["result"].get("applied", False))

    def test_red_redo_appends_new_revision(self):
        bridge = NonMutatingProbe(scenario="redo")
        self.committed(bridge)
        undone = issue("draft.undo", {}, mode="edit", revision=8, bridge=bridge)
        self.assertEqual(undone["state"], "COMMITTED")
        self.assertEqual(undone["result"]["document_revision"], 9)
        result = issue("draft.redo", {}, mode="edit", revision=9, bridge=bridge)
        self.assertEqual(result["state"], "COMMITTED")
        self.assertEqual(result["result"]["document_revision"], 10)
        self.assertFalse(result["result"].get("applied", False))

    def test_red_redo_after_divergent_edit_is_rejected(self):
        bridge = NonMutatingProbe(scenario="redo_divergence")
        self.committed(bridge)
        undo = issue("draft.undo", {}, mode="edit", revision=8, bridge=bridge)
        self.assertEqual(undo["state"], "COMMITTED")
        draft = self.begin(bridge, revision=9)
        valid = issue("draft.validate", {"draft_id": draft},
                      revision=9, mode="stage", bridge=bridge)
        self.assertEqual(valid["state"], "VALIDATED")
        commit = issue("draft.commit", {"draft_id": draft},
                       revision=9, mode="edit", bridge=bridge)
        self.assertEqual(commit["state"], "COMMITTED")
        self.assertEqual(commit["result"]["document_revision"], 10)
        result = issue("draft.redo", {}, mode="edit", revision=10, bridge=bridge)
        rejected_no_result(self, result, "REVISION_CONFLICT")

    def test_red_stale_begin_detects_revision_conflict(self):
        result = issue("draft.begin", {"drawable": 0}, revision=6,
                       bridge=NonMutatingProbe())
        rejected_no_result(self, result, "REVISION_CONFLICT")

    def test_red_second_writer_receives_busy_without_lease(self):
        bridge = NonMutatingProbe(scenario="lease_contended")
        first = self.begin(bridge)
        self.assertTrue(first)
        result = issue("draft.begin", {"drawable": 0}, request_id="req-g5-second",
                       bridge=bridge)
        rejected_no_result(self, result, "BUSY")

    def test_red_stale_commit_does_not_overwrite_ui_revision(self):
        bridge = NonMutatingProbe(scenario="ui_edit_conflict")
        draft = self.validated(bridge)
        # Owner UI has advanced to revision 8 after validation.
        result = issue("draft.commit", {"draft_id": draft},
                       revision=7, mode="edit", bridge=bridge)
        rejected_no_result(self, result, "REVISION_CONFLICT")

    def test_red_stale_model_generation_fails_before_commit(self):
        bridge = NonMutatingProbe(scenario="model_reload")
        draft = self.validated(bridge)
        result = issue("draft.commit", {"draft_id": draft},
                       generation=54, mode="edit", bridge=bridge)
        rejected_no_result(self, result, "STALE_MODEL")

    def test_red_unrecognized_session_cannot_claim_editor_lease(self):
        result = issue("draft.begin", {"drawable": 0},
                       session="session-g5-foreign", bridge=NonMutatingProbe())
        rejected_no_result(self, result, "STALE_SESSION")

    def test_red_lost_ack_is_unknown_not_committed_or_applied(self):
        # Synthetic fault scenario; future Native fixture must delay ACK after
        # effect, not only return a canned error without any commit.
        bridge = NonMutatingProbe(scenario="ack_lost_after_commit")
        draft = self.validated(bridge)
        result = issue("draft.commit", {"draft_id": draft},
                       request_id="req-g5-acklost", mode="edit", bridge=bridge)
        self.assertEqual(result["state"], "UNKNOWN_OUTCOME")
        self.assertEqual(result["error"]["code"], "TIMEOUT_UNKNOWN")
        self.assertIsNone(result["result"])

    def test_red_unknown_outcome_requires_readback_not_replay(self):
        bridge = NonMutatingProbe(scenario="ack_reconcile")
        draft = self.validated(bridge)
        unknown = issue("draft.commit", {"draft_id": draft},
                        request_id="req-g5-acklost", mode="edit", bridge=bridge)
        self.assertEqual(unknown["state"], "UNKNOWN_OUTCOME")
        result = issue("draft.status", {"request_id": "req-g5-acklost"},
                       mode="read", bridge=bridge)
        self.assertEqual(result["state"], "COMPLETED_READ")
        self.assertIn(result["result"]["outcome"],
                      ("COMMITTED", "NOT_APPLIED", "UNKNOWN"))
        self.assertFalse(result["result"].get("applied", False))

    def test_red_apply_requires_acceptance_fence_after_commit(self):
        bridge = NonMutatingProbe(scenario="frame_accepted")
        self.committed(bridge)
        result = issue("physics.apply", {"document_revision": 8},
                       mode="edit", revision=8, bridge=bridge)
        self.assertEqual(result["state"], "APPLIED")
        self.assertEqual(result["result"]["document_revision"], 8)
        self.assertGreater(result["result"]["accepted_frame_generation"], 0)
        self.assertGreater(result["result"]["solver_revision"], 0)

    def test_red_unaccepted_frame_must_not_claim_applied(self):
        bridge = NonMutatingProbe(scenario="frame_rejected")
        self.committed(bridge)
        result = issue("physics.apply", {"document_revision": 8},
                       mode="edit", revision=8, bridge=bridge)
        rejected_no_result(self, result, "COMMIT_FAILED")


if __name__ == "__main__":
    unittest.main()
