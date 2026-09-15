# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
foreach(mode RANGE 0 2)
    execute_process(COMMAND "${CONTROL}" "${mode}" RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 5)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ordinary UBSan control ${mode} failed: ${output} ${error}")
    endif()
    execute_process(COMMAND "${PROBE}" "${mode}" RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 5)
    if(mode EQUAL 0)
        set(reason "runtime error: unsigned integer overflow")
    else()
        set(reason "runtime error: implicit conversion")
    endif()
    if(NOT result MATCHES "^[1-9][0-9]*$" OR NOT error MATCHES "${reason}")
        message(FATAL_ERROR "Integer probe ${mode} did not stop for the required reason: ${output} ${error}")
    endif()
endforeach()
message(STATUS "Additional integer checks stop all three faults accepted by ordinary UBSan")
