# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# One isolated full-manifest check; all result/reason predicates are retained.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DCOMMANDS_FILE=${COMMANDS_FILE}" -P "${CHECKER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
string(REGEX REPLACE "[ \r\n\t]+" " " error_flat "${error}")
if(REASON STREQUAL "")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Valid manifest control was refused: ${output} ${error}")
    endif()
elseif(result EQUAL 0 OR NOT error_flat MATCHES "${REASON}")
    message(FATAL_ERROR "Manifest mutation was not refused for its required reason: ${output} ${error}")
endif()
