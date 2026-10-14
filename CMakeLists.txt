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

# Declaration-only v0.1 candidate: 21 symbolic exports, NO engine behavior.
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
        target_compile_options(ars_contract_r5_model PRIVATE /W4 /WX)
    else()
        target_compile_options(ars_contract_r5_model PRIVATE -Wall -Wextra -Werror)
    endif()

    add_test(NAME prep_abi_c11 COMMAND ars_contract_smoke_c)
    add_test(NAME prep_abi_cpp20 COMMAND ars_contract_smoke_cpp)
    add_test(NAME prep_r5_state_machine COMMAND ars_contract_r5_model)
    add_test(NAME prep_abi21_c11 COMMAND ars_abi_design_smoke_c)
    add_test(NAME prep_abi21_cpp20 COMMAND ars_abi_design_smoke_cpp)
    set_tests_properties(prep_abi21_c11 prep_abi21_cpp20
        PROPERTIES LABELS "contract-prep;NON_PRODUCTION")
    set_tests_properties(prep_abi_c11 prep_abi_cpp20 prep_r5_state_machine
        PROPERTIES LABELS "contract-prep;NON_PRODUCTION")
endif()

message(STATUS "ARS PREP-ONLY: nonfunctional ABI stub and contract model, NOT SDK engine")
