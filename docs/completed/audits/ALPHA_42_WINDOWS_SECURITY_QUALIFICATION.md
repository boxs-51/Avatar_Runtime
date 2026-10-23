# Alpha42: local public security observations

2026-10-23, Asia/Bangkok. [Issue42](https://github.com/boxs-51/Avatar_Runtime/issues/42),
[bounded contract](../../in-progress/ALPHA_42_SECURITY_QUALIFICATION.md).
Base `d75fd268cea9880a5224403e69c010357fbb3580` includes unmerged PR79; this
branch stacks on that work, not a new alpha integration. GitHub Actions is
excluded under the owner's explicit quota instruction. Local Windows is primary.

## Observed cases

Real linked public CPU C ABI, Windows10 build19045/MSVC x64, static Release,
D3D11 OFF, sanitizers NONE. No private runtime access or contract-model calls.
Separate processes each load the generated validated dual bundle, assert exact
allocator address/size/alignment frees, then close and require zero allocations.

| Canonical case | Direct runtime assertions | Local result |
| --- | --- | --- |
| HDL-001 | Destroy character; subsequent destroy/mode return STALE_HANDLE | PASS |
| HDL-002 | Reuse same slot, generation changes, old handle stale, new character mode/update succeeds | PASS |
| HDL-003 | Model ID rejected as character, valid character remains usable | PASS |
| HDL-004 | New runtime has different UID; old runtime/character rejected and new character unaffected | PASS |
| HDL-012 | Zero handle INVALID_ARGUMENT; foreign UID and UINT32_MAX slot STALE_HANDLE; valid character unaffected | PASS |
| HDL-014 | Seed42,100000 randomized slot reuse cycles among16 live characters; generation increases, old handles stale, no allocator peak growth after warmup; final zero allocations | PASS |
| SRC-017 | Registered zero-channel capability denies mouth.open, grants no wildcard; revoke remains effective | PASS |
| SRC-019 | Admin ID bits used as bearer capability denied; real capability accepts same frame sequence; revoke then denies | PASS |

HDL014 measured warmed peak5797216 host-allocated bytes; this is an observation,
not a new production cap. It bounds tracked runtime allocations under the tested
schedule, not all OS/standard-library allocations or every possible race. Normal
execution returning stale without a crash is not sanitizer proof of no UAF.
All87 original case records remain in the receipt unchanged with added result
fields. Exactly8 local execution cells PASS,79 NOT_RUN. **All required original
platform cells and sanitizer obligations remain NOT_RUN; every case's complete
qualification stays HOLD.** This host is not windows-2022, Ubuntu GCC or Clang.
Do not equate8 local observations with8 completely qualified security cases.

## Verification and custody

Source-stable Release runner: `PARTIAL_LOCAL_PASS`,11 command exits0,163 input
hashes equal before/after. It resolves each executable/case/fixture from CTest's
actual configured command and rejects missing/extra cases. Source files,
canonical inventory, fixture SHA/size, executable and two adjacent workers are
pinned; binary/worker hashes are rechecked after each case and at completion.
Compiler metadata, CMake cache digest, host version, exact commands/log hashes,
seed and all87 case dispositions are retained.

Receipt: `E:/Avatar_Runtime/build/alpha42-evidence/344536f870ab48fa8960ab2509997fd3/evidence.json`.

| Pin | SHA256 |
| --- | --- |
| Receipt | `487cbf92099d1208df310974fb01c5c77b64799d391479ab99bb5238ad89bd68` |
| Source map: sorted compact JSON | `16d66a0d67953885992635fb558657be8ed9ad87a8b231c53e5cb7e25579b983` |
| Release public-security executable | `6ad3676e6f7b44ebde63f7177385d29cf9bc99a0296a19e4748381cf5a95a068` |
| Generated positive-dual-full bundle | `6d140aa68b3480e9f1eff80400d926d2e5f6989441118cfad059a432cdfcc727` |

Raw evidence is in ignored local build storage, not a GitHub artifact. This
report and the reproducer preserve the result/pins in version control. Fresh
Python contract discovery PASS188,124.665 seconds,exit0, including five new
negative-verifier tests. They reject incomplete/duplicate/foreign cases, wrong
seed/nonzero-live/missing trace, and dropped/duplicate/unknown inventory IDs.
Raw log `E:/Avatar_Runtime/build/alpha42-contracts.log` is PowerShell UTF16;
unittest writes stderr and PowerShell wraps it as NativeCommandError text, while
the actual process exit is0 and final verdict is OK.

Fresh unchanged production-component regression: PASS18/18,24.62 seconds,exit0.
This includes typed public resolution, registry/lifecycle, actual OS security
backends, source authority/controlled races, provenance/snapshots and all nine
publish-race families. Log `E:/Avatar_Runtime/build/alpha42-regression.log`.
Those component results retain their existing scope and do not turn any of the
79 unexecuted public scenarios into PASS.

Independent review found a real Debug link defect in the new test target:
RuntimeCore's CPU profile uses iterator level0, whereas the new direct C++
consumer defaulted to2. Before correction, static Debug build exit1 with
LNK2038/LNK1319. A conditional test-target definition, following the existing
consumer pattern, fixes it without changing runtime/header/export/defaults.
Debug target build then exit0; all8 registered Debug cases PASS,6.89 seconds,
exit0. These are additional Debug observations, not a new platform qualification.

| RED/GREEN local log | SHA256 |
| --- | --- |
| alpha42-debug-before.log | `70a92b7bf851ea649f80899a8672ff8cd546575d1b60389fe9a3a78d251e8b84` |
| alpha42-debug-after.log | `5ba5afde0a60326a06cc42586f920328d6ce4a79d7dd26a9835599a11222d79d` |
| alpha42-debug-cases.log | `2e240bdf7803c740382ce465a8f34b209112e5d9f8468f489ddbf300e9887dea` |

Preflight `988bbe5ade2d4f219b33220b73ae1f02` FAILED at HDL012 because the
first test implementation incorrectly demanded STALE_HANDLE for zero handles.
The frozen oracle permits INVALID_ARGUMENT or STALE_HANDLE; only the new test
was corrected to those two allowed outcomes, with exact statuses printed.
This is not a discovered runtime defect or a runtime RED-to-GREEN claim. The
runner's first inventory preflight also rejected incorrect numeric RACE IDs;
it now requires the canonical RACE-A..W. Neither failed preflight is acceptance.

## Reproduce and disposition

```powershell
python scripts/verify_public_security_cases.py --build E:/Avatar_Runtime/build/a42/s --evidence E:/Avatar_Runtime/build/alpha42-evidence --dependency-cache E:/Avatar_Runtime/build/c-deps
cmake --build E:/Avatar_Runtime/build/a42/s --config Debug --target ars_public_security_cases --parallel 4
ctest --test-dir E:/Avatar_Runtime/build/a42/s -C Debug -R '^alpha42_' --no-tests=error --output-on-failure
python -m unittest discover -s tests/contracts -p 'test_*.py'
```

The bounded local case-execution task is PASS; overall #42 is UNVERIFIED and
G07/G08 remain HOLD. Public GPU races/retirement, real physical loss, remote
adapter non-disclosure, complete deterministic race schedules, original required
environments and separate sanitizer runs are not proved here. The CPU opt-in
profile currently rejects integrated sanitizer configuration; that is a tooling
gap to resolve with a reviewed isolation/toolchain design, not a passing cell.
#41 and its Linux/pose gaps stay open. #61 whole integrated qualification,
#40 physical loss and #62 release packet remain separate. No merge or shipping
activation occurs, and no security oracle or production cap has changed.

Next runnable #42 work: add controlled public source admission/replay/close
schedules with explicit per-case assertions; design the missing integrated
sanitizer/platform profile before claiming its evidence. Existing component
race suites retain their original narrower scope and cannot fill the remaining
79 public-case cells by inference.

Independent reviewer `/root/f2_independent_review` reviewed the exact source
delta and evidence without edits/native execution. It found the Debug P2 above;
after correction its bounded re-review PASS verified stable source,11 successful
command/log receipts, current executable/worker hashes and cache digest. Final
documentation reconciliation PASS verified exact10-path scope, catalog173,
source/receipt/Debug log pins and the existing18 regression/188 contract logs.
Exact commit/stacked-PR custody is to be recorded on issue42 at publication.
