# Alpha A09 / issue60 isolated native admission design

Status: original A60-0/A60-1a contract plus current continuation. The [typed 2D transfer](ALPHA_60_TYPED_2D_TRANSFER_DESIGN.md) now implements A60-1b-2D privately; complete GLB/dual transfer and public lifecycle remain open.
Base: `49eb54a88cfc30e1104bf12750852827be36aa87` (reviewed ABI20 development SDK).

## Target and sequence

1. **A60-0** freeze native CPU admission boundary and staged evidence below.
2. **A60-1a** run complete native bundle validation inside a zero-capability Windows AppContainer, including ZIP, pinned JSON schemas, PNG, GLB and immutable private CPU model construction. Return only a diagnostic verdict. This is the current implementation slice.
3. **A60-1b** independently validate a complete typed immutable model packet. The current 2D transfer covers geometry/catalog/atlas/bounds; GLB/dual graphs remain pending. A verdict never substitutes for model validation; it cannot create a registry handle.
4. **A60-2** runtime-owned resource and bounded model records; character/model pin and close/OOM semantics.
5. **A60-3** reviewed semantic bindings into existing source/ingress/publication authority; revoke, replay, expiry, clock and close fences.
6. **A60-4** relocated installed C11/C++20 static/shared enabled-profile consumers plus unchanged feature-off consumers.

Full GLB/dual transfer and the last three steps are unimplemented. No change to ABI20, capability bits, public activation, MOC3/Physics support, GPU rendering, main or physical D06/D08 HOLD follows from this probe.

## Choice and deployment

Connecting the existing in-process decoder would violate the required isolation. Implementing the complete model transport and public authority bridge together is viable but joins three trust/lifetime boundaries before isolated decoding has evidence. The selected staged probe supplies an executable isolation oracle first and leaves downstream acceptance explicit.

`ARS_BUILD_CPU_ADMISSION_PROBE` defaults OFF, requires Windows, asset validation, and no sanitizer instrumentation for the contained helper. It uses a dedicated static-CRT experiment build for consistent helper/dependency linkage and rejects combination with SDK installation. It builds separate trusted worker and broker executables; it is neither installed nor exported as the public SDK. Tests copy the worker into a fresh ignored directory with a read-only sentinel, then invoke the fixed built broker with an absolute helper path. The archive controls stdin only. Future SDK helper discovery/deployment still needs its own review and installed relocation proof.

Workers use the existing pipe-only Windows subsystem/CRT entry pattern; no console permission or machine policy is changed. The existing broker source is compiled into a separate target. Its Job retains 512 MiB process/job memory, one active process, two user CPU seconds, five wall seconds and 64 output bytes. The original MOC3 target keeps its existing modes/protocol. Every probe test checks actual launcher accounting and profile deletion. After process exit the broker observes Job accounting reach zero within a bounded 500 ms cleanup drain; it records the initial/final count and fails if zero is never observed. Setup, worker, cleanup and transport failures never become an accepted asset verdict.

## Worker boundary

The worker refuses execution without an AppContainer token, zero token capabilities and enclosing Job. No files or helper paths are supplied by the asset. Strict native decoders and the pinned built-in JSON schemas run in that worker; no nested schema subprocess is attempted under the one-process Job. A private callback overload shares the unchanged ingestion algorithm; the existing `SchemaClient` entry point remains the default for all old callers.

The diagnostic envelope is 32 little-endian bytes: magic `ASV1` (u32), version1 (u32), compressed length (u32), FNV32 correlation checksum (u32), caller nonce (u64), reserved zero (u64), then exactly length bytes and EOF. This checksum is corruption/correlation detection, not authentication. Input is capped at **16 MiB** for this experimental probe, below the native 256 MiB ceiling; this is not a public profile promise. The worker resource cap is 256 MiB under the unchanged OS cap. All other native parsing ceilings remain unchanged. The helper releases its immutable model before destroying its resource context.

A reply is exactly32 bytes: same magic/version, status (u32), zero (u32), echoed nonce (u64), zero (u64). Allowed statuses: OK, BAD_ASSET, OUT_OF_BUDGET, UNSUPPORTED, INTERNAL. Valid status/nonce/header and actual successful worker/cleanup evidence are all required. This contains no reusable model data, IDs, authority or rendering output. Framing failure produces no reply and fails the worker.

## Requirements and tests

| ID | Scope | Acceptance |
| --- | --- | --- |
| REQ60-P1 | Isolation | Direct worker launch rejected; valid generated native 2D/3D/dual decoded under real AppContainer, without nested process |
| REQ60-P2 | Transport | Bad magic/version/length/reserved/checksum, truncation and trailing bytes rejected; nonce mismatch and malformed replies rejected independently |
| REQ60-P3 | Containment | Dedicated helper copy; actual job empty and profile absent per case; production helper exposes no fault modes; broker cap/failure probes |
| REQ60-P4 | Decoder parity | Entire generated native bundle corpus matches its pinned expected statuses; caller-path or private-model import absent |
| REQ60-P5 | Compatibility | Existing native bundle/schema/core/security contracts remain valid; default build and installed SDK stay feature-off |

Implementation, fixture runs and alpha integration are AUTO_WITHIN_SCOPE under the continuing implementation request. The shared callback refactor is AUTO_IF_PARITY_PROVEN. Tests use generated fixtures only. Two build corrections per distinct error are allowed; OS isolation/cleanup failures stop the run with retained proof rather than blind retry. Independent design and exact-diff review precede delivery. Local evidence is primary, CI is a cross-check. No success of this slice closes G00/G01/G02/G10 or issue60.
