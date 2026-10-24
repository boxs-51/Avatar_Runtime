# Development SDK profiles

## Reusable SDK and non-public model information rule

All source-built SDK profiles, installed consumer examples, CI artifacts and
release archives must comply with
[ADR-005: non-public model information and SDK packaging](ADR_005_MODEL_INFORMATION_NONPUBLIC_AND_SDK_PACKAGING.md).
A public SDK profile does **not** grant permission to publish models processed
with it. Installed SDK files must not contain private models, textures, renders,
model identities, geometry/rig exports or identifying model-derived evidence.
Tests may use synthetic data or assets separately approved for public release.

Run the model-publication scanner with --staged before committing and
--ref HEAD before publishing a Git revision, and independently review the
installed SDK manifest and external build/CI logs. Also run the scanner
with --package <sdk-directory> against the installed SDK staging tree before
creating a distributable archive. The Git mode and package mode are both
bounded artifact scans; neither proves historical Git objects or arbitrary
release archives are clear. This policy controls **publication**, not legitimate local Studio
and Agent read/edit access.

Reusable components should first be delivered as independent internal libraries
with stable boundaries and synthetic consumer tests, then promoted to public
SDK packages only after API compatibility, licensing and confidentiality review.
The G5 native transaction core remains an internal test-only library; it does
not enable the G5 writer or modify ABI20.

## Default feature-off profile

This opt-in source-built package is for ABI/lifecycle development. It implements
CPU runtime create/capabilities/close only; model,character,source,motion,render
and GPU drain remain UNSUPPORTED. All capability bits are zero. Installing this
package does not qualify a release or enable public graphics/assets/motion.
The canonical header retains its historical declaration-candidate preamble;
actual behavior is specified by the issue59 profile and tested installed library.

Build an x64 Windows or Linux runtime with `ARS_BUILD_RUNTIME_CORE=ON`,
`ARS_INSTALL_RUNTIME_CORE_SDK=ON`, `ARS_RUNTIME_SANITIZER=NONE`, and choose
`BUILD_SHARED_LIBS=ON` or `OFF`. All installation options default OFF. No asset
validator,worker,private model,renderer,stub or private header is packaged.
Use `cmake --install <build> --config Release --prefix <sdk-directory>`.

An independent consumer uses `find_package(AvatarRuntime 0.1 CONFIG REQUIRED)`
and `target_link_libraries(app PRIVATE ARS::RuntimeCore)`, selecting only the
installed prefix through `CMAKE_PREFIX_PATH`. The public include is `<ars/ars.h>`.
The export supplies `ARS_USE_SHARED` for DLL imports. C source is compiled C11;
static C consumers enable CXX in their CMake project and use the C++ link driver
because the implementation uses the platform C++ runtime. No C++ objects or STL
cross the ABI. C++ consumers require C++20 only for their own optional usage.

The package is relocatable as a whole. A Windows shared host deploys the installed
`bin/ars_runtime_core.dll` beside its executable or supplies a trusted DLL lookup
path. Linux shared consumers use installed loader/RPATH policy; do not depend on
the original build directory. Build and consume matching architecture,platform,
compiler runtime and configuration; arbitrary compiler/CRT/configuration mixing
is not promised. Install each static/shared/configuration variant to its own
prefix. Package version0.1.x compatibility does not advertise runtime features;
`ars_get_abi_version()` remains1.0 and capabilities determine implemented work.

`python scripts/verify_installed_sdk.py` builds/copies standalone test consumers,
installs and physically relocates static/shared packages, then verifies real
behavior using installed files only. Detailed logs remain under build/.
Windows local evidence is primary; other toolchains/platforms need their own
observed results. This package does not close G00,G01,G02,G10 or shipping gates.

## Isolated public CPU asset profile (Windows x64)

Explicitly add `ARS_ENABLE_PUBLIC_CPU_ASSETS=ON`,
`ARS_BUILD_ASSET_VALIDATOR=ON`, `ARS_BUILD_RUNTIME_CORE=ON` and
`ARS_INSTALL_RUNTIME_CORE_SDK=ON`. The CPU asset option defaults OFF. Select a
matching static/shared Release/Debug build and the existing validated dependency
cache; build RuntimeCore plus `ars_cpu_admission_worker` and
`ars_cpu_model_broker` before installing. This profile exports
`AvatarRuntime_PROFILE=ISOLATED_PUBLIC_CPU_ASSETS`; the default profile reports
`CPU_LIFECYCLE_FEATURES_OFF`.

After linking `ARS::RuntimeCore`, call `ars_copy_cpu_helpers(app)` in consumer
CMake. It copies the fixed helpers and, for shared linkage, the runtime DLL. It
also selects the matching MSVC static CRT and iterator ABI. Preserve this
trusted adjacent deployment: static hosts resolve helpers beside the executable,
shared hosts beside the runtime DLL. The runtime ignores worker paths supplied
by assets, environment, PATH or current directory. The worker parses only through
bounded isolated IPC; no private headers or encoded parsers are linked into the
host RuntimeCore dependency closure.

This profile accepts native2D, GLB3D and dual memory bundles, retaining complete
validated CPU models. It implements public model/character/mode/layer and
source/ingress/update lifecycle with generation-bound ownership and final
authority checks. Only `ARS_CAP_HOST_SCOPED_MOTION_INGRESS` is advertised;
render/GPU drain remain UNSUPPORTED. MOC3 import/physics, production rendering,
non-Windows CPU asset packages and shipping activation are separate work.

`python scripts/verify_public_cpu_sdk.py --corpus <generated-corpus> --evidence
<local-evidence-root>` qualifies installed, physically relocated C11/C++20
consumers for all four configurations. Use generated or independently permitted
assets; no restricted sample is installed. See the installed
`ALPHA_60_PUBLIC_CPU_COMPLETION_DESIGN.md` and repository
[completion ledger](../in-progress/ALPHA_60_COMPLETION_PROGRESS.md) for scope and
source-attributed evidence. Neither profile installation is release approval.

## Opt-in public D3D11 host profile (Windows x64)

Add `ARS_ENABLE_PUBLIC_D3D11_HOST=ON` to the isolated public CPU configuration
above. It defaults OFF and requires the CPU profile. The installed package
reports `AvatarRuntime_PROFILE=OPT_IN_PUBLIC_D3D11_HOST`. Build and deploy the
same fixed CPU helpers with `ars_copy_cpu_helpers(app)`; match the static/shared
library, configuration, CRT and iterator ABI. D3D11/DXGI/compiler system libraries
are included in the exported link closure; private ARS render targets and headers
are not dependencies of the consumer.

The existing ABI20 render/drain entrypoints now support committed native2D,
GLB3D and dual rendering, HOST_DEVICE and host-scoped motion ingress in this
profile. The canonical public header and exported symbol set do not change.
MOC3 import/physics and shipping activation remain separate. The CPU-only and
default profiles keep their earlier behavior.

On R, pass a borrowed immediate context and RGBA8_UNORM DEFAULT single-mip,
single-slice, single-sample render target, with a stable nonzero device generation
and dimensions at most8192. Output is sRGB-encoded premultiplied color. Keep the
target identity stable while render returns BUSY; call render again or drain on
R to advance the exact GPU queries. SDK retained references protect in-flight
borrowed resources even if the host releases its own references. `begin_close`
may run on H; finish_close remains BUSY until R drain retires the GPU graph.
Serialize external use of the same immediate context with the documented owned
host domain. Unsupported target shapes are rejected before presentation.

The runtime captures all live characters coherently. It composes 3D with shared
depth, then 2D, and selects the first eligible GLB camera deterministically.
See the installed `ALPHA_39_COMMITTED_RENDER_DESIGN.md` for camera fallback,
configuration fencing, target epoch and loss/receipt rules. GPU losses in tests
are injected; physical recovery remains HOLD.

`python scripts/verify_public_d3d11_sdk.py --corpus <generated-corpus> --evidence
<local-evidence-root>` qualifies relocated static/shared Release/Debug packages
with standalone C11 and C++20 consumers. It exercises actual public render/drain,
100 switches/resizes, dynamic pixels and fractional LOD/mips on WARP and named
local hardware. Use short build paths on Windows. See
[the local acceptance](../completed/audits/ALPHA_39_COMMITTED_RENDER_ACCEPTANCE.md)
for observed results and the separate qualification boundaries.
