"""Private, owner-thread F1 package/evaluator transaction. No GPU calls."""

from __future__ import annotations

from dataclasses import replace
import math
from pathlib import Path
import queue
import subprocess
import threading

from moc3_package_admission import AdmissionError, BudgetPolicy, ModelPackage, admit_package
from moc3_memory import process_memory


MAX_TOTAL_PEAK_COMMIT = 1536 * 1024 * 1024


class EvaluatorError(Exception):
    def __init__(self, code: str):
        super().__init__(code)
        self.code = code


class _Worker:
    """One committed package, one native candidate, one optional snapshot pin.

    The package owns immutable MOC/atlas bytes. The native worker receives the
    exact MOC blob and atlas count; it never reopens a package path. All public
    methods belong to the creating thread. Results contain no host pixels.
    """

    def __init__(self, bridge: Path):
        self.owner = threading.get_ident()
        self.package: ModelPackage | None = None
        self.package_generation = 0
        self.snapshot_generation = 0
        self.feature_inventory: tuple[int, ...] = ()
        self.responses: queue.Queue[bytes | None] = queue.Queue(maxsize=4)
        self.reader = threading.Thread(target=self._read_responses, daemon=True)
        self.process = subprocess.Popen([str(bridge), "--package"],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL)
        started = False
        try:
            self.reader.start()
            started = True
            if self._response() != ["BOOT"]:
                raise EvaluatorError("BRIDGE_BOOT_FAILED")
        except BaseException:
            if self.process.poll() is None:
                self.process.kill()
            self.process.wait(timeout=5)
            if started:
                self.reader.join(timeout=1)
            self.process.stdin.close()
            self.process.stdout.close()
            raise

    def _read_responses(self) -> None:
        while True:
            line = self.process.stdout.readline(1025)
            if not line or len(line) > 1024:
                self.responses.put(None)
                return
            self.responses.put(line)

    def _owner(self) -> None:
        if threading.get_ident() != self.owner:
            raise EvaluatorError("WRONG_THREAD")
        if self.process.poll() is not None:
            raise EvaluatorError("EVALUATOR_UNAVAILABLE")

    def _response(self) -> list[str]:
        try:
            line = self.responses.get(timeout=15)
        except queue.Empty as error:
            self.process.kill()
            self.process.wait(timeout=5)
            raise EvaluatorError("EVALUATOR_TIMEOUT") from error
        if line is None:
            raise EvaluatorError("EVALUATOR_CLOSED")
        return line.decode("ascii").split()

    def command(self, value: str) -> list[str]:
        self._owner()
        self.process.stdin.write(value.encode("ascii") + b"\n")
        self.process.stdin.flush()
        return self._response()

    def replace(self, candidate: ModelPackage, *, fault: int = -1) -> None:
        self._owner()
        # Reject before transport; the native boundary independently checks.
        if not 4 <= len(candidate.moc_bytes) <= 16 * 1024 * 1024 or \
                not 1 <= len(candidate.textures) <= 16 or type(fault) is not int or fault < -1:
            raise EvaluatorError("INVALID_PACKAGE")
        if type(candidate.moc_bytes) is not bytes or type(candidate.textures) is not tuple or \
                any(type(t.rgba) is not bytes or type(t.width) is not int or type(t.height) is not int
                    or not 0 < t.width <= 8192 or not 0 < t.height <= 8192
                    or len(t.rgba) != t.width * t.height * 4 for t in candidate.textures) or \
                sum(len(t.rgba) for t in candidate.textures) > 512 * 1024 * 1024:
            raise EvaluatorError("INVALID_PACKAGE")
        # Prepare all Python-owned publication data before native commit.
        committed = replace(candidate, moc_evaluated=True)
        next_generation = self.package_generation + 1
        header = f"LOAD {len(candidate.moc_bytes)} {len(candidate.textures)} {fault}\n"
        self.process.stdin.write(header.encode("ascii"))
        self.process.stdin.write(candidate.moc_bytes)
        self.process.stdin.flush()
        response = self._response()
        if len(response) != 3 or response[0] != "READY":
            raise EvaluatorError("PACKAGE_REJECTED")
        # The native LOAD publishes only on success. INFO is diagnostic only;
        # failure after native commit invalidates the worker, never keeps an
        # old package paired with a new evaluator.
        try:
            info = self.command("INFO")
            if len(info) != 7 or info[0] != "INFO":
                raise EvaluatorError("INVALID_FEATURE_INVENTORY")
            inventory = tuple(map(int, info[1:]))
            generation = int(response[1])
        except (EvaluatorError, ValueError, MemoryError):
            self.process.kill()
            self.process.wait(timeout=5)
            raise
        self.package = committed
        self.package_generation = next_generation
        self.snapshot_generation = generation
        self.feature_inventory = inventory

    def load(self, root: Path, manifest: str, policy: BudgetPolicy = BudgetPolicy(),
             *, requested_features: tuple[str, ...] = (), fault=None) -> None:
        self._owner()
        candidate = admit_package(root, manifest, policy,
                                  requested_features=requested_features, fault=fault)
        self.replace(candidate)

    def step(self, parameters: dict[str, float], *, expected_generation: int | None = None) -> None:
        self._owner()
        if self.package is None:
            raise EvaluatorError("NO_PACKAGE")
        if expected_generation is not None and expected_generation != self.package_generation:
            raise EvaluatorError("STALE_PACKAGE_GENERATION")
        if not isinstance(parameters, dict) or len(parameters) > 4096:
            raise EvaluatorError("INVALID_PARAMETER")
        lines = []
        for key, value in parameters.items():
            if not isinstance(key, str) or not key or len(key) > 255 or \
                    not key.isascii() or any(c.isspace() for c in key) or \
                    any(ord(c) < 33 or ord(c) > 126 for c in key) or \
                    type(value) not in (int, float):
                raise EvaluatorError("INVALID_PARAMETER")
            try:
                number = float(value)
            except OverflowError as error:
                raise EvaluatorError("INVALID_PARAMETER") from error
            if not math.isfinite(number):
                raise EvaluatorError("INVALID_PARAMETER")
            lines.append(f"{key} {number:.9g}")
        response = self.command("\n".join([f"FRAME {len(lines)}", *lines]))
        if len(response) != 3 or response[0] != "OK":
            raise EvaluatorError("INVALID_PARAMETER_OR_UPDATE")
        self.snapshot_generation = int(response[1])

    def close(self) -> None:
        if self.process.poll() is None:
            try:
                self.command("QUIT")
                self.process.wait(timeout=3)
            except (OSError, EvaluatorError, subprocess.SubprocessError):
                self.process.kill()
                self.process.wait(timeout=5)
        self.reader.join(timeout=1)
        self.process.stdin.close()
        self.process.stdout.close()

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()


class AdmittedSession:
    """Publish a fully prepared immutable package and CPU worker together.

    Replacement owns at most the committed worker and one candidate. A failed
    candidate is destroyed without touching the committed worker or snapshot.
    """

    def __init__(self, bridge: Path):
        self.bridge = bridge
        self.owner = threading.get_ident()
        self.active: _Worker | None = None
        self.native_peak_pair_bytes = 0
        self.peak_commit_limit = MAX_TOTAL_PEAK_COMMIT

    def _owner(self) -> None:
        if threading.get_ident() != self.owner:
            raise EvaluatorError("WRONG_THREAD")

    @property
    def package(self):
        return self.active.package if self.active else None

    @property
    def package_generation(self):
        return self.active.package_generation if self.active else 0

    @property
    def snapshot_generation(self):
        return self.active.snapshot_generation if self.active else 0

    @property
    def feature_inventory(self):
        return self.active.feature_inventory if self.active else ()

    @property
    def process(self):
        return self.active.process if self.active else None

    def _track_peak(self, candidate: _Worker) -> int:
        peaks = process_memory(candidate.process.pid)["peak_commit_bytes"]
        if self.active:
            peaks += process_memory(self.active.process.pid)["peak_commit_bytes"]
        self.native_peak_pair_bytes = max(self.native_peak_pair_bytes, peaks)
        return peaks

    def replace(self, package: ModelPackage, *, fault: int = -1) -> None:
        self._owner()
        candidate = _Worker(self.bridge)
        try:
            candidate.replace(package, fault=fault)
            candidate.package_generation = self.package_generation + 1
            peaks = self._track_peak(candidate)
            if process_memory()["peak_commit_bytes"] + peaks > self.peak_commit_limit:
                raise EvaluatorError("OUT_OF_BUDGET")
        except BaseException:
            try:
                if candidate.process.poll() is None:
                    self._track_peak(candidate)
            finally:
                candidate.close()
            raise
        prior = self.active
        self.active = candidate
        if prior:
            prior.close()

    def load(self, root: Path, manifest: str, policy: BudgetPolicy = BudgetPolicy(),
             *, requested_features: tuple[str, ...] = (), fault=None) -> None:
        self._owner()
        retained = 0
        if self.package:
            retained = (len(self.package.moc_bytes) + sum(len(t.rgba) for t in self.package.textures)
                        + len(self.package.linked_physics_bytes or b""))
        package = admit_package(root, manifest, policy,
                                requested_features=requested_features, fault=fault,
                                retained_bytes=retained)
        self.replace(package)

    def command(self, value: str) -> list[str]:
        self._owner()
        if not self.active:
            raise EvaluatorError("NO_PACKAGE")
        return self.active.command(value)

    def step(self, parameters: dict[str, float], *, expected_generation: int | None = None) -> None:
        self._owner()
        if not self.active:
            raise EvaluatorError("NO_PACKAGE")
        self.active.step(parameters, expected_generation=expected_generation)

    def close(self) -> None:
        self._owner()
        if self.active:
            self.active.close()
            self.active = None

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()
