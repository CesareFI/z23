# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# An instrumented harness alone does not instrument linked C/provider code.
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${NEGATIVE_BUILD}"
    -DCMAKE_C_COMPILER=${COMPILER} -DZCL_FUZZ=ON -DZCL_SANITIZE=OFF
    -DZCL_TLS_REVIEW=OFF -DZCL_JNI=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    RESULT_VARIABLE accepted OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 60)
string(REGEX REPLACE "[ \r\n\t]+" " " error_flat "${error}")
if(accepted EQUAL 0 OR NOT error_flat MATCHES "ZCL_FUZZ requires ZCL_SANITIZE=ON")
    message(FATAL_ERROR "Unsanitized fuzz profile was not refused for the required reason: ${output} ${error}")
endif()

# A host Clang configure also verifies the actual emitted commands, including
# every enabled authored source/provider compilation (test-only seed writers
# need not be instrumented). This is a configuration check, not a fuzz run.
if(DEFINED CLANG AND NOT CLANG STREQUAL "")
    execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${POSITIVE_BUILD}"
        -DCMAKE_C_COMPILER=${CLANG} -DZCL_FUZZ=ON -DZCL_SANITIZE=ON
        -DZCL_TLS_REVIEW=OFF -DZCL_JNI=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        RESULT_VARIABLE refused OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 60)
    if(NOT refused EQUAL 0)
        message(FATAL_ERROR "Instrumented fuzz profile did not configure: ${output} ${error}")
    endif()
    set(checker "${CMAKE_CURRENT_LIST_DIR}/check_fuzz_commands.cmake")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DCOMMANDS_FILE=${POSITIVE_BUILD}/compile_commands.json" -P "${checker}"
        RESULT_VARIABLE refused OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
    if(NOT refused EQUAL 0)
        message(FATAL_ERROR "Fuzz compile command verification failed: ${output} ${error}")
    endif()
    message(STATUS "${output}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DCOMMANDS_FILE=${POSITIVE_BUILD}/compile_commands.json" "-DCHECKER=${checker}"
        "-DOUTPUT_DIRECTORY=${POSITIVE_BUILD}/manifest-mutations"
        -P "${CMAKE_CURRENT_LIST_DIR}/test_fuzz_commands.cmake"
        RESULT_VARIABLE refused OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 60)
    if(NOT refused EQUAL 0)
        message(FATAL_ERROR "Fuzz command mutation checks failed: ${output} ${error}")
    endif()
    message(STATUS "${output}")
else()
    message(STATUS "Host Clang unavailable: positive fuzz-profile compile flags not qualified")
endif()
