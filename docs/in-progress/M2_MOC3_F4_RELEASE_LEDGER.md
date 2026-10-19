# F4 product decision and remaining evidence

**2026-10-19, Asia/Bangkok. F4-A integration, private continuity/installed consumer, bounded Windows parser-preflight and static PNG/Physics3 admission: PASS in their stated scopes. Whole F4 public product/release: HOLD.** This ledger does not rewrite historical baseline gate verdicts. Parent commit is `69eac34`; the newest static slice has its own [source closure](../reference/audits/M2_MOC3_F4_PACKAGE_ADMISSION_SOURCE.json) and [acceptance report](../completed/audits/M2_MOC3_F4_PACKAGE_ADMISSION_ACCEPTANCE.md). Earlier [completion manifest](../reference/audits/M2_MOC3_F4_COMPLETION_SOURCE.json), [completion progress](../reference/audits/M2_MOC3_F4_COMPLETION_PROGRESS.md) and [F4-A local report](../completed/audits/M2_MOC3_F4_LOCAL_ACCEPTANCE.md) remain historical evidence. The owner subsequently authorized committing and pushing completed code; this does not authorize merge, public activation or physical GPU reset. Verification reports retain their historical pre-commit source snapshots.

## Gate disposition

| Gate | New direct F4 contribution | Remaining qualification for this optional product path |
| --- | --- | --- |
| G00 contract | Frozen private core mapping, candidate-copy/final authorization fence, physics/host commit order, ownership and failure quarantine. Independent review supports the implementation architecture. | HOLD public contract: model/capability semantics and product ABI/packaging still require a separate reviewed change. Existing material ADR/security requirements remain authoritative. |
| G01 build | Windows x64 MSVC C++20 Release consumer; actual pinned dependency and binary/source hashes; private target default OFF. | HOLD cross-platform/product build: MOC3 adapter is Windows-only; no Linux MOC3 consumer or shipping configuration proof. |
| G02 asset | Private immutable inputs feed the real consumer. Static public ZIP/manifest/PNG/MOC/Physics3 decode now runs inside AppContainer; host independently validates drawable/atlas data before GPU. | HOLD complete public import: static profile/feature subset only; original over-budget atlas input rejects. Persistent evaluator/physics and public ARSCHAR model-loading ABI remain absent. |
| G03 render2D | F2 numeric WARP/hardware pixels plus actual core-to-physics-to-MOC host frames. | Private subset proof only; public SDK consumer/layer contract needs its own integrated qualification. Author equivalence is a separate unclaimed oracle. |
| G04 render3D | Existing private dual-mode host regression runs. | MOC3 consumer selects 2D only; no new MOC3 3D capability. This does not waive baseline SDK 3D gates. |
| G05 mode | Existing atomic dual-mode host rollback/epoch tests remain green. | HOLD product switching: no public MOC3/native2D/3D representation switch or whole-package interactive replacement is supplied. |
| G06 motion | Actual authorized core source, semantic candidate, linked fixed-clock physics, release, replay rejection/reset and local persistent control with bounded input leases. | HOLD broader orchestration: private single-controller API is not motion/expression/idle arbitration or the public SDK multi-source contract. |
| G07 thread/lifetime | Core consumer owner checks, exact GPU query completion, native core publish race regressions, controlled close and whole-job cancellation. Separate ASAN core unit passes; integrated ASAN render memory acceptance fails and remains HOLD. | HOLD integrated sanitizer/platform qualification and production-host context ownership; private tests cannot close these by themselves. |
| G08 security | Existing authority negatives plus actual static whole-ZIP isolation and two-pass typed host validation before GPU; direct file/process/resource/environment/limited-loopback observations. Independent malformed output and failed-worker paths show zero GPU creation. | HOLD persistent public protocol, final authority/publication, coverage-guided fuzz and sanitizer qualification. Ordinary CORE-LIVE Job ownership only handles lifetime. Universal network attribution remains HOLD. |
| G09 performance | Bounded integration plus actual concurrent WARP/hardware five-minute warm-up and thirty-minute fixed512 continuity, two image slots, recorded timing and unchanged native-plus-owner memory cap. Detailed model-derived measurements stay ignored. | HOLD quality-tier 720p/30-FPS/fallback and retained GPU allocation qualification. The slowed fixed-clock duration proof cannot award G09. |
| G10 SDK embed | Consumer directly links existing private core components; public C/C++ contract regressions pass and public header/ABI sources are unchanged. A private allowlisted installed CLI consumer passes both render backends outside the checkout, with an external model. | HOLD actual shipping SDK embed: model/character/render entry points remain unsupported; no new C ABI, DLL+static/package discovery or installed MOC3 consumer contract. |
| G11 release | Dependency/license custody inventory, local bundle/notices and source-pinned technical evidence are prepared; model assets are excluded. | HOLD reproducible shipping artifacts/SBOM/notices, full gate packet and explicit release/activation decision. Private model inputs/frames must never be packaged. |
| D06 / D08 | Healthy device lifetime and injected controls from earlier work remain separate observations. | HOLD actual loss/recovery: disposable reset-capable Windows GPU runner still absent. No reset/TDR on this machine. |

The owner subsequently selected arbitrary public-user MOC3 input. [Completion progress](../reference/audits/M2_MOC3_F4_COMPLETION_PROGRESS.md) records actual controller/35-minute/bundle/isolation results and failed sanitizer-memory acceptance; [public pipeline contract](M2_MOC3_F4_PUBLIC_PIPELINE_DESIGN.md) lists the outstanding implementation. Parser preflight is not the public importer.

Public CI cannot execute the full evaluator path without a redistributable MOC3 fixture. This limits public qualification, not the actual authorized local result. GitHub Actions remains corroborating rather than the primary source of this local acceptance.

## Dependency and data custody

* Independent evaluator: PurismCore revision `1069334965522df5d0b791e97f01e26b19c45456` (ABI v6), built as a private static dependency. The local LICENSE and [the same revision's upstream LICENSE](https://github.com/SakuraMotion/PurismCore/blob/1069334965522df5d0b791e97f01e26b19c45456/LICENSE) declare MIT, including retention of copyright/permission notice in copies or substantial portions. This is an inventory observation, not a public activation verdict.
* The dependency checkout has local edits in its optional viewer sample only. Its explicit core source list, headers, CMake and license are checked/pinned separately; viewer/samples/tests are OFF in the adapter build and are not part of F4 integration evidence. These edits are preserved, not reset.
* Existing asset/host dependencies follow [the resource/dependency lock](../reference/M1_C_DEPENDENCY_LOCK.json). F4 does not add a new downloaded dependency. D3D11/DXGI/compiler are the existing Windows host path; Python transport/physics uses the standard library.
* Official Cubism SDK is deferred and not linked or downloaded. This independent implementation makes no author/Cubism algorithm-equivalence claim.
* The owner permits local model use and forbids public redistribution. Models, textures, input profiles, derived images/hashes/counts/measurements remain ignored under `local-assets/` and `build/`. The evidence exporter copies only source/binary/dependency metadata into public-safe manifests. Tokens stay native or in the ACL-protected transient harness descriptor.

## Next bounded decision

The next implemented slice is [PUB-02-A bounded retained evaluation](M2_MOC3_PUB02_A_RETAINED_SESSION_DESIGN.md):
one isolated evaluator processes complete parameter frames, followed by full-batch
host validation before GPU allocation. Its [local acceptance](../completed/audits/M2_MOC3_PUB02_A_ACCEPTANCE.md)
separates geometry, pixels and visible/API evidence. Physics remains
**VALIDATED_NOT_SIMULATED**; full PUB-02 and PUB-04 remain **HOLD**. This does not
close the G00/G01/G02 product qualifications above. The next slice must design
and prove persistent isolated physics/state lifetime and host authority/expiry/
cancellation before extending to public interactive publication.

Before public SDK implementation, freeze the intended supported subset, installed consumer/capability behavior and trust boundary. Then implement/qualify public model lifecycle and persistent input ownership, followed by cross-platform/package/ABI tests, parser isolation/fuzz and the required performance workload. Obtain a redistributable test fixture without changing the private model's rights. Track physical loss on a separate runner. Existing source approval or this F4-A result does not authorize advertising a feature or waiving the missing observations.
