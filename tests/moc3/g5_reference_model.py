"""Opt-in, synthetic-only G5 oracle model. NEVER a Studio production writer.

Run with: python -m pytest -q tests/moc3/g5_reference_model.py
This is an executable test oracle for proposed editor semantics, NOT E2E.
"""
import unittest


class ContractFailure(Exception):
    def __init__(self, code):
        self.code = code
        super().__init__(code)


class DraftReference:
    """Pure in-memory state machine. No bridge, model file or solver access."""

    def __init__(self):
        self.generation = 55
        self.session = "session-synthetic"
        self.revision = 7
        self.anchors = frozenset((0, 1))
        self.lease = None
        self.history = []
        self.redo_stack = []
        self.receipts = {}
        self.applied_revision = 7
        self.frame = 42
        self.solver_revision = 2
        self.serial = 0

    def fence(self, rev, *, generation=55, session="session-synthetic"):
        if session != self.session:
            raise ContractFailure("STALE_SESSION")
        if generation != self.generation:
            raise ContractFailure("STALE_MODEL")
        if rev != self.revision:
            raise ContractFailure("REVISION_CONFLICT")

    def begin(self, rev, *, owner="Agent", generation=55):
        self.fence(rev, generation=generation)
        if self.lease is not None:
            raise ContractFailure("BUSY")
        self.serial += 1
        self.lease = {"id": f"draft-{self.serial}", "owner": owner,
                      "base": rev, "anchors": set(self.anchors),
                      "validated": False}
        return {"state": "STAGED", "draft_id": self.lease["id"],
                "base_revision": rev, "applied": False}

    def require(self, draft):
        if not self.lease or draft != self.lease["id"]:
            raise ContractFailure("INVALID_DRAFT")
        return self.lease

    def toggle_anchor(self, draft, vertex, enabled):
        record = self.require(draft)
        if type(vertex) is not int or not 0 <= vertex < 2048:
            raise ContractFailure("INVALID_DRAFT")
        if enabled:
            record["anchors"].add(vertex)
        else:
            record["anchors"].discard(vertex)
        record["validated"] = False

    def validate(self, draft):
        record = self.require(draft)
        if len(record["anchors"]) > 64:
            raise ContractFailure("INVALID_DRAFT")
        record["validated"] = True
        return {"state": "VALIDATED", "base_revision": record["base"],
                "applied": False}

    def commit(self, draft, rev, request_id, *, lose_ack=False):
        self.fence(rev)
        record = self.require(draft)
        if not record["validated"] or record["base"] != rev:
            raise ContractFailure("INVALID_DRAFT")
        if request_id in self.receipts:
            raise ContractFailure("REVISION_CONFLICT")
        self.history.append(self.anchors)
        self.redo_stack.clear()
        self.anchors = frozenset(record["anchors"])
        self.revision += 1
        self.lease = None
        receipt = {"state": "COMMITTED", "document_revision": self.revision,
                   "applied": False, "accepted_frame_generation": None}
        self.receipts[request_id] = receipt
        if lose_ack:
            return {"state": "UNKNOWN_OUTCOME", "error": "TIMEOUT_UNKNOWN",
                    "result": None}
        return receipt

    def abort(self, draft):
        self.require(draft)
        self.lease = None
        return {"state": "ABORTED", "applied": False}

    def undo(self, rev):
        self.fence(rev)
        if self.lease is not None:
            raise ContractFailure("BUSY")
        if not self.history:
            raise ContractFailure("INVALID_DRAFT")
        self.redo_stack.append(self.anchors)
        self.anchors = self.history.pop()
        self.revision += 1
        return {"state": "COMMITTED", "document_revision": self.revision,
                "applied": False}

    def redo(self, rev):
        self.fence(rev)
        if self.lease is not None:
            raise ContractFailure("BUSY")
        if not self.redo_stack:
            raise ContractFailure("REVISION_CONFLICT")
        self.history.append(self.anchors)
        self.anchors = self.redo_stack.pop()
        self.revision += 1
        return {"state": "COMMITTED", "document_revision": self.revision,
                "applied": False}

    def resolve(self, request_id):
        return self.receipts.get(request_id,
                                 {"state": "UNKNOWN", "applied": False})

    def apply(self, *, frame_accepted):
        if not frame_accepted:
            raise ContractFailure("COMMIT_FAILED")
        self.applied_revision = self.revision
        self.frame += 1
        self.solver_revision += 1
        return {"state": "APPLIED", "document_revision": self.revision,
                "accepted_frame_generation": self.frame,
                "solver_revision": self.solver_revision}


class ReferenceSemantics(unittest.TestCase):
    def expect(self, code, fn):
        with self.assertRaises(ContractFailure) as cm:
            fn()
        self.assertEqual(cm.exception.code, code)

    def stage(self, ed, *, changed=True):
        draft = ed.begin(ed.revision)["draft_id"]
        if changed:
            ed.toggle_anchor(draft, 2, True)
        ed.validate(draft)
        return draft

    def test_begin_validate_commit_not_frame_acceptance(self):
        ed = DraftReference()
        draft = self.stage(ed)
        res = ed.commit(draft, 7, "req-synthetic-1")
        self.assertEqual(res["document_revision"], 8)
        self.assertFalse(res["applied"])
        self.assertIsNone(res["accepted_frame_generation"])
        self.assertEqual(ed.applied_revision, 7)
        self.assertEqual(ed.anchors, frozenset((0, 1, 2)))

    def test_unvalidated_and_unknown_drafts_deny_without_effect(self):
        ed = DraftReference()
        self.expect("INVALID_DRAFT", lambda: ed.commit("foreign", 7, "req-a"))
        draft = ed.begin(7)["draft_id"]
        self.expect("INVALID_DRAFT", lambda: ed.commit(draft, 7, "req-b"))
        self.assertEqual(ed.revision, 7)
        self.assertEqual(ed.anchors, frozenset((0, 1)))

    def test_abort_rolls_back_and_frees_lease(self):
        ed = DraftReference()
        draft = ed.begin(7)["draft_id"]
        ed.toggle_anchor(draft, 2, True)
        self.assertEqual(ed.abort(draft)["state"], "ABORTED")
        self.expect("INVALID_DRAFT", lambda: ed.commit(draft, 7, "req-a"))
        self.assertEqual(ed.anchors, frozenset((0, 1)))
        self.assertEqual(ed.begin(7)["state"], "STAGED")

    def test_two_editor_owners_use_single_lease(self):
        ed = DraftReference()
        draft = ed.begin(7, owner="Agent")["draft_id"]
        self.expect("BUSY", lambda: ed.begin(7, owner="UI"))
        ed.abort(draft)
        self.assertEqual(ed.begin(7, owner="UI")["state"], "STAGED")

    def test_cas_stale_revision_model_and_session(self):
        ed = DraftReference()
        self.expect("REVISION_CONFLICT", lambda: ed.begin(6))
        self.expect("STALE_MODEL", lambda: ed.begin(7, generation=54))
        self.expect("STALE_SESSION",
                    lambda: ed.fence(7, session="session-foreign"))
        self.assertIsNone(ed.lease)

    def test_undo_redo_use_new_revisions(self):
        ed = DraftReference()
        draft = self.stage(ed)
        ed.commit(draft, 7, "req-a")
        self.assertEqual(ed.undo(8)["document_revision"], 9)
        self.assertEqual(ed.anchors, frozenset((0, 1)))
        self.assertEqual(ed.redo(9)["document_revision"], 10)
        self.assertEqual(ed.anchors, frozenset((0, 1, 2)))
        self.expect("REVISION_CONFLICT", lambda: ed.redo(9))

    def test_aba_state_equal_does_not_reuse_revision(self):
        ed = DraftReference()
        draft = self.stage(ed)
        ed.commit(draft, 7, "req-a")
        ed.undo(8)
        self.assertEqual(ed.anchors, frozenset((0, 1)))
        self.assertEqual(ed.revision, 9)
        self.expect("REVISION_CONFLICT", lambda: ed.begin(7))

    def test_divergent_new_commit_invalidates_redo(self):
        ed = DraftReference()
        draft = self.stage(ed)
        ed.commit(draft, 7, "req-a")
        ed.undo(8)
        draft2 = self.stage(ed, changed=False)
        ed.commit(draft2, 9, "req-b")
        self.expect("REVISION_CONFLICT", lambda: ed.redo(10))

    def test_lost_ack_reconciles_without_duplicate_commit(self):
        ed = DraftReference()
        draft = self.stage(ed)
        result = ed.commit(draft, 7, "req-loss", lose_ack=True)
        self.assertEqual(result["state"], "UNKNOWN_OUTCOME")
        self.assertEqual(result["error"], "TIMEOUT_UNKNOWN")
        self.assertEqual(ed.resolve("req-loss")["document_revision"], 8)
        self.assertEqual(ed.revision, 8)
        self.expect("REVISION_CONFLICT",
                    lambda: ed.commit(draft, 7, "req-loss"))
        self.assertEqual(ed.revision, 8)

    def test_commit_vs_frame_rejection(self):
        ed = DraftReference()
        draft = self.stage(ed)
        ed.commit(draft, 7, "req-a")
        self.expect("COMMIT_FAILED",
                    lambda: ed.apply(frame_accepted=False))
        self.assertEqual(ed.applied_revision, 7)
        accepted = ed.apply(frame_accepted=True)
        self.assertEqual(accepted["state"], "APPLIED")
        self.assertEqual(accepted["document_revision"], 8)
        self.assertEqual(accepted["accepted_frame_generation"], 43)

    def test_invalid_vertex_has_no_live_effect(self):
        ed = DraftReference()
        draft = ed.begin(7)["draft_id"]
        self.expect("INVALID_DRAFT",
                    lambda: ed.toggle_anchor(draft, True, True))
        self.expect("INVALID_DRAFT",
                    lambda: ed.toggle_anchor(draft, -1, True))
        self.assertEqual(ed.anchors, frozenset((0, 1)))
        self.assertEqual(ed.revision, 7)


if __name__ == "__main__":
    unittest.main()
