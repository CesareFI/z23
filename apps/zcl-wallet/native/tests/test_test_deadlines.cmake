# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
if(NOT DEFINED TEST_ROOT OR NOT DEFINED CHECK_SCRIPT OR NOT DEFINED CTEST_COMMAND)
    message(FATAL_ERROR "Deadline contract requires TEST_ROOT, CHECK_SCRIPT and CTEST_COMMAND")
endif()
function(check_case name timeout expected)
    set(_directory "${TEST_ROOT}/${name}")
    file(MAKE_DIRECTORY "${_directory}")
    set(_registry "")
    if(NOT name STREQUAL "empty")
        set(_registry "add_test(deadline_fixture \"${CMAKE_COMMAND}\" -E sleep 0)\n")
    endif()
    if(NOT timeout STREQUAL "unset")
        string(APPEND _registry "set_tests_properties(deadline_fixture PROPERTIES TIMEOUT ${timeout})\n")
    endif()
    file(WRITE "${_directory}/CTestTestfile.cmake" "${_registry}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DTEST_DIR=${_directory}"
        "-DCTEST_COMMAND=${CTEST_COMMAND}" -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error TIMEOUT 10)
    if(expected STREQUAL "pass")
        if(NOT _result STREQUAL "0" OR NOT _output MATCHES "Verified positive execution deadlines for 1 registered tests")
            message(FATAL_ERROR "${name} control failed: ${_result}: ${_output}${_error}")
        endif()
    elseif(_result STREQUAL "0" OR NOT "${_output}${_error}" MATCHES "${expected}")
        message(FATAL_ERROR "${name} mutation did not refuse as expected: ${_result}: ${_output}${_error}")
    endif()
endfunction()

check_case(empty unset "no registered tests")
check_case(missing unset "no explicit execution deadline")
check_case(zero 0 "invalid execution deadline")
check_case(negative -1 "invalid execution deadline")
check_case(positive 1 pass)

# Qualify CTest's actual termination separately from metadata inspection.
# Both the child and this runner have outer bounds if the property regresses.
set(_probe "${TEST_ROOT}/termination")
file(MAKE_DIRECTORY "${_probe}")
file(WRITE "${_probe}/CTestTestfile.cmake"
    "add_test(deadline_probe \"${CMAKE_COMMAND}\" -E sleep 5)\nset_tests_properties(deadline_probe PROPERTIES TIMEOUT 1)\n")
execute_process(COMMAND "${CTEST_COMMAND}" --test-dir "${_probe}" --output-on-failure
    RESULT_VARIABLE _result OUTPUT_VARIABLE _output ERROR_VARIABLE _error TIMEOUT 10)
if(NOT _result STREQUAL "8" OR NOT _output MATCHES "Timeout")
    message(FATAL_ERROR "CTest did not enforce the one-second deadline: ${_result}: ${_output}${_error}")
endif()
message(STATUS "Deadline metadata mutations, positive control and actual termination passed")
