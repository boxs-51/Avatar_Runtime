"""Test-only two-process G5 recovery transport experiment on Windows.

This is NOT the production Studio IPC and not a security boundary. The
private synthetic Native G5Bus stays in a child process; two physically
different filesystem request slots simulate mutation vs read-only recovery.
No Cubism assets, GUI, customer paths, or writer enablement.
"""
import ctypes
import os
import re
import secrets
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

BASE = Path("build/studio-user")
DLL = Path("build/avatar-studio/Release/studio_g5_ipc_fixture.dll")
LIMIT = 80
TIMEOUT = 8.0


def await_condition(predicate, what):
    end = time.monotonic() + TIMEOUT
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError(f"timeout waiting for {what}")


def publish(folder, command):
    assert isinstance(command, bytes) and len(command) <= LIMIT
    request = folder / "request.txt"
    temp = folder / "request.tmp"
    assert not request.exists() and not temp.exists()
    temp.write_bytes(command)
    os.replace(temp, request)


def native(dll):
    lib = ctypes.CDLL(str(dll))
    lib.g5_fixture_init.argtypes = [ctypes.c_uint64]
    lib.g5_fixture_init.restype = ctypes.c_int
    lib.g5_fixture_commit_without_ack.argtypes = [ctypes.c_uint64]
    lib.g5_fixture_commit_without_ack.restype = ctypes.c_int
    lib.g5_fixture_hold_before_commit.argtypes = [ctypes.c_uint64]
    lib.g5_fixture_hold_before_commit.restype = ctypes.c_int
    lib.g5_fixture_read_only_status.argtypes = [ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)]
    lib.g5_fixture_read_only_status.restype = ctypes.c_int
    lib.g5_fixture_try_new_begin.argtypes = []
    lib.g5_fixture_try_new_begin.restype = ctypes.c_int
    lib.g5_fixture_reconcile.argtypes = [ctypes.c_uint64]
    lib.g5_fixture_reconcile.restype = ctypes.c_int
    lib.g5_fixture_revision.argtypes = []
    lib.g5_fixture_revision.restype = ctypes.c_uint64
    epoch = secrets.randbits(63) or 1
    assert lib.g5_fixture_init(epoch) == 0
    return lib, epoch


def server(root, dll):
    lib, epoch = native(dll)
    mutation, recovery = root / "mutation", root / "recovery"
    (root / "ready").write_text(str(epoch), encoding="ascii")
    while True:
        handled = False
        # Recovery slot is prioritized independently of mutation quarantine.
        for folder, is_recovery in ((recovery, True), (mutation, False)):
            req = folder / "request.txt"
            if not req.exists():
                continue
            handled = True
            payload = req.read_bytes()
            req.unlink()
            response = folder / "response.txt"
            if not is_recovery and payload == b"CLOSE":
                response.write_text("CLOSED", encoding="ascii")
                return 0
            if len(payload) > LIMIT or not payload or any(b < 0x20 or b > 0x7e for b in payload):
                response.write_text("INVALID", encoding="ascii")
                continue
            if is_recovery:
                match = re.fullmatch(rb"STATUS ([1-9][0-9]{0,18})", payload)
                if not match:
                    response.write_text("INVALID", encoding="ascii")
                    continue
                request_id = int(match.group(1))
                if request_id > 0x7FFFFFFFFFFFFFFF:
                    response.write_text("INVALID", encoding="ascii")
                    continue
                version = lib.g5_fixture_revision()
                revision = ctypes.c_uint64()
                code = lib.g5_fixture_read_only_status(request_id, ctypes.byref(revision))
                assert lib.g5_fixture_revision() == version
                if code == 0:
                    result = f"COMMITTED {request_id} {revision.value}"
                elif code == 8:
                    result = f"UNKNOWN {request_id}"
                else:
                    result = f"REJECTED {code}"
                response.write_text(result, encoding="ascii")
                continue
            match = re.fullmatch(rb"COMMIT_LOST ([1-9][0-9]{0,18})", payload)
            if match:
                request_id = int(match.group(1))
                result = lib.g5_fixture_commit_without_ack(request_id)
                assert result == 0, result
                # Intentionally drop ACK AFTER the Native owner commits.
                continue
            match = re.fullmatch(rb"HOLD_LOST ([1-9][0-9]{0,18})", payload)
            if match:
                request_id = int(match.group(1))
                result = lib.g5_fixture_hold_before_commit(request_id)
                assert result == 0, result
                # Owner never committed. ACK also absent: cannot assume replay safe.
                continue
            match = re.fullmatch(rb"RECONCILE ([1-9][0-9]{0,18})", payload)
            if match:
                code = lib.g5_fixture_reconcile(int(match.group(1)))
                response.write_text(f"RECONCILE {code}", encoding="ascii")
                continue
            if payload == b"NEW_BEGIN":
                code = lib.g5_fixture_try_new_begin()
                response.write_text(f"NEW_BEGIN {code}", encoding="ascii")
                continue
            response.write_text("INVALID", encoding="ascii")
        if not handled:
            time.sleep(0.02)


def ask(folder, command):
    response = folder / "response.txt"
    response.unlink(missing_ok=True)
    publish(folder, command)
    await_condition(response.exists, f"reply to {command!r}")
    result = response.read_text(encoding="ascii")
    response.unlink()
    return result


def drive():
    if os.name != "nt":
        raise SystemExit("Windows-only physical fixture")
    assert DLL.is_file(), "Build opt-in studio_g5_ipc_fixture target first"
    BASE.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="g5-recovery-", dir=str(BASE)))
    mutation, recovery = root / "mutation", root / "recovery"
    mutation.mkdir()
    recovery.mkdir()
    child = subprocess.Popen([sys.executable, "-B", __file__, "--server",
                              str(root), str(DLL.resolve())],
                             cwd=str(Path.cwd()), stdin=subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        await_condition(lambda: (root / "ready").exists() or child.poll() is not None,
                        "Native fixture child ready")
        assert child.poll() is None, f"fixture child exited {child.returncode}"
        first_epoch = int((root / "ready").read_text(encoding="ascii"))
        assert first_epoch > 0
        publish(mutation, b"COMMIT_LOST 700")
        await_condition(lambda: not (mutation / "request.txt").exists(), "owner consume")
        assert not (mutation / "response.txt").exists(), "lost ACK must stay absent"
        assert ask(recovery, b"STATUS 701") == "UNKNOWN 701"
        assert ask(recovery, b"STATUS 700") == "COMMITTED 700 8"
        assert ask(recovery, b"COMMIT 700") == "INVALID"
        assert ask(mutation, b"NEW_BEGIN") == "NEW_BEGIN 1"  # actor quarantine
        assert ask(mutation, b"RECONCILE 999") == "RECONCILE 7"  # wrong ID
        assert ask(mutation, b"NEW_BEGIN") == "NEW_BEGIN 1"
        assert ask(mutation, b"RECONCILE 700") == "RECONCILE 0"
        assert ask(mutation, b"NEW_BEGIN") == "NEW_BEGIN 0"
        assert ask(mutation, b"CLOSE") == "CLOSED"
        child.wait(timeout=TIMEOUT)
        assert child.returncode == 0, child.returncode
        child.communicate(timeout=TIMEOUT)

        # A fresh owner process has no volatile receipt from the prior epoch.
        (root / "ready").unlink()
        child = subprocess.Popen([sys.executable, "-B", __file__, "--server",
                                  str(root), str(DLL.resolve())],
                                 cwd=str(Path.cwd()), stdin=subprocess.DEVNULL,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        await_condition(lambda: (root / "ready").exists() or child.poll() is not None,
                        "fresh owner session")
        assert child.poll() is None
        second_epoch = int((root / "ready").read_text(encoding="ascii"))
        assert second_epoch > 0 and second_epoch != first_epoch
        assert ask(recovery, b"STATUS 700") == "UNKNOWN 700"

        # Fault before owner commit: unresolved marker and no mutation receipt.
        publish(mutation, b"HOLD_LOST 701")
        await_condition(lambda: not (mutation / "request.txt").exists(), "lost early ACK")
        assert not (mutation / "response.txt").exists()
        assert ask(recovery, b"STATUS 701") == "UNKNOWN 701"
        assert ask(mutation, b"NEW_BEGIN") == "NEW_BEGIN 1"

        # Crash while outcome is unresolved, then a new session/epoch.
        child.kill()
        child.communicate(timeout=TIMEOUT)
        (root / "ready").unlink()
        child = subprocess.Popen([sys.executable, "-B", __file__, "--server",
                                  str(root), str(DLL.resolve())],
                                 cwd=str(Path.cwd()), stdin=subprocess.DEVNULL,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        await_condition(lambda: (root / "ready").exists() or child.poll() is not None,
                        "owner restart after lost ACK")
        assert child.poll() is None
        third_epoch = int((root / "ready").read_text(encoding="ascii"))
        assert third_epoch > 0 and third_epoch not in (first_epoch, second_epoch)
        assert ask(recovery, b"STATUS 701") == "UNKNOWN 701"
        assert ask(recovery, b"STATUS 700") == "UNKNOWN 700"
        assert ask(mutation, b"CLOSE") == "CLOSED"
        child.wait(timeout=TIMEOUT)
        assert child.returncode == 0
        print("G5_PROCESS_RECOVERY_PASS channels=2 post_effect=COMMITTED "
              "pre_effect=UNKNOWN restart=volatile no_blind_replay=1")
        return 0
    finally:
        if child.poll() is None:
            child.kill()
        child.communicate(timeout=TIMEOUT)
        shutil.rmtree(root)


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--server":
        raise SystemExit(server(Path(sys.argv[2]), Path(sys.argv[3])))
    raise SystemExit(drive())
