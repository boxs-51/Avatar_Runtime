cmake_minimum_required(VERSION 3.21)
project(ars_moc3_i0_probe LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "The I0 private probe is currently Windows-only")
endif()
if(NOT DEFINED PURISM_CORE_ROOT OR NOT EXISTS "${PURISM_CORE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Set PURISM_CORE_ROOT to a locally pinned Purism Core source checkout")
endif()
find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${PURISM_CORE_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE purism_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE purism_git_result)
if(NOT purism_git_result EQUAL 0 OR
   NOT purism_revision STREQUAL "1069334965522df5d0b791e97f01e26b19c45456")
    message(FATAL_ERROR "I0 probe requires Purism Core v1.1.0 at 1069334965522df5d0b791e97f01e26b19c45456")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(PURISM_CORE_ABI v6 CACHE STRING "" FORCE)
set(PURISM_CORE_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(PURISM_CORE_BUILD_VIEWER OFF CACHE BOOL "" FORCE)
set(PURISM_CORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${PURISM_CORE_ROOT}" purismcore)

add_executable(ars_moc3_i0_probe moc3_i0_probe.c)
target_compile_features(ars_moc3_i0_probe PRIVATE c_std_11)
target_link_libraries(ars_moc3_i0_probe PRIVATE PurismCore::PurismCore)
