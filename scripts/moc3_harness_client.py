"""Local agent CLI for the private MOC3 test harness HTTP API."""

from __future__ import annotations

import argparse
import http.client
import json
from pathlib import Path
import re
import secrets
import sys


DESCRIPTOR = Path(__file__).resolve().parents[1] / "build" / "moc3-harness" / "session.json"


def call(method: str, route: str, body: dict | None = None) -> tuple[int, dict]:
    descriptor = json.loads(DESCRIPTOR.read_text(encoding="utf-8"))
    if descriptor.get("host") != "127.0.0.1" or \
            type(descriptor.get("port")) is not int or \
            not 1 <= descriptor["port"] <= 65535 or \
            not isinstance(descriptor.get("token"), str):
        raise ValueError("Invalid local session descriptor")
    connection = http.client.HTTPConnection("127.0.0.1", descriptor["port"], timeout=5)
    headers = {"X-Moc3-Test-Token": descriptor["token"]}
    payload = None
    if body is not None:
        body = {"request_id": secrets.token_hex(12), **body}
        payload = json.dumps(body, allow_nan=False).encode("utf-8")
        headers["Content-Type"] = "application/json"
    try:
        connection.request(method, route, body=payload, headers=headers)
        response = connection.getresponse()
        return response.status, json.loads(response.read())
    finally:
        connection.close()


def checked_id(value: str) -> str:
    if re.fullmatch(r"[0-9a-f]{24}", value) is None:
        raise ValueError("Invalid opaque ID")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("status")
    create = sub.add_parser("create")
    create.add_argument("--mode", choices=("deterministic", "live"),
                        default="deterministic")
    step = sub.add_parser("step")
    step.add_argument("session_id")
    step.add_argument("--delta-ns", type=int, default=50_000_000)
    run = sub.add_parser("run")
    run.add_argument("session_id")
    run.add_argument("suite_id")
    result = sub.add_parser("result")
    result.add_argument("run_id")
    frame = sub.add_parser("frame")
    frame.add_argument("session_id")
    events = sub.add_parser("events")
    events.add_argument("run_id")
    events.add_argument("--after", type=int, default=0)
    cancel = sub.add_parser("cancel")
    cancel.add_argument("run_id")
    args = parser.parse_args()
    try:
        if args.command == "status":
            status, value = call("GET", "/v1/status")
        elif args.command == "create":
            status, value = call("POST", "/v1/sessions",
                                 {"fixture_alias": "synthetic_t0", "mode": args.mode})
        elif args.command == "step":
            status, value = call("POST", f"/v1/sessions/{checked_id(args.session_id)}/steps",
                                 {"delta_ns": args.delta_ns, "parameters": {}})
        elif args.command == "run":
            status, value = call("POST", f"/v1/sessions/{checked_id(args.session_id)}/runs",
                                 {"suite_id": args.suite_id})
        elif args.command == "result":
            status, value = call("GET", f"/v1/runs/{checked_id(args.run_id)}")
        elif args.command == "frame":
            status, value = call("GET", f"/v1/sessions/{checked_id(args.session_id)}/frames/latest")
        elif args.command == "events":
            status, value = call("GET", f"/v1/runs/{checked_id(args.run_id)}/events?after={args.after}")
        else:
            status, value = call("POST", f"/v1/runs/{checked_id(args.run_id)}/cancel", {})
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(json.dumps({"error": type(error).__name__,
                          "message": "Local harness unavailable or descriptor invalid"}),
              file=sys.stderr)
        return 2
    print(json.dumps({"http_status": status, **value}, ensure_ascii=False,
                     indent=2, allow_nan=False))
    return 0 if 200 <= status < 300 else 1


if __name__ == "__main__":
    raise SystemExit(main())
