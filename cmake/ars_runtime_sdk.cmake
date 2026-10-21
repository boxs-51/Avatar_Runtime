# Explicit development packaging only; does not enable any runtime capability.
if(NOT ARS_RUNTIME_SANITIZER STREQUAL "NONE")
    message(FATAL_ERROR "Installed development SDK requires ARS_RUNTIME_SANITIZER=NONE")
endif()
include(CMakePackageConfigHelpers)
# Fixed relative layout keeps the exported package relocatable and avoids
# accidentally installing headers or binaries from any private dependency.
set_target_properties(ars_runtime_core PROPERTIES EXPORT_NAME RuntimeCore)
install(TARGETS ars_runtime_core EXPORT AvatarRuntimeTargets
    RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/ars/ars.h" DESTINATION include/ars)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/reference/SDK_DEVELOPMENT_PACKAGE.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/in-progress/ALPHA_59_PUBLIC_ABI_CONTRACT.md"
    DESTINATION share/AvatarRuntime)
if(ARS_ENABLE_PUBLIC_CPU_ASSETS)
    set(ARS_SDK_PROFILE "ISOLATED_PUBLIC_CPU_ASSETS")
    set_target_properties(ars_cpu_admission_worker PROPERTIES EXPORT_NAME CpuAdmissionWorker)
    set_target_properties(ars_cpu_model_broker PROPERTIES EXPORT_NAME CpuModelBroker)
    install(TARGETS ars_cpu_isolation ars_cpu_model_packet ars_typed_glb
        EXPORT AvatarRuntimeTargets ARCHIVE DESTINATION lib)
    install(TARGETS ars_cpu_admission_worker ars_cpu_model_broker
        EXPORT AvatarRuntimeTargets RUNTIME DESTINATION bin)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AvatarRuntimeCpuHelpers.cmake"
        DESTINATION lib/cmake/AvatarRuntime)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/in-progress/ALPHA_60_PUBLIC_CPU_COMPLETION_DESIGN.md"
        DESTINATION share/AvatarRuntime)
else()
    set(ARS_SDK_PROFILE "CPU_LIFECYCLE_FEATURES_OFF")
endif()
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/AvatarRuntimeConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AvatarRuntimeConfig.cmake"
    INSTALL_DESTINATION lib/cmake/AvatarRuntime)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/AvatarRuntimeConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AvatarRuntimeConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AvatarRuntimeConfigVersion.cmake"
    DESTINATION lib/cmake/AvatarRuntime)
install(EXPORT AvatarRuntimeTargets FILE AvatarRuntimeTargets.cmake
    NAMESPACE ARS:: DESTINATION lib/cmake/AvatarRuntime)
