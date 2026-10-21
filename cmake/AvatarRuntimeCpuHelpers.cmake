# Explicit CPU-profile consumer deployment. Helpers are fixed imported SDK
# targets; callers cannot substitute asset/env/PATH/cwd parser executables.
function(ars_copy_cpu_helpers target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "ars_copy_cpu_helpers requires an existing executable target")
    endif()
    if(NOT MSVC)
        message(FATAL_ERROR "The installed Windows CPU profile requires its matching MSVC CRT")
    endif()
    target_compile_definitions("${target}" PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    set_property(TARGET "${target}" PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:ARS::CpuAdmissionWorker>" "$<TARGET_FILE_DIR:${target}>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:ARS::CpuModelBroker>" "$<TARGET_FILE_DIR:${target}>")
    get_target_property(runtime_type ARS::RuntimeCore TYPE)
    if(runtime_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:ARS::RuntimeCore>" "$<TARGET_FILE_DIR:${target}>")
    endif()
endfunction()
