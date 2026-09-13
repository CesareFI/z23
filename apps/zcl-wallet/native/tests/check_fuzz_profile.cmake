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
    file(READ "${POSITIVE_BUILD}/compile_commands.json" commands)
    string(JSON count LENGTH "${commands}")
    math(EXPR last "${count} - 1")
    set(observed 0)
    foreach(index RANGE 0 ${last})
        string(JSON source GET "${commands}" ${index} file)
        if(source MATCHES "/native/src/|/vendor/android-[^/]+/|/contexts/commons/packages/")
            string(JSON command GET "${commands}" ${index} command)
            if(NOT command MATCHES "-fsanitize=address,undefined" OR NOT command MATCHES "-fno-sanitize-recover=all")
                message(FATAL_ERROR "Required sanitizer or fail-on-finding flags missing for ${source}")
            endif()
            math(EXPR observed "${observed} + 1")
        endif()
    endforeach()
    if(observed EQUAL 0)
        message(FATAL_ERROR "No authored/provider compile commands were checked")
    endif()
    message(STATUS "Verified ASan/UBSan and fail-on-finding flags on ${observed} authored/provider compilations")
else()
    message(STATUS "Host Clang unavailable: positive fuzz-profile compile flags not qualified")
endif()
