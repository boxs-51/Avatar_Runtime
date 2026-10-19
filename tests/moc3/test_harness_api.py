"""HTTP boundary checks for the private synthetic T0 coordinator."""

import http.client
import json
from pathlib import Path
import sys
import subprocess
import secrets
import shutil
import threading
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from moc3_test_harness import HarnessServer  # noqa: E402


class HarnessApiTests(unittest.TestCase):
    def setUp(self):
        self.server = HarnessServer()
        # Unit tests exercise the synthetic control contract independently of
        # optional local GPU builds. Actual F2 pixels have a separate live run.
        self.server.coordinator.pixel_fixture_ready = False
        self.server.coordinator.physics_fixture_ready = False
        self.server.coordinator.core_fixture_ready = False
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)

    def call(self, method, path, body=None, token=True, raw=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port,
                                                timeout=3)
        headers = {}
        if token:
            headers["X-Moc3-Test-Token"] = self.server.token
        if body is not None:
            raw = json.dumps(body).encode("utf-8")
        if raw is not None:
            headers["Content-Type"] = "application/json"
        connection.request(method, path, body=raw, headers=headers)
        response = connection.getresponse()
        value = json.loads(response.read())
        status = response.status
        connection.close()
        return status, value

    def session(self, mode="deterministic"):
        status, value = self.call("POST", "/v1/sessions", {
            "request_id": "create-1", "fixture_alias": "synthetic_t0", "mode": mode})
        self.assertEqual(status, 201)
        return value["session_id"]

    def test_huge_integer_control_returns_typed_error(self):
        coordinator=self.server.coordinator
        coordinator.run={'run_id':'generated','state':'RUNNING','suite_id':'CORE-LIVE'}
        code,value=self.call('POST','/v1/runs/generated/controls',{
            'request_id':'huge-control','sequence':1,'drive':10**1000,
            'physics':True,'reset':False,'lease_ms':1000})
        self.assertEqual((code,value['error']),(400,'INVALID_DRIVE'))
        self.assertEqual(coordinator.control_sequence,0)
        self.assertTrue(coordinator.controls.empty())

    def test_token_and_no_private_backend_claim(self):
        self.assertEqual(self.call("GET", "/healthz", token=False)[0], 200)
        self.assertEqual(self.call("GET", "/v1/status", token=False)[0], 401)
        status, value = self.call("GET", "/v1/status")
        self.assertEqual(status, 200)
        self.assertEqual(value["backend"], "SYNTHETIC")
        self.assertEqual(value["capabilities"]["host_pixels"],
                         "HOLD_NO_HOST_RENDERER")

    def test_idempotent_step_and_invalid_input_preserve_state(self):
        session = self.session()
        path = f"/v1/sessions/{session}/steps"
        step = {"request_id": "step-1", "delta_ns": 50_000_000,
                "parameters": {}}
        status, first = self.call("POST", path, step)
        self.assertEqual(status, 200)
        self.assertEqual(first["synthetic_tick"], 1)
        self.assertIsNone(first["frame_generation"])
        self.assertEqual(first["verdict"], "UNVERIFIED")
        self.assertEqual(self.call("POST", path, step), (status, first))
        self.assertEqual(self.call("POST", path, {**step, "delta_ns": 1})[1]["error"],
                         "REQUEST_ID_CONFLICT")
        self.assertEqual(self.call("POST", path, {"request_id": "bad-1",
                                                  "delta_ns": -1,
                                                  "parameters": {}})[1]["error"],
                         "INVALID_CLOCK_DELTA")
        self.assertEqual(self.call("POST", path, {"request_id": "bad-2",
                                                  "delta_ns": 1,
                                                  "parameters": {"X": 1}})[1]["error"],
                         "MODEL_PARAMETERS_UNAVAILABLE")
        self.assertEqual(self.call("GET", "/v1/status")[1]["session"]["synthetic_tick"], 1)
        self.assertEqual(self.call("GET", f"/v1/sessions/{session}/frames/latest"),
                         (409, {"schema_version": 1,
                                "error": "HOLD_NO_HOST_RENDERER"}))

    def test_run_event_order_and_unimplemented_suites_hold(self):
        session = self.session()
        status, queued = self.call("POST", f"/v1/sessions/{session}/runs",
                                   {"request_id": "run-1", "suite_id": "T0-SYNTHETIC"})
        self.assertEqual(status, 202)
        run_id = queued["run_id"]
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            result = self.call("GET", f"/v1/runs/{run_id}")[1]
            if result["state"] == "COMPLETE":
                break
            time.sleep(0.02)
        self.assertEqual(result["state"], "COMPLETE")
        self.assertEqual(result["verdict"], "PASS")
        self.assertEqual(result["proof_tier"], "synthetic_harness")
        self.assertIsNone(result["frame_generation"])
        events = self.call("GET", f"/v1/runs/{run_id}/events?after=0")[1]
        sequences = [item["seq"] for item in events["events"]]
        self.assertEqual(sequences, sorted(set(sequences)))
        self.assertFalse(events["cursor_gap"])
        self.assertEqual(events["events"][-1]["kind"], "RUN_COMPLETE")
        self.assertEqual(self.call("GET", f"/v1/runs/{run_id}/events?after={events['next_seq']}")[1]["events"], [])
        status, held = self.call("POST", f"/v1/sessions/{session}/runs",
                                 {"request_id": "run-2", "suite_id": "PHYS-LINK"})
        self.assertEqual(status, 202)
        self.assertEqual(held["verdict"], "HOLD")
        self.assertEqual(held["reason"], "HOLD_F3_NOT_CONNECTED")
        self.assertEqual(self.call("GET", f"/v1/runs/{run_id}")[1]["verdict"],
                         "PASS")
        old_events = self.call("GET", f"/v1/runs/{run_id}/events?after=0")[1]["events"]
        self.assertTrue(all(event.get("run_id") == run_id for event in old_events))

    def test_cancel_and_cursor_gap(self):
        session = self.session()
        run_id = self.call("POST", f"/v1/sessions/{session}/runs",
                           {"request_id": "run-cancel", "suite_id": "T0-SYNTHETIC"})[1]["run_id"]
        self.call("POST", f"/v1/runs/{run_id}/cancel", {"request_id": "cancel-1"})
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            result = self.call("GET", f"/v1/runs/{run_id}")[1]
            if result["state"] == "COMPLETE":
                break
            time.sleep(0.01)
        self.assertEqual(result["verdict"], "CANCELLED")
        for index in range(70):
            status, _ = self.call("POST", f"/v1/sessions/{session}/steps",
                                  {"request_id": f"step-{index}",
                                   "delta_ns": 1, "parameters": {}})
            self.assertEqual(status, 200)
        events = self.call("GET", f"/v1/runs/{run_id}/events?after=0")[1]
        self.assertTrue(events["cursor_gap"])

    def test_agent_can_run_f1_cpu_preflight_without_claiming_evaluator(self):
        session = self.session()
        status, queued = self.call("POST", f"/v1/sessions/{session}/runs",
                                   {"request_id": "f1-preflight",
                                    "suite_id": "F1-CPU-PREFLIGHT"})
        self.assertEqual(status, 202)
        run_id = queued["run_id"]
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            result = self.call("GET", f"/v1/runs/{run_id}")[1]
            if result["state"] == "COMPLETE":
                break
            time.sleep(0.05)
        self.assertEqual(result["verdict"], "PASS")
        self.assertEqual(result["proof_tier"], "cpu_preflight_fixture")
        self.assertEqual(result["reason"], "SYNTHETIC_MANIFEST_AND_PNG_ONLY")
        self.assertIsNone(result["frame_generation"])
        status, held = self.call("POST", f"/v1/sessions/{session}/runs",
                                 {"request_id": "f1-full", "suite_id": "ADMIT"})
        self.assertEqual(status, 202)
        self.assertEqual(held["verdict"], "HOLD")

    def test_private_replay_unavailable_is_explicit_hold(self):
        self.server.coordinator.private_fixture_ready = False
        session = self.session()
        status, held = self.call("POST", f"/v1/sessions/{session}/runs",
                                 {"request_id": "f1-private-unavailable",
                                  "suite_id": "F1-LOCAL-REPLAY"})
        self.assertEqual(status, 202)
        self.assertEqual(held["verdict"], "HOLD")
        self.assertEqual(held["reason"], "HOLD_NO_LOCAL_FIXTURE_OR_BRIDGE")
        self.assertIsNone(held["frame_generation"])

    def test_optimized_private_verifier_fails_before_model_work(self):
        result = subprocess.run([sys.executable, "-O", "scripts/moc3_local_replay.py"],
                                cwd=ROOT, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn("F1_PRIVATE_LOCAL_REPLAY_FAIL", result.stdout)
        self.assertNotIn("F1_STAGE", result.stdout)

    def test_host_frame_metadata_binary_auth_and_invalidation_wire_only(self):
        session = self.session()
        run_id = secrets.token_hex(12)
        root = ROOT / "build" / "moc3-f2-local" / run_id
        (root / "warp").mkdir(parents=True)
        def cleanup():
            if not root.resolve().is_relative_to((ROOT / "build" / "moc3-f2-local").resolve()):
                raise RuntimeError("invalid test cleanup")
            shutil.rmtree(root)
        self.addCleanup(cleanup)
        ppm = b"P6\n512 512\n255\n" + bytes(512*512*3)
        rgba = bytes(512*512*4)
        (root / "warp" / "frame-1.ppm").write_bytes(ppm)
        (root / "warp" / "frame-1.rgba").write_bytes(rgba)
        meta = {"width":512,"height":512,"frame_generation":1,"snapshot_generation":2,
                "target_epoch":1,"device_generation":101,"backend":"D3D11_WARP",
                "image":"warp/frame-1.ppm"}
        (root / "frame-1.json").write_text(json.dumps(meta),encoding="utf-8")
        coordinator = self.server.coordinator
        with coordinator.lock:
            coordinator.run = {"run_id":run_id,"state":"COMPLETE"}
        coordinator._publish_frame(run_id,1)
        route = f"/v1/sessions/{session}/frames/latest"
        status, observed = self.call("GET",route)
        self.assertEqual(status,200)
        self.assertEqual(observed["frame_generation"],1)
        self.assertNotIn("image",observed)
        self.assertEqual(self.call("GET",route+"/rgba",token=False)[0],401)
        for suffix,expected in (("image",ppm),("rgba",rgba)):
            connection = http.client.HTTPConnection("127.0.0.1",self.server.server_port,timeout=3)
            connection.request("GET",route+"/"+suffix,
                               headers={"X-Moc3-Test-Token":self.server.token})
            response = connection.getresponse()
            self.assertEqual(response.status,200)
            self.assertEqual(response.getheader("X-Frame-Generation"),"1")
            self.assertEqual(response.getheader("X-Run-Id"),run_id)
            self.assertEqual(response.getheader("X-Session-Id"),session)
            self.assertEqual(response.getheader("X-Snapshot-Generation"),"2")
            self.assertEqual(response.getheader("X-Device-Generation"),"101")
            self.assertEqual(response.read(),expected)
            connection.close()
        status,_ = self.call("POST",f"/v1/sessions/{session}/runs",
                             {"request_id":"replace-wire-run","suite_id":"T0-SYNTHETIC"})
        self.assertEqual(status,202)
        self.assertEqual(self.call("GET",route)[0],409)
        # Counters may restart; binary identity must still distinguish the run.
        replacement = secrets.token_hex(12)
        replacement_root = root.parent / replacement
        shutil.copytree(root,replacement_root)
        self.addCleanup(lambda: shutil.rmtree(replacement_root))
        with coordinator.lock:
            coordinator.run = {"run_id":replacement,"state":"COMPLETE"}
        coordinator._publish_frame(replacement,1)
        connection = http.client.HTTPConnection("127.0.0.1",self.server.server_port,timeout=3)
        connection.request("GET",route+"/rgba",headers={"X-Moc3-Test-Token":self.server.token})
        response = connection.getresponse()
        self.assertEqual(response.getheader("X-Frame-Generation"),"1")
        self.assertEqual(response.getheader("X-Target-Epoch"),"1")
        self.assertEqual(response.getheader("X-Run-Id"),replacement)
        self.assertNotEqual(response.getheader("X-Run-Id"),observed["run_id"])
        self.assertEqual(response.read(),rgba)
        connection.close()

    def test_core_stream_atomic_identity_and_binary_header_wire_only(self):
        from moc3_frame_packet import encode
        session=self.session();coordinator=self.server.coordinator;rid=secrets.token_hex(12)
        with coordinator.lock:coordinator.run={"run_id":rid,"suite_id":"CORE-INTEGRATION","state":"RUNNING"}
        meta={"width":512,"height":512,"frame_generation":1,"snapshot_generation":2,
              "target_epoch":1,"device_generation":101,"core_generation":1,
              "backend":"D3D11_WARP","behavior":"CORE_RELEASE"}
        rgba=bytes(512*512*4);ppm=b"P6\n512 512\n255\n"+bytes(512*512*3)
        packet=encode(meta,rgba,ppm);coordinator._publish_stream(rid,packet)
        before=coordinator.latest_frame(session,clean=True)
        with self.assertRaises(ValueError):coordinator._publish_stream(rid,packet)
        with self.assertRaises(ValueError):coordinator._publish_stream(rid,"F4_PACKET invalid")
        self.assertEqual(coordinator.latest_frame(session,clean=True),before)
        connection=http.client.HTTPConnection("127.0.0.1",self.server.server_port,timeout=3)
        connection.request("GET",f"/v1/sessions/{session}/frames/latest/rgba",headers={"X-Moc3-Test-Token":self.server.token})
        response=connection.getresponse()
        self.assertEqual(response.status,200);self.assertEqual(response.getheader("X-Core-Generation"),"1")
        self.assertEqual(response.getheader("X-Run-Id"),rid);self.assertEqual(response.read(),rgba);connection.close()

    def test_malformed_json_and_unknown_fixture(self):
        status, value = self.call("POST", "/v1/sessions",
                                  raw=b'{"request_id":"a","request_id":"b"}')
        self.assertEqual((status, value["error"]), (400, "INVALID_JSON"))
        status, value = self.call("POST", "/v1/sessions", {
            "request_id": "bad-alias", "fixture_alias": "../private",
            "mode": "deterministic"})
        self.assertEqual((status, value["error"]), (422, "FIXTURE_UNAVAILABLE"))


if __name__ == "__main__":
    unittest.main()
