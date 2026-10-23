# Alpha41: Windows public asset/build qualification

2026-10-23, Asia/Bangkok. Requirement: [issue41](https://github.com/boxs-51/Avatar_Runtime/issues/41),
bounded by the [A41 contract](../../in-progress/ALPHA_41_ASSET_BUILD_QUALIFICATION.md).
Baseline alpha `006f36689d1c432659925ea2f9abe865a7825831`; branch
`dev/alpha-41-asset-build-qualification`. Production sources, ABI20 header,
default flags, frozen corpus and historical receipts are unchanged. This is a
completed Windows experiment, **not completion of issue41 or G01/G02**.

## Direct local observations

The source-stable run returned `WINDOWS_SLICE_PASS`, exit0, all45 commands exit0.
It hashed161 input files before/after and required exact equality. Dependencies
came from the existing local cache, with fetching disabled. Only generated
public fixtures were used; no Studio or private model inputs.

| Windows MSVC x64 configuration | Installed public C11 corpus | Native upload observation | Public lifecycle | Public WARP render/drain |
| --- | --- | --- | --- | --- |
| Static Release | PASS90 | PASS90 | PASS2D/3D/dual | PASS |
| Static Debug | PASS90 | PASS90 | PASS2D/3D/dual | PASS |
| Shared Release | PASS90 | PASS90 | PASS2D/3D/dual | PASS |
| Shared Debug | PASS90 | PASS90 | PASS2D/3D/dual | PASS |

Each corpus leg has12 OK,4 UNSUPPORTED,67 BAD_ASSET,7 OUT_OF_BUDGET results,
matching the committed independent manifest, in exact order. Bundles cover
embedded ZIP/JSON/PNG/GLB faults. Standalone raw GLB/PNG fixtures are excluded
because public MEMORY_BUNDLE accepts bundles. This is90 fixed fixtures, not an
arbitrary-model compatibility claim.

The consumer uses the installed header/library through `find_package` after
physical SDK relocation. It runs from a separate deployment with hostile cwd,
PATH and worker-environment decoys. Shared exports are checked exactly. Source
buffers are overwritten/freed immediately after load. Rejected handles must
be all-zero; successful handles are released and rejected as stale on reuse.
The accepted model remains usable to create another character; model release
while its original character lives also succeeds. Final close/drain must
finish with allocator live bytes zero.

The accepted baseline has nondefault layer opacity. Its image must differ from
neutral and contain visible pixels; every corpus admission is followed by an
exact64x64 RGBA comparison **before another update**. This proves preservation
of the accepted model/configuration and rendered image under the tested calls.
It does **not** prove nonneutral committed motion-pose preservation: this
fixture has zero vertex deltas. That additional integrated proof remains open.

The separate native executable attaches the existing `PublicD3D11Hooks` creation
probe to the actual public host. Every public load, including rejects, must
leave its counter unchanged. Real subsequent rendering must increment it as a
positive control. This establishes no renderer resource-creation attempt in
the observed admission path. It is not driver-wide upload telemetry. The
installed consumer has no private header or observation export; production
ABI/runtime code is unchanged. Corpus hashes are rechecked after each installed
and native run; concurrent corpus tampering was not simulated.

The unchanged `verify_installed_sdk.py` separately returned
`INSTALLED_ABI20_RELOCATED_STATIC_SHARED_PASS cases=4` for default feature-OFF
static/shared Release/Debug, including installed C11/C++20 consumers and package
negative controls. No feature is enabled by default by this change.

Fresh static Release CTest ran `cpu_public_api`, `cpu_public_c11`,
`cpu_public_cpp20`, `cpu_custody_owned`, `cpu_custody_orphan`: PASS5/5,18.23 seconds,
exit0. This includes the existing allocator/lifecycle/custody negatives without
changing their oracles. Full Python contract discovery: PASS183,126.817 seconds,
exit0, including five new verifier negative-oracle tests. Those reject missing,
duplicate or wrong-status cases, incomplete/nonzero-live final summaries, empty
or duplicate corpus and same-size byte/hash drift. Their success does not stand
in for the actual native corpus runs above.

## Reproduction and local evidence custody

From this branch, using the Windows MSVC toolchain:

```powershell
python scripts/verify_asset_runtime_qualification.py --evidence E:/Avatar_Runtime/build/alpha41-evidence --build-root E:/Avatar_Runtime/build/a41 --dependency-cache E:/Avatar_Runtime/build/c-deps
python scripts/verify_installed_sdk.py --work-root E:/Avatar_Runtime/build/alpha41-featureoff --configs Release Debug
python -m unittest discover -s tests/contracts -p 'test_*.py'
```

Primary detailed local receipt:
`E:/Avatar_Runtime/build/alpha41-evidence/1bcb7c2511034a54b813720b2ebde1b6/evidence.json`.
It includes all720 case observations,45 command logs/hashes, installed/deployed
file hashes and161 source hashes. SHA256:
`9f58de0a1c27ee1aff68d962665eccef511f009b2beee473c7be4c7df4f2ff1b`.
Canonical source-map digest (sorted JSON, compact separators):
`515db7bc0ab2a2eba2456210798dadc539ae6b8994c05b39886740628f8d7994`.

Feature-OFF receipt: `E:/Avatar_Runtime/build/alpha41-featureoff/evidence.json`,
SHA256 `b7c1a7d49846d41560968e55b04d9f1708b8eff4c6119dde38258d52c67b6a74`.
These raw receipts are retained locally in ignored build storage; they are not
GitHub artifacts. The table/pins here and the reproducible runner form the
versioned delivery; exact commit/PR custody is recorded on issue41.
Native regression log: `E:/Avatar_Runtime/build/alpha41-native-test.log`, SHA256
`330548228747e7d33da30d3f37b73ec6fcc383466a32623623fc096465e4cbd7`.
Contract log: `E:/Avatar_Runtime/build/alpha41-contracts.log`; PowerShell records
unittest stderr as NativeCommandError text, but the actual process exit is0 and
the final unittest verdict is `Ran183 tests ... OK`.

| Source input | SHA256 |
| --- | --- |
| CMakeLists.txt | `255fe9abcef3f0f435931fc2f148b2d723f84185ea9dd34285596b19108b6b2e` |
| scripts/verify_asset_runtime_qualification.py | `9a85252cf4a7aa94db94cec9c7c688c75ead01839a6d10a3abeee0a5a049777f` |
| tests/abi/asset_qualification_consumer.c | `dd225c276103700059f63c46cdfa3c5dc0e7f100d825c7bb93ff5b5821d25483` |
| tests/rendering/public_asset_upload_probe.cpp | `7f7ec53808dab453a4ebe28583db959b50f4b586bf7d1f8bfd8d3c10495ec68c` |
| Unchanged tests/assets/fixtures/manifest.json | `fa2da91c5414ac89db1a7ba7c70798c9db8f3b15e7ac94431fd55df3fc0b53e4` |

| Configuration | Installed qualification.exe SHA256 | Native probe SHA256 |
| --- | --- | --- |
| Static Release | `08e3d7de4d21fd26f8785b903bc5f1948415f69952f10349af3058e276f88a96` | `1bdcd37ad09dbae56a1a1dbcfbc979b8b8ce4f2d5866554eb6640859f5b4cae9` |
| Static Debug | `b0955d9fea2f80aea682cb16fc0695bf3fa44598f860d181d6d3e25b0574d6de` | `60144b051526d42c80c1324bcdec41572336bcda5f858c4192efda45ea44f4da` |
| Shared Release | `e50ba187046ea7956a259412021fe9dab038d4c530b0c92057b551d53bc95baa` | `02c97c5c05a09075a80cb2d637714f2d512462efbe6a54a24735e6e39f9a5742` |
| Shared Debug | `b5c4132a1c41094a712e88bf480e53679f6cb94dd5828d9d4d86d0d14a9d242d` | `0e329a81405cdc258afb0e426d22e827d2f09e0bb3347e9c026b1937d114c8e6` |

Earlier preflight `b4b7e362e355470e9d8c6ed6bd7f4246` correctly FAILED
`SOURCE_CHANGED` while tests were strengthened. Its per-leg output is excluded
from acceptance. No receipt was rewritten into a success.

## Historical CI corroboration, not fresh local fuzz

The source-associated [M1-C2 run39687799148](https://github.com/boxs-51/Avatar_Runtime/actions/runs/39687799148)
was rechecked live. Both full campaigns succeeded after separate61-second
smokes: [arschar](https://github.com/boxs-51/Avatar_Runtime/actions/runs/39687799148/job/119526771217)
12547735 runs/3601 seconds;
[GLB](https://github.com/boxs-51/Avatar_Runtime/actions/runs/39687799148/job/119526771362)
45666229 runs/3601 seconds. Logs checkout PR76 merge `a5d1b13` and identify
reviewed source `150888b59bd0be43579083478a754c6bfbf5d6ff`.
That reviewed source and alpha006f366 have identical trees. This branch changes
no parser, fixture/oracle, fuzz workflow or dependency; its CMake addition is
only a D3D11 opt-in test executable, outside those fuzz configurations. Reuse
is historical corroboration of the unchanged parser closure, not a newly run
hour campaign or full #61 qualification. The raw Windows receipt conservatively
lists fuzz/review reconciliation pending; this report supplies that later
assessment without mutating the receipt.

## Gate disposition and next work

| Requirement/gate | Disposition |
| --- | --- |
| A41-1 Windows installed profiles | PASS for the four opt-in and four default-OFF configurations above |
| A41-2 public fixture admission | PASS fixed Windows90-bundle corpus, each profile/configuration |
| A41-3 preservation/lifetime | PASS tested model/config/pixels/handles and allocator/close; nonneutral motion-pose proof UNVERIFIED |
| A41-4 pre-GPU/render/drain | PASS existing renderer-creation seam with positive control plus public WARP; driver-wide telemetry not claimed |
| A41-5 custody | Source-stable local receipts and exact11-file scope; commit/PR reconciliation recorded on issue41 at publication |
| A41-6 fuzz/ABI/security | Historical full parser campaigns retained; exact installed exports checked; whole87 security qualification remains #42 |
| G01 overall | HOLD: no actual local Linux GCC/Clang execution; historical private/core Linux CI is corroboration only |
| G02 overall | HOLD: Linux public admission/isolation is a code gap, and nonneutral motion-pose rollback proof is incomplete |
| #40/#42/#61/#62 | Remain separate; no physical loss, whole security, performance/platform or release closure |

Independent reviewer `/root/f2_independent_review` performed read-only source,
documentation and receipt reconciliation. Bounded Windows review PASS, no new
P0-P2 findings: verified161 current source hashes,45 command/log receipts,
installed/deployed hashes, four complete corpus legs and exact11-file scope.
It inspected existing native5/5 and contracts183 results; it did not execute
native workloads. This is separate review, not a second independent execution.

Next: freeze a Linux public admission/worker isolation design with explicit
platform and render capability semantics, then implement and run actual Linux
GCC/Clang installed consumers. In parallel, design an observable nonzero-motion
pose rollback fixture without weakening the frozen90-bundle oracle. No Linux
N/A waiver, OS installation, GPU reset, main merge or shipping activation occurs
in this Windows slice. The bounded Windows task is PASS. Overall #41 is REVISE
because required Linux public activation is still missing; its unrun platform
checks and nonneutral-pose proof are UNVERIFIED. Operational G01/G02 decisions
remain HOLD, and the issue stays open even when this branch's bounded tests pass.
