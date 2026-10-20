# Alpha #59: ABI20 profile, ownership and installed development contract

2026-10-20, Asia/Bangkok. Baseline alpha
`52f3c7366021988d6d035d70c43bf13d22f4e337` after issue58/PR70.
This slice freezes the already owner-selected ABI20 surface and its current
feature-off development-package behavior, and records the implementation handoff
for #60/#38/#39. It does not approve a new symbol,asset format or capability.
The original canonical declarations,status values and security oracles stay intact.

## Sources and reconciliation

| Input | Role at this checkpoint | Reconciliation |
| --- | --- | --- |
| `include/ars/ars.h` and SPECIFICATION_V0.1 section5/7 | Existing ABI20 layout/symbols and approved H/I/S/R design | Preserve all declarations,handle representation and status numbers; header candidate preamble is historical and does not describe later CPU lifecycle implementation |
| `src/abi/api_validation.hpp`,runtime_api.cpp,unsupported.cpp | Current executable boundary | CPU lifecycle is real; model/character/source/update/render/drain remain feature-off; descriptor prefix checks do not constitute asset validation |
| RuntimeRouter,SourceAuthority,PublishGate | Current private CPU ownership/authority/publication | Internal character/source APIs and adapters are not extra exported C functions |
| [issue58 ownership inventory](ALPHA_58_SESSION_LIFECYCLE_DESIGN.md) and [acceptance](../completed/audits/ALPHA_58_SESSION_ACCEPTANCE.md) | Source-grounded private GPU/context/worker owners and verified bounded lease lifecycle | Installed RuntimeCore owns no public GPU context or MOC3 session; those owners must be registered explicitly by later production integration |
| [earlier ABI freeze map](V0_1_PUBLIC_ABI_FREEZE_MAP.md) | Mixed historical design plus effective owner ABI20 decision | Earlier missing-signature/stub-only descriptions are historical; packaging/runtime proof must name the actual profile rather than infer whole G00/G01/G10 closure |
| [linear backlog](ALPHA_LINEAR_BACKLOG.md) | Current order | #59 contract/package then #60 CPU asset/authority then #38/#39 production integration |

No shared DB or architecture cutover applies. Private parsers/renderers coexist
with a deliberately bounded public CPU lifecycle; package exports expose only
the C boundary. No private C++ class,model-derived payload or mutable raw pointer
becomes an SDK authority object.

## Frozen negotiation and memory rules

Package0.1.0 and ABI1.0 are different versions. `ars_get_abi_version()` returns
`(1<<16)|0`. Same-major caller records with newer minor/longer size use only the
known required layout; higher minor never grants features. A null record,known
size too short,wrong major or nonzero recognized reserved field is INVALID_ARGUMENT.
The caller supplies readable/aligned native pointers and an accessible size prefix;
the implementation is not a hostile-pointer sandbox. Short records return before
reading fields beyond their declared known prefix. Unknown suffix bytes are not
read/written. There is no implicit byte-stream-to-native-pointer conversion.

All handle/capability outputs are fixed-size caller-owned writable objects and
are cleared before any failure. Output records first validate known capacity,
major and recognized reserved fields; malformed output descriptors are left
untouched. Once validated,the known output body is cleared even if handle lookup
fails; success returns the known struct_size and current ABI version. Trailing
caller canaries are unchanged. No API returns borrowed STL storage. Unknown enum
or payload validation in feature-off stubs must not be confused with implemented
feature validation: current stubs check prefix/reserved fields and return
UNSUPPORTED without parsing pointer payloads or authenticating runtime handles.

H is the creating/control thread. Input descriptors and all nested byte/string/
channel arrays remain readable for their synchronous call only. Enabled future
operations must copy into budgeted owned storage before returning; no input
pointer retention. Alloc/free are both null (implementation pair) or both present
(host pair),receive matching byte count/alignment,user data,and outlive every
allocation and completion. They cannot throw across C,run under security/pose/
GPU locks,or reenter prohibited control methods. Null allocation cannot publish
partial state. Clock callback storage outlives runtime; security lease time uses
the existing OS clock,not caller simulation timestamps. C++ implementation and
exceptions never cross the C ABI. Static C consumers use a C++ link driver only.

## Per-operation contract and current support

The table separates actual installed behavior from the unchanged target design.
H/I/S/R meanings follow the specification. Management exceptions below describe
the CPU-only profile; they do not authorize GPU cleanup from the wrong thread.

| Symbol (exact20) | Current installed profile | Ownership/thread and enabled implementation obligation |
| --- | --- | --- |
| ars_get_abi_version | Returns ABI1.0 | No runtime/pointer requirement |
| ars_runtime_create | Real CPU runtime;paired callbacks,zero failure output | H established on successful create;descriptor copied;OOM/misalignment roll back allocation/registry reservation |
| ars_runtime_get_capabilities | Real lookup;all feature bits/feature limits0 | Synchronous work lease;CLOSED after begin_close,STALE_HANDLE after finish;known writable output zeroed on lookup failure |
| ars_model_load | UNSUPPORTED after descriptor-prefix validation | Future H:bounded synchronous memory-bundle adapter,immutable owned validated CPU asset;copy or isolate caller bytes;no GPU upload or raw input retention |
| ars_model_release | UNSUPPORTED | Future H:invalidate user handle,retain CPU model pins held by characters/work;one logical release must not invalidate a live character |
| ars_character_create | UNSUPPORTED,zero output | Future H:pin model in same runtime,validate requested representation/caps,budgeted instance;failure leaves model and prior objects intact |
| ars_character_destroy | UNSUPPORTED | Future H:close instance admission,revoke sources,invalidate handle,retire pins independently;never free live GPU work on H implicitly |
| ars_character_set_mode | UNSUPPORTED | Future H at simulation quiescence:prepare/validate/commit or preserve old mode and accepted pose;AUTO uses only actually supported representations |
| ars_character_set_visual_layer | UNSUPPORTED after prefix validation | Future H:explicit Profile B layer-to-part mapping;atomic visibility/opacity/order change;3D layer control stays unsupported until separately specified |
| ars_source_register | UNSUPPORTED,zero admin and bearer | Future trusted H only:copy allowed semantic IDs,bounded source policy,match live character catalog;remote authentication belongs to host adapter |
| ars_source_admin_release | UNSUPPORTED | Future H:drop administrative handle according to existing source state/lease rules;never mint a replacement token implicitly |
| ars_source_rotate | UNSUPPORTED,zero bearer | Future trusted H:invalidate prior epoch and mint or terminal-fail;lost/revoked tokens cannot recover via request-controlled IDs |
| ars_source_revoke | UNSUPPORTED | Future trusted H:final source fence denies in-flight speculative publication;accepted state is retained until explicit authorized replacement |
| ars_motion_submit | UNSUPPORTED after prefix validation | Future concurrent I:bounded complete channel copies;replay/TTL/scope/type/clock/rate/queue checks from current security engine;no remote principal authority in frame data |
| ars_runtime_update | UNSUPPORTED | Future single S:prepare immutable candidate,final authority fence,atomic accepted snapshot;no overlap with update/structural H mutation |
| ars_runtime_render | UNSUPPORTED after prefix validation | Future R only:borrowed same-generation HOST_DEVICE/context/RTV,restore touched state,exact query and final publication;retain COM/snapshot pins until completion or justified quarantine |
| ars_runtime_begin_close | Real;H only;repeat returns CLOSED | Close work admission first,begin quiescence;no GPU completion fabricated |
| ars_runtime_drain_gpu | UNSUPPORTED | Future R:bounded progress of nominated queries;BUSY while pending,DEVICE_LOST/quarantine on unknown loss;must consume production context barrier from #38 |
| ars_runtime_query_close | Real management query | Thread-safe CPU management lease;own query pin is included;COMPLETE reflects quiescing+no CPU work,not an assertion that a public GPU was tested |
| ars_runtime_finish_close | Real management finalization | CPU-only finalizer may run on another thread;BUSY until quiescent/no pins;one winner unlinks before free outside locks,subsequent calls STALE_HANDLE;GPU-enabled future must satisfy R retirement/barrier first |

Current lifecycle:OPEN -> begin_close on H -> QUIESCING -> management query/
finish -> removed registry entry. Pending queries count their own management pin;
COMPLETE does not mean outstanding_pins is necessarily0 while query owns a pin.
The installed C++ consumer checks wrong-thread begin and a separate-thread CPU
finalizer. It does not extrapolate CPU behavior to future GPU destruction.

## Status matrix and feature-off precedence

Numeric values remain the existing int32 ABI constants:

| Value/status | Caller meaning and preservation rule |
| --- | --- |
| 0 OK | Fully initialized success output;accepted state advances only at its actual commit |
| 1 INVALID_ARGUMENT | Invalid native descriptor/prefix/reserved fields or zero runtime ID;no partial resource |
| 2 UNSUPPORTED | Feature/profile unavailable;current stubs return this before runtime/asset/authority validation once their descriptor prefix is acceptable |
| 3 BAD_ASSET | Enabled adapter rejects malformed/bad semantic asset before GPU;old accepted objects stay valid |
| 4 OUT_OF_BUDGET | Allocation/work/admission cap;no automatic cap increase or partial output |
| 5 WRONG_THREAD | H/S/R affinity violation for implemented operation;no unsafe cleanup |
| 6 DEVICE_LOST | Enabled graphics generation cannot complete safely;quarantine until justified teardown |
| 7 STALE_HANDLE | Nonzero runtime/object identity no longer resolves or generation/type mismatch |
| 8 BUSY | Pins,callback guard,finalizer or pending work prevents progress;retry through authorized management path |
| 9 INTERNAL | Unexpected caught internal failure;typed cleanup and no boundary exception |
| 10 CLOSED | Runtime/object closing rejects ordinary work;management exceptions remain bounded |
| 11 REVOKED | Explicit source revocation;never equivalent to renewing a lease |
| 12 SCOPE_DENIED | Bearer lacks requested character/channel scope |
| 13 INVALID_FRAME | Enabled ingress rejects value/type/shape;no unauthorized candidate publication |
| 14 CLOCK_REGRESSION | Simulation/accepted input clock violation;do not relabel rejected time |
| 15 HANDLE_EXHAUSTED | UID/generation/token space exhausted;never wrap identities |
| 16 CLOCK_FAILURE | Required clock sample cannot be trusted;do not synthesize successful time |
| 17 RATE_LIMIT | Source credit policy denied work;no hidden queue growth |
| 18 QUEUE_FULL | Bounded ingress queue denied work;no silent admission |
| 19 SOURCE_EXPIRED | Terminal lease expiry;distinct from explicit revoke |
| 20 SECURITY_CLOCK_FAULT | Existing security clock poison/rollback policy;not caller-resettable |

No new error symbol,diagnostic function or capability bit is introduced. The
optional21st diagnostic remains deferred. Future enabled APIs validate native
record shape,admit required runtime/object leases,check thread/authority and
then perform bounded work;their exact combined-error precedence must be tested
in #60 without changing the feature-off profile's observed UNSUPPORTED behavior.

## Assets,input and rendering handoff boundaries

The existing asset kind remains ARS_ASSET_MEMORY_BUNDLE. It is not permission to
reinterpret arbitrary MOC3 bytes as the approved native2D/GLB bundle. #60 must bind
the schema-valid immutable bundle/model registry to approved CPU semantics and
retain default feature-off behavior. Public MOC3 ingestion needs an explicit
versioned supported profile and bounded IPC integration; all user MOC/PNG/Physics
parsing stays in the isolated path from #58. A trusted-local oracle is never the
public parser. Unsupported/full-resolution derivatives must remain explicit.

There is no public parameter-catalog/readback/HTTP API in ABI20. The #58 loopback
observer is a private authenticated test tool;numeric private slots and ordinal
manual channels cannot silently become public semantic IDs. #60 must derive
stable allowed semantic channels from validated approved rig bindings. Additional
public import/control/readback functions require a separate versioned decision,
not a new alias or an undocumented kind value in this package.

HOST_DEVICE remains the owner-approved target design:host retains device/context/
RTV through GPU completion;R serialization,device/target generations,color/alpha,
query retirement and restore scopes remain mandatory. Private LeasePublication,
HostDevice,QueryRetirement,atlas residency and UI pipe owners are separate domains
identified in #58;installing RuntimeCore registers none of them in a production
barrier. #38 must enumerate/admit/surrender those references before a production
receipt;#39 must demonstrate committed pose/camera/render integration. No GPU or
worker implementation is linked/exported by this development package.

## Installed package and acceptance

See the [package usage contract](../reference/SDK_DEVELOPMENT_PACKAGE.md).
`ARS_INSTALL_RUNTIME_CORE_SDK=OFF` by default. ON requires actual RuntimeCore and
NONE sanitizer;the package exports only `ARS::RuntimeCore`,the canonical header
and bounded development documentation. Static/shared variants and configurations
are separate prefixes. Same-major ABI records and package0.1.x version selection
remain separate from capabilities. The config rejects incompatible platform or
requested unknown required component;the version file checks pointer width.

Real acceptance uses a fresh actual library,cmake install,physical prefix move,
standalone copied C11/C++20 consumers and installed-only find_package. All20 APIs
are resolved/called against the real library. Short/major/minor/reserved/output
canary/failure-zero and H/CPU management lifetime negatives are behavioral proof.
A poisoned copied installed header must fail compilation,preventing checkout
include fallback from masquerading as SDK proof. Export checks retain exact20.
Static/shared Release/Debug Windows local runs are primary;Linux GCC/Clang and
other platform packaging require their own observed runs. Missing platform proof
is reported,never inferred from a CMake configure or CI queue.

This slice closes only the ABI20 current-profile contract and installed development
packaging. G00 technical-method,G01/G02/G10 full supported feature matrices,#60
public CPU asset/authority,#38/#39 production host and physical D06/D08 retain
their own evidence obligations. No main merge,public activation,asset publication,
SDK dependency adoption or driver reset follows from this document.
