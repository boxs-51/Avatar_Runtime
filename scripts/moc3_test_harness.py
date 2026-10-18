"""Private MOC3 test coordinator: synthetic T0 and bounded F1 diagnostics."""

from __future__ import annotations

import argparse
import csv
from collections import deque
from collections.abc import Callable
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path, PurePosixPath
import re
import queue
import secrets
import stat
import subprocess
import sys
import threading
import time
from urllib.parse import parse_qs, urlsplit

from moc3_local_replay import eligible as local_replay_eligible
from moc3_local_pixels import eligible as local_pixels_eligible
from moc3_process_tree import ProcessTree
from moc3_package_admission import AdmissionError, _read_contained


ROOT = Path(__file__).resolve().parents[1]
MAX_BODY = 16 * 1024
EVENT_LIMIT = 64
IDEMPOTENCY_LIMIT = 128
SCHEMA_VERSION = 1


class ApiError(Exception):
    def __init__(self, status: int, code: str):
        super().__init__(code)
        self.status = status
        self.code = code


def _unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def _reject_constant(_value: str) -> None:
    raise ValueError("nonfinite JSON value")


def _harden_local_path(path: Path) -> bool:
    """Remove inherited ACLs and grant only the launching Windows account."""
    if os.name != "nt":
        os.chmod(path, 0o600)
        return True
    try:
        identity = subprocess.run(["whoami", "/user", "/fo", "csv", "/nh"],
                                  capture_output=True, text=True, check=True, timeout=5)
        sid = next(csv.reader(identity.stdout.splitlines()))[1]
        if not sid.startswith("S-1-"):
            return False
        restricted = subprocess.run(["icacls", str(path), "/inheritance:r",
                                     "/grant:r", f"*{sid}:F"],
                                    capture_output=True, text=True, timeout=5)
        return restricted.returncode == 0
    except (OSError, ValueError, IndexError, StopIteration,
            subprocess.SubprocessError):
        return False


class Coordinator:
    """One bounded synthetic session; all UI and HTTP reads share this state."""

    def __init__(self) -> None:
        self.lock = threading.RLock()
        self.session: dict | None = None
        self.run: dict | None = None
        self.runs: dict[str, dict] = {}
        self.run_start_seq: dict[str, int] = {}
        self.events: deque[dict] = deque(maxlen=EVENT_LIMIT)
        self.event_seq = 0
        self.idempotency: dict[str, tuple[str, str, int, dict]] = {}
        self.stop_run = threading.Event()
        self.closed = False
        self.private_fixture_ready = local_replay_eligible()
        self.pixel_fixture_ready = local_pixels_eligible()
        self.frame_meta: dict | None = None
        self.frame_image: bytes | None = None
        self.frame_rgba: bytes | None = None
        self.ui_frame: dict | None = None
        self.ui_visible = False
        try:
            revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                                      capture_output=True, text=True, timeout=2,
                                      check=True).stdout.strip()
            dirty = subprocess.run(["git", "status", "--porcelain"], cwd=ROOT,
                                   capture_output=True, text=True, timeout=2,
                                   check=True).stdout != ""
            self.source_revision = revision
            self.worktree_dirty = dirty
        except (OSError, subprocess.SubprocessError):
            self.source_revision = None
            self.worktree_dirty = None

    def _event(self, kind: str, **fields: object) -> int:
        self.event_seq += 1
        self.events.append({"seq": self.event_seq, "kind": kind, **fields})
        return self.event_seq

    def status(self) -> dict:
        with self.lock:
            return {
                "schema_version": SCHEMA_VERSION,
                "phase": "T0_F1_F2_LOCAL",
                "backend": self.frame_meta["backend"] if self.frame_meta else "SYNTHETIC",
                "source_revision": self.source_revision,
                "worktree_dirty": self.worktree_dirty,
                "session": dict(self.session) if self.session else None,
                "run": dict(self.run) if self.run else None,
                "capabilities": {
                    "admission": ("TRUSTED_LOCAL_RUN_SUITE" if self.private_fixture_ready
                                  else "HOLD_NO_LOCAL_FIXTURE_OR_BRIDGE"),
                    "persistent_model_session": "HOLD_PERSISTENT_MODEL_SESSION_NOT_CONNECTED",
                    "cpu_preflight_fixture": "AVAILABLE",
                    "private_fixture_replay": ("AVAILABLE_OPT_IN_PRIVATE" if self.private_fixture_ready
                                               else "HOLD_NO_LOCAL_FIXTURE_OR_BRIDGE"),
                    "host_pixels": ("AVAILABLE_F2_LOCAL_SUITE" if self.pixel_fixture_ready
                                    else "HOLD_NO_HOST_RENDERER"),
                    "linked_physics": "HOLD_F3_NOT_CONNECTED",
                    "physical_device_loss": "HOLD_NO_RESET_RUNNER",
                },
                "event_seq": self.event_seq,
                "ui_visible": self.ui_visible,
                "ui_frame": dict(self.ui_frame) if self.ui_frame else None,
            }

    def create_session(self, body: dict) -> tuple[int, dict]:
        if set(body) != {"request_id", "fixture_alias", "mode"}:
            raise ApiError(400, "INVALID_REQUEST")
        if body["fixture_alias"] != "synthetic_t0":
            raise ApiError(422, "FIXTURE_UNAVAILABLE")
        if body["mode"] not in ("deterministic", "live"):
            raise ApiError(400, "INVALID_MODE")
        with self.lock:
            if self.session is not None:
                raise ApiError(409, "SESSION_EXISTS")
            self.session = {
                "session_id": secrets.token_hex(12),
                "fixture_alias": "synthetic_t0",
                "mode": body["mode"],
                "simulation_ns": 0,
                "snapshot_generation": None,
                "frame_generation": None,
                "synthetic_tick": 0,
            }
            self._event("SESSION_CREATED", session_id=self.session["session_id"],
                        proof_tier="synthetic_harness")
            return 201, {"schema_version": SCHEMA_VERSION, **self.session,
                         "proof_tier": "synthetic_harness",
                         "host_pixels": "HOLD_NO_HOST_RENDERER"}

    def _session(self, session_id: str, mode: str | None = None) -> dict:
        if self.session is None or self.session["session_id"] != session_id:
            raise ApiError(404, "SESSION_NOT_FOUND")
        if mode and self.session["mode"] != mode:
            raise ApiError(409, "WRONG_MODE")
        return self.session

    def _tick(self, delta_ns: int, run_id: str | None = None) -> dict:
        assert self.session is not None
        self.session["simulation_ns"] += delta_ns
        self.session["synthetic_tick"] += 1
        self._event("SYNTHETIC_TICK", session_id=self.session["session_id"],
                    run_id=run_id,
                    synthetic_tick=self.session["synthetic_tick"],
                    simulation_ns=self.session["simulation_ns"])
        return {"schema_version": SCHEMA_VERSION,
                "session_id": self.session["session_id"],
                "simulation_ns": self.session["simulation_ns"],
                "synthetic_tick": self.session["synthetic_tick"],
                "snapshot_generation": None, "frame_generation": None,
                "proof_tier": "synthetic_harness",
                "verdict": "UNVERIFIED", "reason": "NO_MODEL_OR_HOST_FRAME"}

    def step(self, session_id: str, body: dict) -> tuple[int, dict]:
        if set(body) != {"request_id", "delta_ns", "parameters"}:
            raise ApiError(400, "INVALID_REQUEST")
        if body["parameters"] != {}:
            raise ApiError(422, "MODEL_PARAMETERS_UNAVAILABLE")
        delta = body["delta_ns"]
        if type(delta) is not int or not 1 <= delta <= 1_000_000_000:
            raise ApiError(400, "INVALID_CLOCK_DELTA")
        with self.lock:
            self._session(session_id, "deterministic")
            if self.run and self.run["state"] in ("QUEUED", "RUNNING"):
                raise ApiError(409, "CONTROL_BUSY")
            return 200, self._tick(delta)

    def start_run(self, session_id: str, body: dict) -> tuple[int, dict]:
        if set(body) != {"request_id", "suite_id"}:
            raise ApiError(400, "INVALID_REQUEST")
        suite = body["suite_id"]
        if not isinstance(suite, str):
            raise ApiError(400, "INVALID_SUITE")
        with self.lock:
            self._session(session_id)
            if self.run and self.run["state"] in ("QUEUED", "RUNNING"):
                raise ApiError(409, "RUN_BUSY")
            run_id = secrets.token_hex(12)
            self.frame_meta = None
            self.frame_image = None
            self.frame_rgba = None
            self.ui_frame = None
            self.session.update(frame_generation=None, snapshot_generation=None)
            self.run = {"run_id": run_id, "session_id": session_id,
                        "suite_id": suite, "state": "QUEUED",
                        "verdict": None, "reason": None,
                        "proof_tier": ("synthetic_harness" if suite == "T0-SYNTHETIC" else
                                       "cpu_preflight_fixture" if suite == "F1-CPU-PREFLIGHT" else
                                       "trusted_local_cpu" if suite == "F1-LOCAL-REPLAY" else
                                       "host_integrated_local" if suite == "PIXEL-2D" else None),
                        "completed_steps": 0,
                        "frame_generation": None, "target_epoch": None}
            if len(self.runs) >= 32:
                evicted = next(iter(self.runs))
                del self.runs[evicted]
                del self.run_start_seq[evicted]
            self.runs[run_id] = self.run
            self.run_start_seq[run_id] = self._event("RUN_QUEUED", run_id=run_id,
                                                       suite_id=suite)
            if suite == "F1-CPU-PREFLIGHT":
                self.stop_run.clear()
                threading.Thread(target=self._preflight_run, args=(run_id,), daemon=True).start()
            elif suite == "F1-LOCAL-REPLAY" and self.private_fixture_ready:
                self.stop_run.clear()
                threading.Thread(target=self._local_replay_run, args=(run_id,), daemon=True).start()
            elif suite == "PIXEL-2D" and self.pixel_fixture_ready:
                self.stop_run.clear()
                threading.Thread(target=self._local_replay_run, args=(run_id,"F2"), daemon=True).start()
            elif suite != "T0-SYNTHETIC":
                reasons = {"ADMIT": "HOLD_PERSISTENT_MODEL_SESSION_NOT_CONNECTED",
                           "POSE-REPLAY": "HOLD_PERSISTENT_MODEL_SESSION_NOT_CONNECTED",
                           "F1-LOCAL-REPLAY": "HOLD_NO_LOCAL_FIXTURE_OR_BRIDGE",
                           "PIXEL-2D": "HOLD_NO_HOST_RENDERER",
                           "PHYS-LINK": "HOLD_F3_NOT_CONNECTED",
                           "LIVE-CONTINUITY": "HOLD_NO_HOST_RENDERER",
                           "FAULT-LIFECYCLE": "HOLD_NO_HOST_RENDERER"}
                self.run.update(state="COMPLETE", verdict="HOLD",
                                reason=reasons.get(suite, "SUITE_UNAVAILABLE"))
                self._event("RUN_COMPLETE", run_id=run_id, verdict="HOLD",
                            reason=self.run["reason"])
            else:
                self.stop_run.clear()
                threading.Thread(target=self._synthetic_run, args=(run_id,), daemon=True).start()
            return 202, {"schema_version": SCHEMA_VERSION, **self.run}

    def _preflight_run(self, run_id: str) -> None:
        with self.lock:
            if self.run is None or self.run["run_id"] != run_id:
                return
            self.run["state"] = "RUNNING"
            self._event("RUN_STARTED", run_id=run_id)
        command = [sys.executable, "-m", "unittest", "discover", "-s",
                   "tests/moc3", "-p", "test_package_admission.py"]
        try:
            completed = subprocess.run(command, cwd=ROOT, capture_output=True,
                                       text=True, timeout=30, check=False)
            match = re.search(r"Ran (\d+) tests?", completed.stderr)
            passed = completed.returncode == 0 and match is not None and int(match[1]) >= 5
            verdict = "PASS" if passed else "FAIL"
            reason = "SYNTHETIC_MANIFEST_AND_PNG_ONLY" if passed else "CPU_PREFLIGHT_TEST_FAILED"
        except (OSError, subprocess.SubprocessError):
            verdict, reason = "FAIL", "CPU_PREFLIGHT_RUNNER_ERROR"
        with self.lock:
            if self.run and self.run["run_id"] == run_id:
                if self.stop_run.is_set():
                    verdict, reason = "CANCELLED", "CANCEL_REQUESTED"
                self.run.update(state="COMPLETE", verdict=verdict, reason=reason)
                self._event("RUN_COMPLETE", run_id=run_id, verdict=verdict,
                            reason=reason)

    def _publish_frame(self, run_id: str, number: int) -> None:
        root = ROOT / "build" / "moc3-f2-local" / run_id
        attrs = getattr(root.lstat(), "st_file_attributes", 0)
        if root.is_symlink() or attrs & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0):
            raise ValueError("reparse frame directory")
        root = root.resolve(strict=True)
        if not root.is_relative_to((ROOT / "build" / "moc3-f2-local").resolve()):
            raise ValueError("frame directory escape")
        info = json.loads(_read_contained(root, PurePosixPath(f"frame-{number}.json"), 16384),
                          object_pairs_hook=_unique_object, parse_constant=_reject_constant)
        if (info["frame_generation"] != number or info["target_epoch"] != number or
                info["width"] != 512 or info["height"] != 512 or
                info["backend"] != "D3D11_WARP" or
                info["image"] != f"warp/frame-{number}.ppm"):
            raise ValueError("invalid frame publication")
        image = _read_contained(root, PurePosixPath(info["image"]), 1024*1024)
        if not image.startswith(b"P6\n512 512\n255\n") or len(image) != 512*512*3+15:
            raise ValueError("invalid frame image size")
        rgba = _read_contained(root, PurePosixPath(f"warp/frame-{number}.rgba"), 512*512*4)
        if len(rgba) != 512*512*4:
            raise ValueError("invalid clean frame size")
        with self.lock:
            if self.run and self.run["run_id"] == run_id and not self.closed:
                meta = {key: info[key] for key in ("width", "height", "frame_generation",
                        "snapshot_generation", "target_epoch", "device_generation", "backend")}
                meta.update(schema_version=SCHEMA_VERSION, session_id=self.session["session_id"],
                            run_id=run_id, proof_tier="host_integrated_local",
                            clean_format="RGBA8_PREMULTIPLIED_UNORM_RAW_SAMPLES",
                            presenter="PPM_CHECKER_AFTER_READBACK")
                self.frame_meta = meta
                self.frame_image = image
                self.frame_rgba = rgba
                self.session.update(frame_generation=number, snapshot_generation=info["snapshot_generation"])
                self.run.update(frame_generation=number, target_epoch=info["target_epoch"])
                self._event("HOST_FRAME_COMMITTED", run_id=run_id, frame_generation=number,
                            target_epoch=info["target_epoch"])

    def latest_frame(self, session_id: str, *, clean: bool = False) -> tuple[dict, bytes]:
        with self.lock:
            self._session(session_id)
            if self.frame_meta is None or self.frame_image is None or self.frame_rgba is None:
                raise ApiError(409, "HOLD_NO_COMPLETED_HOST_FRAME" if self.pixel_fixture_ready
                               else "HOLD_NO_HOST_RENDERER")
            return dict(self.frame_meta), self.frame_rgba if clean else self.frame_image

    def _local_replay_run(self, run_id: str, phase: str = "F1") -> None:
        with self.lock:
            if self.run is None or self.run["run_id"] != run_id:
                return
            self.run["state"] = "RUNNING"
            self._event("RUN_STARTED", run_id=run_id)
        process = None
        process_tree = None
        verdict, reason = "FAIL", "PRIVATE_CPU_REPLAY_FAILED" if phase=="F1" else "LOCAL_F2_PIXELS_FAILED"
        messages: queue.Queue[str] = queue.Queue(maxsize=16)
        output_pass = False
        output_thread = None
        def collect_output():
            while True:
                line = process.stdout.readline(128)
                if not line:
                    return
                messages.put(line.strip())
        def consume_output():
            nonlocal output_pass
            while True:
                try:
                    line = messages.get_nowait()
                except queue.Empty:
                    return
                if line == ("F1_PRIVATE_LOCAL_REPLAY_PASS" if phase=="F1" else "F2_LOCAL_HOST_PIXELS_PASS"):
                    output_pass = True
                elif line in {"F1_STAGE PACKAGE", "F1_STAGE EVALUATOR", "F1_STAGE NEGATIVE",
                              "F1_STAGE MEMORY", "F1_STAGE COMPLETE", "F2_STAGE WARP",
                              "F2_STAGE HARDWARE", "F2_STAGE PACKAGE", "F2_STAGE RENDER", "F2_STAGE COMPLETE"}:
                    with self.lock:
                        if self.run and self.run["run_id"] == run_id:
                            self.run["stage"] = line.split()[1]
                            self._event(f"{phase}_STAGE", run_id=run_id, stage=self.run["stage"])
                elif phase=="F2" and re.fullmatch(r"F2_FRAME [1-9][0-9]?",line):
                    self._publish_frame(run_id,int(line.split()[1]))
        try:
            command = [sys.executable, str(ROOT / "scripts" /
                                          ("moc3_local_replay.py" if phase=="F1" else "moc3_local_pixels.py"))]
            if phase=="F2":
                command += ["--run-id",run_id]
                process_tree = ProcessTree()
            process = subprocess.Popen(command,
                                       cwd=ROOT, stdout=subprocess.PIPE,
                                       stderr=subprocess.DEVNULL, text=True,
                                       env=process_tree.environment() if process_tree else None)
            if process_tree:
                process_tree.attach_and_release(process.pid)
            output_thread = threading.Thread(target=collect_output, daemon=True)
            output_thread.start()
            deadline = time.monotonic() + 180
            while True:
                consume_output()
                if self.stop_run.is_set() or self.closed:
                    verdict, reason = "CANCELLED", "CANCEL_REQUESTED"
                    process_tree.terminate() if process_tree else process.terminate()
                    break
                if time.monotonic() >= deadline:
                    reason = "PRIVATE_CPU_REPLAY_TIMEOUT" if phase=="F1" else "LOCAL_F2_TIMEOUT"
                    process_tree.terminate() if process_tree else process.terminate()
                    break
                if process.poll() is not None:
                    output_thread.join(timeout=1)
                    consume_output()
                    if process.returncode == 0 and output_pass:
                        verdict, reason = "PASS", ("TRUSTED_LOCAL_F1_CPU_VERIFIED" if phase=="F1"
                                                   else "TRUSTED_LOCAL_F2_HOST_PIXELS_VERIFIED")
                    break
                self.stop_run.wait(0.1)
            process.wait(timeout=5)
            if process_tree and not process_tree.wait_empty():
                verdict, reason = "FAIL", "PROCESS_TREE_NOT_EMPTY_AFTER_RUN"
                process_tree.terminate()
        except (AdmissionError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError):
            reason = "PRIVATE_CPU_REPLAY_RUNNER_ERROR" if phase=="F1" else "LOCAL_F2_RUNNER_ERROR"
            if process is not None and process.poll() is None:
                if process_tree:
                    process_tree.terminate()
                process.kill()
                process.wait(timeout=5)
        finally:
            if process_tree is not None:
                if not process_tree.wait_empty():
                    process_tree.terminate()
                    if not process_tree.wait_empty():
                        verdict, reason = "FAIL", "PROCESS_TREE_CLEANUP_FAILED"
                process_tree.close()
            if output_thread is not None:
                output_thread.join(timeout=1)
            if process is not None:
                process.stdout.close()
        with self.lock:
            if self.run and self.run["run_id"] == run_id:
                self.run.update(state="COMPLETE", verdict=verdict, reason=reason)
                if phase=="F2":
                    self.run["process_tree_empty"] = reason != "PROCESS_TREE_CLEANUP_FAILED"
                self._event("RUN_COMPLETE", run_id=run_id, verdict=verdict,
                            reason=reason)

    def _synthetic_run(self, run_id: str) -> None:
        with self.lock:
            if self.run is None or self.run["run_id"] != run_id:
                return
            self.run["state"] = "RUNNING"
            self._event("RUN_STARTED", run_id=run_id)
        for _ in range(20):
            if self.stop_run.wait(0.05):
                with self.lock:
                    if self.run and self.run["run_id"] == run_id:
                        self.run.update(state="COMPLETE", verdict="CANCELLED",
                                        reason="CANCEL_REQUESTED")
                        self._event("RUN_COMPLETE", run_id=run_id,
                                    verdict="CANCELLED", reason="CANCEL_REQUESTED")
                return
            with self.lock:
                if self.run is None or self.run["run_id"] != run_id or self.closed:
                    return
                self._tick(50_000_000, run_id=run_id)
                self.run["completed_steps"] += 1
        with self.lock:
            if self.run and self.run["run_id"] == run_id:
                cancelled = self.stop_run.is_set()
                verdict = "CANCELLED" if cancelled else "PASS"
                reason = ("CANCEL_REQUESTED" if cancelled
                          else "SYNTHETIC_EVENT_SEQUENCE_ONLY")
                self.run.update(state="COMPLETE", verdict=verdict, reason=reason)
                self._event("RUN_COMPLETE", run_id=run_id, verdict=verdict,
                            reason=self.run["reason"])

    def get_run(self, run_id: str) -> dict:
        with self.lock:
            run = self.runs.get(run_id)
            if run is None:
                raise ApiError(404, "RUN_NOT_FOUND")
            return {"schema_version": SCHEMA_VERSION, **run}

    def cancel(self, run_id: str) -> tuple[int, dict]:
        with self.lock:
            run = self.runs.get(run_id)
            if run is None:
                raise ApiError(404, "RUN_NOT_FOUND")
            if run["state"] in ("QUEUED", "RUNNING"):
                self.stop_run.set()
            return 202, {"schema_version": SCHEMA_VERSION, **run}

    def get_events(self, run_id: str, after: int) -> dict:
        with self.lock:
            if run_id not in self.runs:
                raise ApiError(404, "RUN_NOT_FOUND")
            oldest = self.events[0]["seq"] if self.events else self.event_seq + 1
            lower_bound = max(after, self.run_start_seq[run_id] - 1)
            return {"schema_version": SCHEMA_VERSION, "run_id": run_id,
                    "next_seq": self.event_seq,
                    "cursor_gap": lower_bound < oldest - 1,
                    "events": [dict(event) for event in self.events
                               if event["seq"] > after and event.get("run_id") == run_id]}

    def close(self) -> None:
        with self.lock:
            self.closed = True
            self.stop_run.set()


class HarnessServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, coordinator: Coordinator | None = None):
        self.coordinator = coordinator or Coordinator()
        self.token = secrets.token_urlsafe(32)
        super().__init__(("127.0.0.1", 0), HarnessHandler)

    def server_close(self) -> None:
        self.coordinator.close()
        super().server_close()


class HarnessHandler(BaseHTTPRequestHandler):
    server: HarnessServer

    def setup(self) -> None:
        super().setup()
        self.connection.settimeout(5)

    def log_message(self, _format: str, *_args: object) -> None:
        # Never log paths, tokens, request bodies or private query strings.
        pass

    def _send(self, status: int, value: dict) -> None:
        data = json.dumps(value, allow_nan=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _send_image(self, data: bytes, meta: dict, *, clean: bool = False) -> None:
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream" if clean else "image/x-portable-pixmap")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Frame-Generation", str(meta["frame_generation"]))
        self.send_header("X-Target-Epoch", str(meta["target_epoch"]))
        self.send_header("X-Run-Id", meta["run_id"])
        self.send_header("X-Session-Id", meta["session_id"])
        self.send_header("X-Snapshot-Generation", str(meta["snapshot_generation"]))
        self.send_header("X-Device-Generation", str(meta["device_generation"]))
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _auth(self) -> None:
        if not secrets.compare_digest(self.headers.get("X-Moc3-Test-Token", ""),
                                      self.server.token):
            raise ApiError(401, "UNAUTHORIZED")

    def _body(self) -> dict:
        if self.headers.get("Content-Type") != "application/json":
            raise ApiError(415, "JSON_REQUIRED")
        try:
            length = int(self.headers.get("Content-Length", ""))
        except ValueError as error:
            raise ApiError(411, "LENGTH_REQUIRED") from error
        if not 1 <= length <= MAX_BODY:
            raise ApiError(413, "BODY_TOO_LARGE")
        try:
            body = json.loads(self.rfile.read(length), object_pairs_hook=_unique_object,
                              parse_constant=_reject_constant)
        except (ValueError, UnicodeDecodeError, RecursionError) as error:
            raise ApiError(400, "INVALID_JSON") from error
        if not isinstance(body, dict) or not isinstance(body.get("request_id"), str) or \
                not 1 <= len(body["request_id"]) <= 80:
            raise ApiError(400, "INVALID_REQUEST_ID")
        return body

    def do_GET(self) -> None:
        try:
            path = urlsplit(self.path)
            if path.path == "/healthz":
                self._send(200, {"ready": True, "phase": "T0_F1_F2_LOCAL"})
                return
            self._auth()
            coordinator = self.server.coordinator
            if path.path == "/v1/status":
                self._send(200, coordinator.status())
                return
            parts = path.path.strip("/").split("/")
            if len(parts) == 3 and parts[:2] == ["v1", "runs"]:
                self._send(200, coordinator.get_run(parts[2]))
                return
            if len(parts) == 4 and parts[:2] == ["v1", "runs"] and parts[3] == "events":
                query = parse_qs(path.query, strict_parsing=True)
                if set(query) != {"after"} or len(query["after"]) != 1:
                    raise ApiError(400, "INVALID_CURSOR")
                try:
                    after = int(query["after"][0])
                except ValueError as error:
                    raise ApiError(400, "INVALID_CURSOR") from error
                if after < 0:
                    raise ApiError(400, "INVALID_CURSOR")
                self._send(200, coordinator.get_events(parts[2], after))
                return
            if len(parts) == 5 and parts[:2] == ["v1", "sessions"] and parts[3:] == ["frames", "latest"]:
                meta, _image = coordinator.latest_frame(parts[2])
                self._send(200, meta)
                return
            if len(parts) == 6 and parts[:2] == ["v1", "sessions"] and parts[3:] == ["frames", "latest", "image"]:
                meta, image = coordinator.latest_frame(parts[2])
                self._send_image(image, meta)
                return
            if len(parts) == 6 and parts[:2] == ["v1", "sessions"] and parts[3:] == ["frames", "latest", "rgba"]:
                meta, rgba = coordinator.latest_frame(parts[2], clean=True)
                self._send_image(rgba, meta, clean=True)
                return
            raise ApiError(404, "ROUTE_NOT_FOUND")
        except ApiError as error:
            self._send(error.status, {"schema_version": SCHEMA_VERSION,
                                      "error": error.code})
        except ValueError:
            self._send(400, {"schema_version": SCHEMA_VERSION, "error": "INVALID_QUERY"})

    def do_POST(self) -> None:
        try:
            self._auth()
            body = self._body()
            coordinator = self.server.coordinator
            path = urlsplit(self.path).path
            if urlsplit(self.path).query:
                raise ApiError(400, "QUERY_NOT_ALLOWED")
            identity = body["request_id"]
            fingerprint = json.dumps(body, sort_keys=True, separators=(",", ":"))
            with coordinator.lock:
                prior = coordinator.idempotency.get(identity)
                if prior:
                    old_path, old_body, status, value = prior
                    if old_path != path or old_body != fingerprint:
                        raise ApiError(409, "REQUEST_ID_CONFLICT")
                else:
                    parts = path.strip("/").split("/")
                    if path == "/v1/sessions":
                        status, value = coordinator.create_session(body)
                    elif len(parts) == 4 and parts[:2] == ["v1", "sessions"] and parts[3] == "steps":
                        status, value = coordinator.step(parts[2], body)
                    elif len(parts) == 4 and parts[:2] == ["v1", "sessions"] and parts[3] == "runs":
                        status, value = coordinator.start_run(parts[2], body)
                    elif len(parts) == 4 and parts[:2] == ["v1", "runs"] and parts[3] == "cancel":
                        if set(body) != {"request_id"}:
                            raise ApiError(400, "INVALID_REQUEST")
                        status, value = coordinator.cancel(parts[2])
                    elif len(parts) == 4 and parts[:2] == ["v1", "sessions"] and parts[3] == "actions":
                        coordinator._session(parts[2], "live")
                        raise ApiError(409, "HOLD_NO_LIVE_INPUT_BACKEND")
                    else:
                        raise ApiError(404, "ROUTE_NOT_FOUND")
                    if len(coordinator.idempotency) >= IDEMPOTENCY_LIMIT:
                        coordinator.idempotency.pop(next(iter(coordinator.idempotency)))
                    coordinator.idempotency[identity] = (path, fingerprint, status, dict(value))
            self._send(status, value)
        except ApiError as error:
            self._send(error.status, {"schema_version": SCHEMA_VERSION,
                                      "error": error.code})


def show_window(coordinator: Coordinator, shutdown: Callable[[], None],
                duration_seconds: float | None = None) -> None:
    import tkinter as tk
    from tkinter import ttk

    window = tk.Tk()
    window.title("MOC3 Test Harness - F1 CPU + F2 Host Pixels")
    window.geometry("950x760")
    heading = ttk.Label(window, text="MOC3 TEST HARNESS - F1 CPU + F2 HOST PIXELS",
                        font=("Segoe UI", 16, "bold"))
    heading.pack(padx=16, pady=(18, 8), anchor="w")
    ttk.Label(window, text="Run F2 pixels: actual host target   |   Linked Physics3: HOLD",
              font=("Segoe UI", 10)).pack(padx=16, anchor="w")
    controls = ttk.Frame(window)
    controls.pack(padx=16, pady=(10, 0), anchor="w")
    operator_message = tk.StringVar(value="Synthetic controls only")

    def operate(action: str) -> None:
        try:
            with coordinator.lock:
                if action == "create":
                    coordinator.create_session({"request_id": "ui-create",
                                                "fixture_alias": "synthetic_t0",
                                                "mode": "deterministic"})
                elif action == "step":
                    if coordinator.session is None:
                        raise ApiError(409, "CREATE_SESSION_FIRST")
                    coordinator.step(coordinator.session["session_id"],
                                     {"request_id": "ui-step", "delta_ns": 50_000_000,
                                      "parameters": {}})
                elif action == "run":
                    if coordinator.session is None:
                        raise ApiError(409, "CREATE_SESSION_FIRST")
                    coordinator.start_run(coordinator.session["session_id"],
                                          {"request_id": "ui-run",
                                           "suite_id": "T0-SYNTHETIC"})
                elif action == "f1":
                    if coordinator.session is None:
                        raise ApiError(409, "CREATE_SESSION_FIRST")
                    coordinator.start_run(coordinator.session["session_id"],
                                          {"request_id": "ui-f1",
                                           "suite_id": "F1-LOCAL-REPLAY"})
                elif action == "f2":
                    if coordinator.session is None:
                        raise ApiError(409, "CREATE_SESSION_FIRST")
                    coordinator.start_run(coordinator.session["session_id"],
                                          {"request_id": "ui-f2", "suite_id": "PIXEL-2D"})
                elif action == "cancel" and coordinator.run is not None:
                    coordinator.cancel(coordinator.run["run_id"])
                else:
                    raise ApiError(409, "NO_RUN_TO_CANCEL")
            operator_message.set(f"{action.upper()} accepted")
        except ApiError as error:
            operator_message.set(error.code)

    for label, action in (("Create session", "create"), ("Step", "step"),
                          ("Run synthetic", "run"), ("Run F1 local", "f1"),
                          ("Run F2 pixels", "f2"),
                          ("Cancel", "cancel")):
        ttk.Button(controls, text=label, command=lambda a=action: operate(a)).pack(
            side="left", padx=(0, 8))
    ttk.Label(window, textvariable=operator_message).pack(padx=16, pady=(4, 0), anchor="w")
    state = tk.StringVar(value="Waiting for agent/API session")
    ttk.Label(window, textvariable=state, font=("Consolas", 11)).pack(padx=16, pady=12, anchor="w")
    preview = tk.Canvas(window, height=300, bg="#17202b", highlightthickness=0)
    preview.pack(padx=16, pady=(0, 10), fill="x")
    preview_label = preview.create_text(16, 16, text="SYNTHETIC TIMELINE - NO MODEL PIXELS",
                        fill="#f6c453", anchor="nw", font=("Segoe UI", 11, "bold"))
    marker = preview.create_oval(12, 70, 30, 88, fill="#44ccaa", outline="")
    ttk.Label(window, text="Shared API/window event timeline; checker is added after clean host readback").pack(
        padx=16, anchor="w")
    timeline = tk.Text(window, height=21, width=110, state="disabled", font=("Consolas", 10))
    timeline.pack(padx=16, pady=8, fill="both", expand=True)
    last = 0
    photo = None
    frame_key = None

    def refresh() -> None:
        nonlocal last, photo, frame_key
        current = coordinator.status()
        session = current["session"]
        run = current["run"]
        state.set(f"session={session['session_id'] if session else 'none'}   "
                  f"tick={session['synthetic_tick'] if session else 0}   "
                  f"run={run['suite_id'] if run else 'none'}   "
                  f"verdict={run['verdict'] if run else 'none'}")
        tick = session["synthetic_tick"] if session else 0
        width = max(50, preview.winfo_width() - 40)
        x = 12 + (tick % 40) * width / 40
        preview.coords(marker, x, 70, x + 18, 88)
        with coordinator.lock:
            coordinator.ui_visible = bool(window.winfo_ismapped())
            meta = dict(coordinator.frame_meta) if coordinator.frame_meta else None
            image = coordinator.frame_image
            events = [dict(item) for item in coordinator.events if item["seq"] > last]
        if meta is not None and image is not None:
            key = (meta["run_id"], meta["frame_generation"])
            if key != frame_key:
                photo = tk.PhotoImage(data=image, format="PPM").subsample(2, 2)
                preview.delete("host-frame")
                preview.create_image(16, 38, image=photo, anchor="nw", tags="host-frame")
                preview.itemconfigure(marker, state="hidden")
                preview.itemconfigure(preview_label, text=f"{meta['backend']} HOST FRAME "
                                      f"{meta['frame_generation']} | target epoch {meta['target_epoch']}")
                frame_key = key
                with coordinator.lock:
                    if coordinator.frame_meta and (coordinator.frame_meta["run_id"],
                            coordinator.frame_meta["frame_generation"]) == frame_key:
                        coordinator.ui_frame = {key: meta[key] for key in ("run_id", "frame_generation", "target_epoch")}
        elif frame_key is not None:
            preview.delete("host-frame")
            frame_key = None
            photo = None
            preview.itemconfigure(marker, state="normal")
            preview.itemconfigure(preview_label, text="NO COMPLETED HOST FRAME")
        if events:
            timeline.configure(state="normal")
            for event in events:
                timeline.insert("end", f"{event['seq']:05d}  {event['kind']}  "
                                f"{event.get('reason', event.get('stage', ''))}\n")
            timeline.see("end")
            timeline.configure(state="disabled")
            last = events[-1]["seq"]
        window.after(100, refresh)

    def close() -> None:
        with coordinator.lock:
            coordinator.ui_visible = False
            coordinator.ui_frame = None
        shutdown()
        window.destroy()

    window.protocol("WM_DELETE_WINDOW", close)
    if duration_seconds is not None:
        window.after(max(1, int(duration_seconds * 1000)), close)
    refresh()
    window.mainloop()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--window", action="store_true", help="show live synthetic timeline")
    parser.add_argument("--duration-seconds", type=float,
                        help="bounded local smoke run; omit for interactive use")
    args = parser.parse_args()
    if args.duration_seconds is not None and not 0 < args.duration_seconds <= 3600:
        parser.error("--duration-seconds must be in (0, 3600]")
    server = HarnessServer()
    descriptor = ROOT / "build" / "moc3-harness" / "session.json"
    descriptor.parent.mkdir(parents=True, exist_ok=True)
    attrs = getattr(descriptor.parent.lstat(), "st_file_attributes", 0)
    if descriptor.parent.is_symlink() or attrs & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0):
        server.server_close()
        raise SystemExit("HARNESS_DESCRIPTOR_DIRECTORY_REPARSE_HOLD")
    if not _harden_local_path(descriptor.parent):
        server.server_close()
        raise SystemExit("HARNESS_DESCRIPTOR_DIRECTORY_ACL_HOLD")
    descriptor_data = json.dumps({"schema_version": SCHEMA_VERSION,
                                  "host": "127.0.0.1", "port": server.server_port,
                                  "token": server.token})
    try:
        fd = os.open(descriptor, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except OSError:
        server.server_close()
        raise SystemExit("HARNESS_DESCRIPTOR_ALREADY_EXISTS_OR_UNAVAILABLE")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            stream.write(descriptor_data)
        if not _harden_local_path(descriptor):
            raise OSError("descriptor ACL")
    except OSError:
        descriptor.unlink(missing_ok=True)
        server.server_close()
        raise SystemExit("HARNESS_DESCRIPTOR_ACL_HOLD")
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    print("MOC3_T0_F1_F2_LOCAL", flush=True)
    print("Session descriptor: build/moc3-harness/session.json", flush=True)

    shutdown_lock = threading.Lock()
    stopped = False

    def shutdown() -> None:
        nonlocal stopped
        with shutdown_lock:
            if stopped:
                return
            stopped = True
        server.shutdown()
        server.server_close()
        descriptor.unlink(missing_ok=True)

    try:
        if args.window:
            show_window(server.coordinator, shutdown, args.duration_seconds)
        else:
            deadline = (time.monotonic() + args.duration_seconds
                        if args.duration_seconds is not None else None)
            while worker.is_alive() and (deadline is None or time.monotonic() < deadline):
                worker.join(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        shutdown()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
