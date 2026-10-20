# Feature-off CPU development SDK package

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
