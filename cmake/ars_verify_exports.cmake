# Inspect actual exported symbol names; signatures alone do not establish exports.
if(NOT EXISTS "${ARS_LIBRARY}" OR NOT EXISTS "${ARS_EXPORT_TOOL}")
    message(FATAL_ERROR "Missing foundation library or native export inspector")
endif()
if(WIN32)
    execute_process(COMMAND "${ARS_EXPORT_TOOL}" /exports "${ARS_LIBRARY}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
else()
    execute_process(COMMAND "${ARS_EXPORT_TOOL}" -D --defined-only "${ARS_LIBRARY}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
endif()
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Export inspection failed: ${status}: ${error}")
endif()
set(expected
    ars_get_abi_version ars_runtime_create ars_runtime_get_capabilities
    ars_model_load ars_model_release ars_character_create ars_character_destroy
    ars_character_set_mode ars_character_set_visual_layer ars_source_register
    ars_source_admin_release ars_source_rotate ars_source_revoke ars_motion_submit
    ars_runtime_update ars_runtime_render ars_runtime_begin_close ars_runtime_drain_gpu
    ars_runtime_query_close ars_runtime_finish_close)
string(REPLACE "\r" "" output "${output}")
string(REPLACE "\n" ";" lines "${output}")
set(actual)
foreach(line IN LISTS lines)
    if(WIN32)
        if(line MATCHES "^[ \t]*[0-9]+[ \t]+[0-9A-Fa-f]+[ \t]+[0-9A-Fa-f]+[ \t]+([^ \t]+)")
            list(APPEND actual "${CMAKE_MATCH_1}")
        endif()
    elseif(line MATCHES "^[0-9A-Fa-f]+[ \t]+[A-Za-z][ \t]+([^ \t]+)")
        list(APPEND actual "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(SORT actual)
list(SORT expected)
if(NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR "Foundation export drift. Expected=${expected}; actual=${actual}\n${output}")
endif()
message(STATUS "PASS BUILD-03: exact 20 unmangled public exports: ${actual}")
