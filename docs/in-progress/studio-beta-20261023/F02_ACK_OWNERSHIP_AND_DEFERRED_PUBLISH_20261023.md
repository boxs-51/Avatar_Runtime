# F02 — Native ACK ownership and deferred publication

- Date: 2026-10-23
- Branch: exp/layer-physics-native-studio-local
- Baseline: 3be637a9faa9acc404f54190014a92fc282ac424
- Authority: local engineering self-check, not independent audit
- Status: Scoped RED-to-GREEN; F02 PARTIAL; RELEASE HOLD

## Native protocol changes

When Windows sharing rules prevent atomic replacement of response.txt,
Native keeps the already flushed and closed response.tmp and retries
publication in future frames. It does NOT retry or re-apply the command.
The render loop is not blocked by a long synchronous retry.

Native also checks whether a previous response.txt is still present.
Until the previous response is removed, the dispatcher does not admit
another request. Unexpected file-status errors fail closed. This
prevents direct same-user writers from causing the second Native
command to overwrite an unacknowledged first response.

The ACK must remain a complete, owner-token-correlated frame. A
successful file publication is not equivalent to an accepted physics
frame or durable exactly-once mutation.

## RED-first evidence

1. A Windows physical test used a handle that explicitly denied
   response-file replacement. Previously Native consumed the request,
   failed replacement and silently dropped the ACK: RED.
2. After adding deferred publication, the same test passed when the
   handle was released: GREEN.
3. A second physical test placed request B before response A was
   acknowledged. Native previously overwrote A's response: RED.
4. The strict unread-response admission guard now preserves A and
   delays B until the client removes A. Tests: 5/5 physical repeats,
   both stale-response blocking and ACK handoff PASS.
5. Fresh Release build and V1/V2, Native grammar, local model read,
   restart and writer race regressions all PASS in the tested scope.

Raw trace data, private paths, model identifiers, numeric responses
and screenshots stay under ignored build/studio-user; this report
contains categorical results only.

## Remaining F02 limitations

- Crash between committing a non-idempotent mutation and publishing
  or consuming its ACK may leave UNKNOWN_OUTCOME.
- The in-memory pending state is not a crash-proof journal.
- A same-user process able to delete or forge IPC files is not
  isolated by this mechanism; OS authentication boundaries are not
  proved.
- Stale request recovery, malicious direct-writer interference,
  filesystem corruption and edit/frame acceptance are still OPEN.

F02 remains PARTIAL, F07 PARTIAL, G4 PARTIAL, release HOLD. Native
SDK ABI20 unchanged. No GitHub writes, push, merge or deployment.
