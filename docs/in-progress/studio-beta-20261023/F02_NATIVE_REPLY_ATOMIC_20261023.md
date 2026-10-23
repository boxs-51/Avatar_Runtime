# F02 Native reply publication — atomic file visibility checkpoint

- Date: 2026-10-23
- Branch: exp/layer-physics-native-studio-local
- Source baseline: 00fa4449ffe3dff6463dfabc50bcecc5a40a972b
- Role: development self-test; independent audit remains outstanding
- Status: F02 PARTIAL; release HOLD

## Problem and bounded remediation

The Native v1 dispatcher formerly wrote directly into response.txt by
fopen(destination, "w"), fprintf, fclose. A same-user client polling the
file could observe it before write completion. A prior post-commit
regression run had one Native grammar RED without preserved detail in
its redirected output; a direct rerun then passed. Root cause of that
single transient failure remains UNPROVEN. The non-atomic response
visibility was independently observed in source and is a valid F02
robustness defect irrespective of the transient test.

The dispatcher now writes a sibling response.tmp, checks write,
flush and close, and only then uses the existing Windows
studio_replace_file helper (MoveFileExW replace/write-through) to
publish the complete response.txt. Failed publication does not claim
a successful ACK; any failed temporary file is removed when possible.
The V1 Python client separately requires a complete, typed,
newline-terminated ACK from its own request token. The Native grammar
test likewise ignores incomplete reply frames rather than accepting
a partial status.

## Verification (development scope)

- Native Studio RelWithDebInfo and Release fresh builds PASS.
- F01 Native malformed input matrix extended to 52 cases, including
  new model-generation read opcodes. RelWithDebInfo 4 consecutive
  runs: 52/52 PASS each; Release grammar regression PASS.
- Live V1 Agent API, local V2 Native generation readback, two-process
  model restart and IPC writer race PASS on the rebuilt Release.
- Earlier client truncated-ACK and foreign-ACK tests remain in
  the regression suite. Private stdout and any model-derived data are
  retained only in ignored build/studio-user local evidence.

## Security limitations

Atomic file visibility is not durable exactly-once execution.
Direct external same-user IPC writers, stale request ownership,
lost ACK after a committed mutation, request journal/replay policy,
power failure and native crash timing have not been fully qualified.
No transport swap, shared broker, distributed journal or H5 adapter
is introduced. Same-user Windows process permissions remain a trust
boundary. F02 stays PARTIAL / REVISE and release remains HOLD.

Next test matrix: crash before publish, after publish before read,
stale reply on restart, direct native-writer collision, and
idempotent state inspection after UNKNOWN_OUTCOME. Never interpret
file receipt OK as an accepted XPBD frame.

## Follow-up: Windows native receipt-sharing diagnostic

A later stress campaign reproduced a Win32 PermissionError at
scripts/test_studio_native_grammar.py inside path.read_text while
Native Studio concurrently published the response. That is a
test-harness file-sharing race, not evidence that PARAM_SET accepts
an invalid suffix. The earlier post-commit unexplained RED cannot
be retrospectively assigned the same cause without its raw trace.

The grammar test now uses a bounded strict-frame polling helper:
on transient OSError/PermissionError it retries within the original
deadline; only full newline-terminated responses with the expected
request token, OK/ERR code and nonempty receipt can be accepted.
Partial or foreign ACKs remain rejected/not deleted. It never retries
a Native command after an ambiguous outcome, only the read operation.

Synthetic tests in test_studio_native_reply_harness.py cover:
1. first read PermissionError then complete ERR;
2. incomplete OK never considered success;
3. foreign-token ACK neither accepted nor deleted;
4. valid complete OK accepted and cleaned up.

After the harness fix, 14 consecutive physical Native grammar matrix
runs each passed 52/52 with no unhandled access error. This supports
F07 test reliability improvement only; it does not prove physical
lost-ACK replay, exactly-once semantics or close F02.
