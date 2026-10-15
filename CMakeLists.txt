cmake_minimum_required(VERSION 3.22)

# PREP ONLY: NO Avatar Runtime SDK production engine.
project(AvatarRuntimeContractPrep VERSION 0.1.0 LANGUAGES C CXX)

option(ARS_PREP_ONLY "Nonshipping contract tests only; never production" ON)
option(ARS_ENABLE_PREP_TESTS "Enable CTest contract checks" ON)
if(NOT ARS_PREP_ONLY)
    message(FATAL_ERROR "No production SDK is implemented. Do not disable ARS_PREP_ONLY.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# NONFUNCTIONAL stub: every API returns UNSUPPORTED; never install or package.
add_library(ars_contract_test_stub SHARED tests/contracts/p0_stub_export.c)
set_target_properties(ars_contract_test_stub PROPERTIES OUTPUT_NAME ars_p0_contract)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(ars_contract_test_stub PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

if(MSVC)
    target_compile_options(ars_contract_test_stub PRIVATE /W4 /WX)
else()
    target_compile_options(ars_contract_test_stub PRIVATE -Wall -Wextra -Werror)
endif()

# Declaration-only v0.1 candidate: 20 symbolic exports, NO engine behavior.
add_library(ars_abi_design_stub SHARED tests/abi/design_stub.c)
target_include_directories(ars_abi_design_stub PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(ars_abi_design_stub PRIVATE ARS_BUILD_SHARED=1)
set_target_properties(ars_abi_design_stub PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
if(MSVC)
    target_compile_options(ars_abi_design_stub PRIVATE /W4 /WX)
else()
    target_compile_options(ars_abi_design_stub PRIVATE -Wall -Wextra -Werror)
endif()

if(ARS_ENABLE_PREP_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)
    add_executable(ars_contract_smoke_c tests/contracts/p0_header_smoke.c)
    add_executable(ars_contract_smoke_cpp tests/contracts/p0_header_smoke.cpp)
    add_executable(ars_contract_r5_model tests/contracts/p0_r5_state_machine_model.cpp)
    add_executable(ars_abi_design_smoke_c tests/abi/design_smoke.c)
    add_executable(ars_abi_design_smoke_cpp tests/abi/design_smoke.cpp)
    foreach(abi_target IN ITEMS ars_abi_design_smoke_c ars_abi_design_smoke_cpp)
        target_compile_definitions(${abi_target} PRIVATE ARS_USE_SHARED=1)
        target_link_libraries(${abi_target} PRIVATE ars_abi_design_stub)
        if(MSVC)
            target_compile_options(${abi_target} PRIVATE /W4 /WX)
        else()
            target_compile_options(${abi_target} PRIVATE -Wall -Wextra -Werror)
        endif()
    endforeach()

    foreach(tgt IN ITEMS ars_contract_smoke_c ars_contract_smoke_cpp)
        target_compile_definitions(${tgt} PRIVATE ARS_P0_USE_SHARED=1)
        target_link_libraries(${tgt} PRIVATE ars_contract_test_stub)
        if(MSVC)
            target_compile_options(${tgt} PRIVATE /W4 /WX)
        else()
            target_compile_options(${tgt} PRIVATE -Wall -Wextra -Werror)
        endif()
    endforeach()

    target_link_libraries(ars_contract_r5_model PRIVATE Threads::Threads)
    if(MSVC)
        # Keep the historical model's assert oracles active in ASan release builds.
        target_compile_options(ars_contract_r5_model PRIVATE /W4 /WX /UNDEBUG)
    else()
        target_compile_options(ars_contract_r5_model PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()

    add_test(NAME prep_abi_c11 COMMAND ars_contract_smoke_c)
    add_test(NAME prep_abi_cpp20 COMMAND ars_contract_smoke_cpp)
    add_test(NAME prep_r5_state_machine COMMAND ars_contract_r5_model)
    add_test(NAME prep_abi20_c11 COMMAND ars_abi_design_smoke_c)
    add_test(NAME prep_abi20_cpp20 COMMAND ars_abi_design_smoke_cpp)
    set_tests_properties(prep_abi20_c11 prep_abi20_cpp20
        PROPERTIES LABELS "contract-prep;NON_PRODUCTION")
    set_tests_properties(prep_abi_c11 prep_abi_cpp20 prep_r5_state_machine
        PROPERTIES LABELS "contract-prep;NON_PRODUCTION")
endif()

message(STATUS "ARS PREP-ONLY: nonfunctional ABI stub and contract model, NOT SDK engine")

# Separate opt-in build foundation. Never rename/reuse the historical PREP stub.
option(ARS_BUILD_FOUNDATION "Build bounded M1-A ABI foundation; no runtime engine" OFF)
option(ARS_ENABLE_FOUNDATION_SANITIZERS "ASan/UBSan on foundation CPU targets" OFF)
if(ARS_BUILD_FOUNDATION)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "M1-A foundation supports x64 builds only")
    endif()
    if(ARS_ENABLE_FOUNDATION_SANITIZERS AND (MSVC OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"))
        message(FATAL_ERROR "Foundation sanitizer evidence currently requires Linux GCC/Clang")
    endif()
    include(cmake/ars_compiler_options.cmake)
    add_library(ars_foundation src/abi/abi_version.cpp src/abi/unsupported.cpp)
    add_library(ARS::Foundation ALIAS ars_foundation)
    target_include_directories(ars_foundation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_features(ars_foundation PRIVATE cxx_std_20)
    set_target_properties(ars_foundation PROPERTIES
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(ars_foundation PRIVATE ARS_BUILD_SHARED=1 INTERFACE ARS_USE_SHARED=1)
    endif()
    ars_foundation_options(ars_foundation)
    enable_testing()
    add_executable(ars_foundation_contract tests/abi/foundation_contract.c)
    add_executable(ars_foundation_cpp tests/abi/foundation_cpp.cpp)
    foreach(tgt IN ITEMS ars_foundation_contract ars_foundation_cpp)
        target_link_libraries(${tgt} PRIVATE ARS::Foundation)
        ars_foundation_options(${tgt})
    endforeach()
    add_test(NAME foundation_bounded_contract COMMAND ars_foundation_contract)
    add_test(NAME foundation_cpp20 COMMAND ars_foundation_cpp)
    if(BUILD_SHARED_LIBS)
        if(MSVC)
            get_filename_component(compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            find_program(ARS_EXPORT_TOOL NAMES dumpbin HINTS "${compiler_dir}" REQUIRED)
        else()
            set(ARS_EXPORT_TOOL "${CMAKE_NM}")
        endif()
        add_test(NAME foundation_exact_exports COMMAND "${CMAKE_COMMAND}"
            "-DARS_LIBRARY=$<TARGET_FILE:ars_foundation>" "-DARS_EXPORT_TOOL=${ARS_EXPORT_TOOL}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ars_verify_exports.cmake")
    endif()
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/foundation-$<CONFIG>.cmake" CONTENT
"set(ARS_FOUNDATION_LIBRARY \"$<TARGET_LINKER_FILE:ars_foundation>\")
set(ARS_FOUNDATION_INCLUDE \"${CMAKE_CURRENT_SOURCE_DIR}/include\")
set(ARS_FOUNDATION_RUNTIME_DIR \"$<TARGET_FILE_DIR:ars_foundation>\")
set(ARS_FOUNDATION_SHARED ${BUILD_SHARED_LIBS})
set(ARS_FOUNDATION_SANITIZERS ${ARS_ENABLE_FOUNDATION_SANITIZERS})
")
    message(STATUS "M1-A BUILD FOUNDATION: ABI version + bounded UNSUPPORTED, NOT a working runtime")
endif()

# B1 is independent of the pre-runtime foundation and never enables sources/GPU.
option(ARS_BUILD_RUNTIME_CORE "Build CPU runtime and private B2 authority components" OFF)
set(ARS_RUNTIME_SANITIZER "NONE" CACHE STRING "B1 sanitizer: NONE, ADDRESS or THREAD")
set_property(CACHE ARS_RUNTIME_SANITIZER PROPERTY STRINGS NONE ADDRESS THREAD)
if(ARS_BUILD_RUNTIME_CORE)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "B1 supports x64 only")
    endif()
    if(NOT ARS_RUNTIME_SANITIZER MATCHES "^(NONE|ADDRESS|THREAD)$")
        message(FATAL_ERROR "Unknown B1 sanitizer configuration")
    endif()
    if(MSVC AND ARS_RUNTIME_SANITIZER STREQUAL "THREAD")
        message(FATAL_ERROR "MSVC TSan is not a supported B1 configuration")
    endif()
    find_package(Threads REQUIRED)
    include(cmake/ars_compiler_options.cmake)
    add_library(ars_runtime_components OBJECT src/core/handle_registry.cpp src/core/runtime_lifecycle.cpp
        src/core/character_authority.cpp src/security/security_clock.cpp src/security/token_ledger.cpp
        src/security/admission_gate.cpp src/security/ingress_queue.cpp src/security/source_authority.cpp)
    if(WIN32)
        target_sources(ars_runtime_components PRIVATE src/security/clock_windows.cpp src/security/os_entropy_windows.cpp)
        target_link_libraries(ars_runtime_components PUBLIC bcrypt)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_sources(ars_runtime_components PRIVATE src/security/clock_linux.cpp src/security/os_entropy_linux.cpp)
    else()
        message(FATAL_ERROR "B2 security backends support Windows and Linux only")
    endif()
    target_include_directories(ars_runtime_components PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/core")
    set_target_properties(ars_runtime_components PROPERTIES POSITION_INDEPENDENT_CODE ON
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
    target_link_libraries(ars_runtime_components PUBLIC Threads::Threads)
    ars_runtime_options(ars_runtime_components)
    add_library(ars_runtime_core src/abi/abi_version.cpp src/abi/unsupported.cpp src/abi/runtime_api.cpp
        $<TARGET_OBJECTS:ars_runtime_components>)
    add_library(ARS::RuntimeCore ALIAS ars_runtime_core)
    target_include_directories(ars_runtime_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(ars_runtime_core PRIVATE Threads::Threads)
    if(WIN32)
        target_link_libraries(ars_runtime_core PRIVATE bcrypt)
    endif()
    target_compile_definitions(ars_runtime_core PRIVATE ARS_RUNTIME_CORE=1)
    target_compile_features(ars_runtime_core PRIVATE cxx_std_20)
    set_target_properties(ars_runtime_core PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(ars_runtime_core PRIVATE ARS_BUILD_SHARED=1 INTERFACE ARS_USE_SHARED=1)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            # Some libstdc++ inline declarations have explicit default visibility
            # even with hidden inlines. The unchanged exact-20 checker remains
            # the authority for the public ars_* symbol set.
            file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/ars_runtime_core.map"
                CONTENT "{ global: ars_*; local: *; };\n")
            target_link_options(ars_runtime_core PRIVATE
                "LINKER:--version-script,${CMAKE_BINARY_DIR}/ars_runtime_core.map")
        endif()
    endif()
    ars_runtime_options(ars_runtime_core)
    enable_testing()
    add_executable(ars_runtime_core_contract tests/abi/runtime_core_contract.c)
    add_executable(ars_runtime_core_cpp tests/abi/runtime_core_cpp.cpp)
    add_executable(ars_handle_registry_tests tests/security/handle_registry_tests.cpp)
    add_executable(ars_runtime_lifecycle_tests tests/security/runtime_lifecycle_tests.cpp)
    add_executable(ars_security_os_tests tests/security/security_os_tests.cpp)
    add_executable(ars_source_authority_tests tests/security/source_authority_tests.cpp)
    add_executable(ars_source_authority_race_tests tests/security/source_authority_race_tests.cpp)
    foreach(tgt IN ITEMS ars_runtime_core_contract ars_runtime_core_cpp)
        target_link_libraries(${tgt} PRIVATE ARS::RuntimeCore Threads::Threads)
    endforeach()
    set_target_properties(ars_runtime_core_contract PROPERTIES LINKER_LANGUAGE CXX)
    foreach(tgt IN ITEMS ars_handle_registry_tests ars_runtime_lifecycle_tests
            ars_security_os_tests ars_source_authority_tests ars_source_authority_race_tests)
        target_link_libraries(${tgt} PRIVATE ars_runtime_components Threads::Threads)
        target_include_directories(${tgt} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/security")
    endforeach()
    foreach(tgt IN ITEMS ars_runtime_core_contract ars_runtime_core_cpp ars_handle_registry_tests ars_runtime_lifecycle_tests
            ars_security_os_tests ars_source_authority_tests ars_source_authority_race_tests)
        target_compile_features(${tgt} PRIVATE cxx_std_20)
        ars_runtime_options(${tgt})
    endforeach()
    add_test(NAME b1_public_c COMMAND ars_runtime_core_contract)
    add_test(NAME b1_public_cpp_races COMMAND ars_runtime_core_cpp)
    add_test(NAME b1_registry_components COMMAND ars_handle_registry_tests)
    add_test(NAME b1_lifecycle_components COMMAND ars_runtime_lifecycle_tests)
    add_test(NAME b2_native_security_os COMMAND ars_security_os_tests)
    add_test(NAME b2_source_authority COMMAND ars_source_authority_tests)
    add_test(NAME b2_controlled_authority_races COMMAND ars_source_authority_race_tests)
    set_tests_properties(b2_native_security_os b2_source_authority b2_controlled_authority_races PROPERTIES TIMEOUT 300)
    set_tests_properties(b1_public_c b1_public_cpp_races b1_registry_components b1_lifecycle_components
        PROPERTIES TIMEOUT 180)
    if(BUILD_SHARED_LIBS)
        if(MSVC)
            get_filename_component(compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
            find_program(ARS_EXPORT_TOOL NAMES dumpbin HINTS "${compiler_dir}" REQUIRED)
        else()
            set(ARS_EXPORT_TOOL "${CMAKE_NM}")
        endif()
        add_test(NAME b1_exact_exports COMMAND "${CMAKE_COMMAND}" "-DARS_LIBRARY=$<TARGET_FILE:ars_runtime_core>"
            "-DARS_EXPORT_TOOL=${ARS_EXPORT_TOOL}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ars_verify_exports.cmake")
    endif()
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/core-$<CONFIG>.cmake" CONTENT
"set(ARS_FOUNDATION_LIBRARY \"$<TARGET_LINKER_FILE:ars_runtime_core>\")
set(ARS_FOUNDATION_INCLUDE \"${CMAKE_CURRENT_SOURCE_DIR}/include\")
set(ARS_FOUNDATION_RUNTIME_DIR \"$<TARGET_FILE_DIR:ars_runtime_core>\")
set(ARS_FOUNDATION_SHARED ${BUILD_SHARED_LIBS})
set(ARS_CONSUMER_RUNTIME_CORE ON)
set(ARS_RUNTIME_SANITIZER ${ARS_RUNTIME_SANITIZER})
")
    message(STATUS "B2 private CPU authority: public source/model/character/update/render and GPU remain unsupported")
endif()
