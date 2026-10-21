# CPU development SDK profiles

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
