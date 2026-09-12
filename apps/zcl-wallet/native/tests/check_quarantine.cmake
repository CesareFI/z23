# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Inspect whole archives: dead stripping in the final JNI .so alone is weaker.
execute_process(COMMAND "${NM}" --defined-only "${CORE}" "${HASH}"
    RESULT_VARIABLE result OUTPUT_VARIABLE symbols ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect wallet archives: ${errors}")
endif()
if(symbols MATCHES "(zcl_transport_|zcl_tls_|mbedtls_(ssl|x509|rsa|ecp|pk)_)")
    message(FATAL_ERROR "Quarantined TLS symbols entered the enabled wallet build")
endif()
if(NOT symbols MATCHES "zcl_sync_reply" OR NOT symbols MATCHES "mbedtls_sha256_update")
    message(FATAL_ERROR "Quarantine inspection did not observe required wallet/hash symbols")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SOURCE}" -B "${NEGATIVE_BUILD}"
    -DZCL_TLS_REVIEW=ON -DZCL_JNI=ON
    RESULT_VARIABLE refused OUTPUT_VARIABLE output ERROR_VARIABLE error)
string(REGEX REPLACE "[ \r\n\t]+" " " error_flat "${error}")
if(refused EQUAL 0 OR NOT error_flat MATCHES "no Android/JNI binding is permitted")
    message(FATAL_ERROR "TLS/JNI combination was not refused for the expected reason: ${output} ${error}")
endif()
