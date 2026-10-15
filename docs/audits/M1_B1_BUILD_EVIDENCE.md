# M1-B1 CPU registry and lifecycle evidence

Owner approved implementation of PR20 preparation at
`2d37c28a03f90de551b82b66c94153554b442bf5` on 2026-10-16 (Asia/Bangkok).
Implementation base is main `ab21908310d48466b27bb06c92ea1623353c79f5`.
Scope is the exact 24-path B1 set enforced by `check_m1_b1`; B2/B3 remain later decisions.
This document does not authorize merging the implementation. Final source SHA and
matching CI attestations belong in PR21, avoiding a self-referential commit hash.

## Implementation boundary

Opt-in `ARS_BUILD_RUNTIME_CORE=ON` builds `ARS::RuntimeCore`, separately from the
unchanged foundation behavior. Consumers link exactly one target. The core implements
runtime create, capabilities, begin/query/finish close and the existing ABI version.
All feature bits and model/character/source/GPU capability limits remain zero.
Other ABI entry points remain bounded UNSUPPORTED, including GPU drain.
The frozen public header and 9 predecessor plus 8 G00 design blobs are unchanged.

The production registry validates UID, slot, generation and object type before payload
access. Move-only leases release through their retained control reference, rather than
re-resolving a stale handle. Slots retire at UINT32_MAX; runtime UIDs never wrap or
reuse. Default bounds are 64 live/reserved runtimes, 1024 slots and 1024 pins per runtime.
Trusted component fixtures use isolated routers and lower limits; no public reset,
generation override, source grant or test export exists.

Close intent is sticky and creator-thread owned. Existing CPU work leases drain
asynchronously; new work is CLOSED. Query and finish are management exceptions.
Query includes its own management pin. CPU fence completion does not imply finish
can proceed while management pins remain. Finish claims exclusive finalization,
checks pins, invalidates and unlinks under the router lock, then frees outside locks.
Allocator callbacks also run outside locks; allocation failure rolls back reservations
without publishing a handle. Failed create UIDs are burned.

## RED evidence

Committed RED source: `a13beab75492d700424f4d153d57b77ada6c148c`.
The new public C contract was compiled against the pre-B1 foundation with MSVC:
`FAIL B1-create expected=0 actual=2`, exit 1 (real UNSUPPORTED behavior).
Local raw log: `build/integration-evidence/b1-behavior-red.txt`.
Remote RED run: https://github.com/boxs-51/Avatar_Runtime/actions/runs/38777752610.
All nine B1 matrix jobs failed at the old provenance gate (unrecognized B1 branch,
expected historical base b8099719..., actual ab219083...). This remote failure is
source-custody RED, not proof that a sanitizer or runtime test executed.

## Native verification

Local Windows x64 MSVC 19.51 / Visual Studio 18 2026:

- Shared Debug: 13/13 CTest tests passed, including exact 20 exports for both libraries.
- Static Debug: 11/11 CTest tests passed.
- Static RelWithDebInfo ASan: 11/11 passed; dumpbin confirms
  `clang_rt.asan_dynamic-x86_64.dll` and `__asan_*` imports. The historical PREP
  model keeps assertions enabled in release configurations, preserving its oracles.
- Four external plain-C consumers (foundation/core, shared/static) each passed.
- Both runs preserve the original 189-case foundation contract and five PREP tests.
- Public C exercises successful CPU create/capabilities/close, malformed inputs,
  stale handles, zero outputs, bounded prefix reads and future-tail preservation.
- Public C++ executes 1000 concurrent finalizer races with exactly one successful
  finish, plus wrong-thread begin-close validation.
- Registry tests exercise actual production objects: typed/foreign/stale handles,
  pin/capacity limits, 100000 reuse cycles, generation exhaustion/retirement,
  move ownership, invalidate with old leases and synchronized concurrent pinning.
- Lifecycle tests exercise actual production objects: work/management pin gates,
  concurrent finalizers, UID exhaustion, reservation contention, allocation rollback,
  callback reentry, and unlink-before-free with exactly one free.

Raw per-case native output is retained in `build/b1-local-{shared,static}/Testing/Temporary/LastTest.log`.
CI uses Windows-2022 VS2022 and Ubuntu GCC/Clang shared/static, separate ASan/UBSan
and TSan configurations, plus external plain-C consumers for foundation and core.
Sanitizer and final-source CI results must be checked on PR21; declaring a matrix
does not itself establish execution or success.

The first full candidate exposed four libstdc++ inline symbols on Clang shared
despite hidden visibility. Linux shared linking now restricts visibility with
a version script; the original strict exact-20 export checker is unchanged.
This correction requires fresh same-source CI, rather than inheriting earlier
worker results.

## Security and governance limits

`M1_B_CASE_EXECUTION.json` retains every original scenario/oracle/platform record,
with all 87 whole-case verdicts HOLD. Native component coverage is useful evidence
but cannot clear scenarios needing public model/character/source authority, adapters
or physical GPU resources. G07/G08 and M1-D GPU evidence remain HOLD.
No source revocation/quarantine, hostile decoder, device-loss, callback delivery,
GPU fence or full SDK claim is made.

Implementation and technical checks use the owner's connected account. They do not
constitute independent actor review. Owner exact-head merge approval remains separate.
