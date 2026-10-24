cmake_minimum_required(VERSION 3.22)
project(AvatarStudio LANGUAGES C CXX)
if(NOT WIN32)
  message(FATAL_ERROR "Avatar Studio currently uses the Windows desktop file picker")
endif()
get_filename_component(ARS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(STUDIO_PURISM_ROOT "" CACHE PATH "Existing local Purism Core checkout")
set(STUDIO_RAYLIB_ROOT "" CACHE PATH "Existing local raylib checkout; no downloads")
if(NOT EXISTS "${STUDIO_PURISM_ROOT}/CMakeLists.txt" OR NOT EXISTS "${STUDIO_RAYLIB_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "Set STUDIO_PURISM_ROOT and STUDIO_RAYLIB_ROOT to existing local source trees")
endif()
set(PURISM_CORE_BUILD_VIEWER ON CACHE BOOL "" FORCE)
set(PURISM_CORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PURISM_CORE_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(FETCHCONTENT_SOURCE_DIR_RAYLIB "${STUDIO_RAYLIB_ROOT}" CACHE PATH "" FORCE)
# Studio owns F12 and its private output directory. Disable raylib's global
# shortcut, which otherwise writes screenshot000.png into the working directory.
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
add_subdirectory("${STUDIO_PURISM_ROOT}" purism EXCLUDE_FROM_ALL)
target_compile_definitions(raylib PRIVATE SUPPORT_SCREEN_CAPTURE=0)
get_target_property(support viewer SOURCES)
set(studio_sources "")
foreach(path IN LISTS support)
  if(path MATCHES "(viewer|render).c$")
    continue()
  endif()
  if(NOT IS_ABSOLUTE "${path}")
    set(path "${STUDIO_PURISM_ROOT}/${path}")
  endif()
  list(APPEND studio_sources "${path}")
endforeach()
set(UTF8PROC_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${ARS_ROOT}/build/c-deps/utf8proc" utf8proc EXCLUDE_FROM_ALL)
add_executable(avatar_studio studio.c studio_render.c studio_backend.cpp studio_xpbd.cpp studio_rig.c studio_layer_analysis.c ${studio_sources}
  "${ARS_ROOT}/src/moc3/retained_physics.cpp"
  "${ARS_ROOT}/src/moc3/physics_admission.cpp"
  "${ARS_ROOT}/src/validation/strict_json.cpp"
  "${ARS_ROOT}/src/validation/strict_value.cpp")
add_dependencies(avatar_studio viewer)
target_compile_features(avatar_studio PRIVATE c_std_11 cxx_std_20)
target_compile_definitions(avatar_studio PRIVATE NOMINMAX _ITERATOR_DEBUG_LEVEL=0)
target_include_directories(avatar_studio PRIVATE "${STUDIO_PURISM_ROOT}/src/samples/viewer"
  "${STUDIO_PURISM_ROOT}/src" "${CMAKE_CURRENT_BINARY_DIR}/purism" "${ARS_ROOT}/include")
target_link_libraries(avatar_studio PRIVATE PurismCore::PurismCore raylib utf8proc opengl32 gdi32 winmm comdlg32 bcrypt)
if(MSVC)
  set_source_files_properties(studio.c studio_backend.cpp PROPERTIES COMPILE_OPTIONS /W4)
endif()


# Synthetic XPBD matrix: never loads model/texture or creates a GUI window.
add_executable(studio_xpbd_parameter_tests tests/native_xpbd_parameter_tests.cpp)
target_compile_features(studio_xpbd_parameter_tests PRIVATE cxx_std_20)

# Pure local rig editor regression, no model / raylib required.
add_executable(studio_rig_tests tests/studio_rig_tests.c studio_rig.c)
target_compile_features(studio_rig_tests PRIVATE c_std_11)
target_compile_definitions(studio_rig_tests PRIVATE STUDIO_RIG_TEST_FAULT_INJECTION=1)

# Pure C deterministic layer analysis: synthetic mesh only.
add_executable(studio_layer_analysis_tests tests/studio_layer_analysis_tests.c studio_layer_analysis.c)
target_compile_features(studio_layer_analysis_tests PRIVATE c_std_11)

# G5 T1 Native in-memory transaction RED-first; no Studio ingress or model data.
add_executable(studio_g5_txn_tests tests/studio_g5_txn_tests.c studio_g5_txn.c studio_rig.c)
target_compile_features(studio_g5_txn_tests PRIVATE c_std_11)

# Additional fault, ABA and capacity oracles; synthetic input only.
add_executable(studio_g5_txn_adversarial tests/studio_g5_txn_adversarial.c studio_g5_txn.c studio_rig.c)
target_compile_features(studio_g5_txn_adversarial PRIVATE c_std_11)

# Exact typed G5 draft edit operations; no public editor writer enabled.
add_executable(studio_g5_txn_edit_ops tests/studio_g5_txn_edit_ops.c studio_g5_txn.c studio_rig.c)
target_compile_features(studio_g5_txn_edit_ops PRIVATE c_std_11)

# Local-only G5 synthetic Native tests; no model fixtures or GUI required.
enable_testing()
add_test(NAME g5_txn_core COMMAND studio_g5_txn_tests)
add_test(NAME g5_txn_adversarial COMMAND studio_g5_txn_adversarial)
add_test(NAME g5_txn_edit_ops COMMAND studio_g5_txn_edit_ops)

if(MSVC)
  foreach(target IN ITEMS studio_g5_txn_tests studio_g5_txn_adversarial studio_g5_txn_edit_ops)
    target_compile_options(${target} PRIVATE /W4)
  endforeach()
endif()

# Experimental G5 unified owner routing and recovery semantics, test only.
add_executable(studio_g5_bus_recovery_tests tests/studio_g5_bus_recovery_tests.c studio_g5_bus.c studio_g5_txn.c studio_rig.c)
target_compile_features(studio_g5_bus_recovery_tests PRIVATE c_std_11)
target_compile_definitions(studio_g5_bus_recovery_tests PRIVATE G5_TEST_ONLY_UNBOUND_BUS=1)
add_test(NAME g5_bus_recovery COMMAND studio_g5_bus_recovery_tests)

# Fixed read-only recovery message decoder, not connected to Studio IPC.
add_executable(studio_g5_recovery_wire_tests tests/studio_g5_recovery_wire_tests.c studio_g5_recovery_wire.c studio_g5_bus.c studio_g5_txn.c studio_rig.c)
target_compile_features(studio_g5_recovery_wire_tests PRIVATE c_std_11)
target_compile_definitions(studio_g5_recovery_wire_tests PRIVATE G5_TEST_ONLY_UNBOUND_BUS=1)
add_test(NAME g5_recovery_wire COMMAND studio_g5_recovery_wire_tests)

# Cryptographic receipt fingerprint primitive, model-free and cross-platform C11.
add_executable(studio_g5_sha256_tests tests/studio_g5_sha256_tests.c)
target_compile_features(studio_g5_sha256_tests PRIVATE c_std_11)
add_test(NAME g5_sha256_vectors COMMAND studio_g5_sha256_tests)

# Not included in default CTest until a physical Windows RED/GREEN run.
# The reviewed Native Studio/V2 G5 writer remains OFF.
option(STUDIO_G5_ACTOR_RED_FIRST "Build opt-in trusted actor and receipt oracle" OFF)
if(STUDIO_G5_ACTOR_RED_FIRST)
  add_executable(studio_g5_actor_receipt_redfirst
    tests/studio_g5_actor_receipt_redfirst.c
    studio_g5_bus.c studio_g5_txn.c studio_rig.c)
  target_compile_features(studio_g5_actor_receipt_redfirst PRIVATE c_std_11)
  add_test(NAME g5_actor_receipt_redfirst COMMAND studio_g5_actor_receipt_redfirst)

  # G5-ACK-05: only exact original command identity may clear quarantine.
  add_executable(studio_g5_receipt_binding_redfirst
    tests/studio_g5_receipt_binding_redfirst.c
    studio_g5_bus.c studio_g5_txn.c studio_rig.c)
  target_compile_features(studio_g5_receipt_binding_redfirst PRIVATE c_std_11)
  add_test(NAME g5_receipt_binding_redfirst COMMAND studio_g5_receipt_binding_redfirst)
endif()

# Offline-only process IPC experiment. Never link into the Studio product.
option(STUDIO_G5_IPC_FIXTURE "Build model-free G5 recovery IPC test DLL" OFF)
if(STUDIO_G5_IPC_FIXTURE)
  add_library(studio_g5_ipc_fixture SHARED
    tests/studio_g5_ipc_fixture.c studio_g5_bus.c studio_g5_txn.c studio_rig.c)
  target_compile_features(studio_g5_ipc_fixture PRIVATE c_std_11)
  # Physically separate process and read-only recovery file lane; test-only.
  find_package(Python3 QUIET COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME g5_process_recovery_fixture
      COMMAND "${Python3_EXECUTABLE}" -B
      "${ARS_ROOT}/scripts/test_studio_g5_process_recovery.py")
    set_tests_properties(g5_process_recovery_fixture PROPERTIES
      WORKING_DIRECTORY "${ARS_ROOT}" TIMEOUT 25)
  endif()
endif()
