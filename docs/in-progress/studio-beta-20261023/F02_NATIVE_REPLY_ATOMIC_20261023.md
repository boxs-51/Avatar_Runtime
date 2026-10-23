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
