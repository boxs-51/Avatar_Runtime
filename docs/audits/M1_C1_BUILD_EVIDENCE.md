# M1-C1 private CPU ingestion and bounded schema helper

Owner approved C0/C1/C2 plans, then `YES` for PR26's seven-file schema-process
amendment. Actual parent is landed B3 `7900f4954f49bd5d0ae3c7db0bc7e3f35095ec79`.
The implementation has exactly 41 paths: PR25's 34 C1 paths plus the approved
seven helper paths. PR25/26 are historical decision packets, not production
predecessors requiring an implicit merge. No public ABI/schema/inventory/design
blob change, source/model/character/update/render activation, GPU or SDK package.
Classification: SELF_AUDIT / INDEPENDENCE_NOT_AVAILABLE.

## Negative-first and C0 remediation

Native RED `9a1780f` calls the private production SchemaClient with a structurally
valid rig and expects OK; the missing process implementation returns UNSUPPORTED,
test exit 1. Raw log: `build/integration-evidence/c1-schema-red.log`.
The C0 PMR probe remains historical RED: the pinned schema library allocates
outside the host arena. It was not relabeled safe or replaced with a partial
schema checker. Jsoncons Draft2020-12 executes only in the approved helper.

Each request has fixed protocol/version, schema selector, length, unique checked
nonce and payload correlation checksum, with a 32-byte reply and EOF required.
The checksum/nonce is correlation, not cryptographic authentication. The trusted
absolute UTF-8 helper path never comes from an asset, shell or PATH lookup.
Schemas are generated from the frozen local files; no asset schema, network
resolver or external schema read is allowed. Parent strict parsing rejects
duplicate/escaped-duplicate keys, invalid UTF-8/surrogates, trailing tokens,
nonfinite/exponent-overflow numbers and depth/node/work excess before materialization.

Windows creates a suspended hidden process, assigns it to a Job with 128 MiB
process memory, one active process, CPU time and kill-on-close, then resumes it.
Only explicit stdin/stdout/NUL handles are inherited. A bounded I/O thread lets
the owner enforce the deadline even if the peer stops consuming a full pipe.
Linux posix_spawn uses an owned socketpair, closes inherited descriptors and
uses MSG_NOSIGNAL for writes. The trusted worker sets hard address-space/CPU/core
limits before reading any payload and installs parent-death cleanup. Polling,
kill/wait and Windows job/process/thread joins retain ownership through cleanup.

One helper per client and one full validation job per Resource are admitted;
reentry returns BUSY. Resource accounting reserves 128 MiB plus bounded IPC
workspace before launch. Crash, timeout, invalid status/version/nonce, short or
oversized reply produces typed failure and no model. The dedicated resource
probe is compiled from the actual worker setup with a test-only allocation
challenge; its branch is absent from the shipping helper. Resource isolation is
not a claim of a complete hostile-code sandbox. The helper is intentionally
unsanitized so Linux's 128 MiB address-space limit remains real; parent/decoders
are instrumented and the helper's actual OS limits are independently exercised.

## CPU asset boundary

The parser copies input into tracked ownership, validates a memory-only ZIP
envelope and never extracts, fetches a URI, loads a plugin or invokes GPU code.
It checks local/central names, flags, lengths/CRC, descriptors, contiguous
nonoverlapping ranges, symlinks, encoding, Unicode NFC/casefold aliases, Windows
device/drive/ADS paths, traversal and nested executable/archive payload names.
Stored/raw-DEFLATE members have cumulative byte/entry/ratio limits and actual
decoded-byte CRC. Pinned zlib/libpng/utf8proc objects use the real host allocation
boundary; cached source archives are verified before building, with no mutable
tag or system-library fallback. Offline cache miss/corruption is a configure error.

The parent uses a strict bounded flat JSON DOM in a shared 64 MiB arena.
The full schema runs in the process, then cross-file semantic checks validate
canonical channel identities, typed defaults/ranges, layer/parent/mask union
cycles, one-owner layer/part bindings, mapping types and parameter defaults,
finite meshes/UV/pivots, triangle indices and delta lengths. Actual PNG CRC,
chunk/count/decode/pixel limits precede image allocation; atlas rectangles use
decoded PNG dimensions, never canvas dimensions. Scalar, palette, gray, RGB,
16-bit and interlaced positive PNGs exercise the native decoder.

Only after all C1 checks and disabled-feature dispatch does ModelBuilder publish
an immutable owned CPU result, containing actual pixels, parameters, meshes,
deltas, layers/effective inherited visibility/opacity and semantic metadata.
Caller input may disappear afterward. All failure paths return a zero result;
existing separately held immutable models are not changed. The caller-owned
Resource must outlive its CPU results. This private contract is not public
registry/character ownership and does not grant source authority.

C1 rejects required 3D/VRM representations as UNSUPPORTED after its applicable
manifest/reference/2D checks; it does not claim to validate GLB accessors, skins,
morphs, animation or images. Actual GLB corruption/combined 3D disabled-feature
precedence is C2 evidence, still HOLD. No full G02/G08 or renderable-bundle PASS.

## Verification and exact-source evidence

Six native C1 groups exercise the compiled production pipeline, every generated
C1 fixture, worker protocol/OS cap/deadline/failure/cleanup, actual pixels and
caller lifetime, strict JSON and allocator faults. Allocation failure/retry is
tested through stored, DEFLATE and palette paths, plus throwing/misaligned host
allocation and reentry. Native logs list each fixture's expected/actual status
and each allocation cut; the committed corpus manifest retains its exact hash.
Final source SHA, matrix results, raw artifacts/digests and mutation evidence
are attested in the implementation PR rather than self-referentially here.

Local initial 75-fixture source passed all six Windows ASAN C1 groups (69 C1
fixtures plus explicit C2 boundary rejection), 52 stored-path fault/retry cuts,
and actual OS 256 MiB allocation denial under the 128 MiB worker cap. Later
source expands fault families/corpus and requires its own complete results.
These earlier passes do not certify final source or substitute for Linux.

Final required matrix is GCC/Clang shared/static, Linux ASAN+UBSAN, separate TSan,
Windows shared/static and actual Windows ASAN, preserved foundation/core tests,
ABI20 exports and two independently built plain-C consumers. Clang libFuzzer
instruments actual parser/decoder objects, not only the harness; 60-second smoke
and a 3600-second acceptance campaign retain seed, counts/RSS, logs and digests.
Any failed/timeout campaign remains failed until fixed and rerun on the new source.

Harness corrections retained honestly: Python ZipInfo normalized a hostile raw
backslash on Windows; C1 now writes the original filename bytes explicitly.
The first CI source failed all three Windows fixture targets because Git checks
text out with CRLF while the generator writes LF. The generator now compares the
complete ordered JSON oracle, including every binary hash and expected verdict,
independently of text line endings; it still rejects any oracle content drift.
MSVC debug iterator proxies allocate inside noexcept constructors, preventing
typed host-OOM propagation. Only private C++ assets/tests use the same iterator
ABI in all configs; public ABI and historical targets are unchanged, assertions
and ASAN remain active. PNG's setjmp frame contains only scalar locals and no
C++ owner; owning State/Image live in the caller and always clean up afterward.
The localized MSVC warning annotation documents this C-compatible error frame.
An initial concurrent local MSBuild exhausted this host's process memory;
subsequent local builds use one compiler and no node reuse, without reducing tests.

Historical B3 custody is pinned to its landed snapshot, including mocked git
targets. Existing B3 algorithms and B1/B2/A/M0 tests remain unchanged; a separate
41-path C1 gate rejects drift, helper omission, frozen mutations and fabricated
public/full-G02 PASS. All 87 original B whole-case verdicts remain HOLD, PASS=0.
C2 requires the actual landed C1 parent and its own exact-source gates; plan
approval persists but no merge is implied. Rollback is a separately reviewed
normal revert preserving the frozen ABI/history and prior evidence.
