# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Mutate only emitted fixture manifests, never source or compiler configuration.
file(READ "${COMMANDS_FILE}" commands)
string(JSON count LENGTH "${commands}")
math(EXPR last "${count} - 1")
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")

function(check_manifest label content reason)
    set(path "${OUTPUT_DIRECTORY}/${label}.json")
    file(WRITE "${path}" "${content}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DCOMMANDS_FILE=${path}" -P "${CHECKER}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
    string(REGEX REPLACE "[ \r\n\t]+" " " error_flat "${error}")
    if(reason STREQUAL "")
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Valid manifest control ${label} was refused: ${output} ${error}")
        endif()
    elseif(result EQUAL 0 OR NOT error_flat MATCHES "${reason}")
        message(FATAL_ERROR "Manifest mutation ${label} was not refused for its required reason: ${output} ${error}")
    endif()
endfunction()

function(replace_flag target flag replacement reason)
    set(selected -1)
    foreach(index RANGE 0 ${last})
        string(JSON command GET "${commands}" ${index} command)
        if(command MATCHES "CMakeFiles/${target}\\.dir/")
            set(selected ${index})
            break()
        endif()
    endforeach()
    if(selected EQUAL -1)
        message(FATAL_ERROR "Mutation target ${target} has no emitted command")
    endif()
    string(REPLACE "${flag}" "${replacement}" changed "${command}")
    if(changed STREQUAL command)
        message(FATAL_ERROR "Mutation target ${target} lacks ${flag} before mutation")
    endif()
    string(REPLACE "\\" "\\\\" escaped "${changed}")
    string(REPLACE "\"" "\\\"" escaped "${escaped}")
    string(JSON mutant SET "${commands}" ${selected} command "\"${escaped}\"")
    string(MAKE_C_IDENTIFIER "${target}_${flag}_${replacement}" label)
    check_manifest("${label}" "${mutant}" "${reason}")
endfunction()

foreach(target zcl_wallet_core zcl_hash_provider zcl_blake2_provider zcl_qr_provider
    zcl_scan_provider zcl_json_provider secp256k1 secp256k1_precomputed)
    replace_flag("${target}" "-fsanitize=fuzzer-no-link" "" "Required fuzz coverage instrumentation missing")
endforeach()
replace_flag(zcl_wallet_core "-fsanitize=address,undefined" "" "Required sanitizer or fail-on-finding flags missing")
replace_flag(zcl_wallet_core "-fno-sanitize-recover=all" "" "Required sanitizer or fail-on-finding flags missing")
replace_flag(zcl_wallet_core
    "-fsanitize=unsigned-integer-overflow,implicit-integer-truncation,implicit-integer-sign-change"
    ""
    "Required authored integer checks missing")
# A positive flag earlier in a command must not conceal an overriding opt-out.
# Mutate emitted manifests only; never compile or execute a weakened target.
foreach(target zcl_wallet_core zcl_scan_provider)
    foreach(override -fno-sanitize=address -fno-sanitize=undefined -fno-sanitize=all
        -fno-sanitize=unsigned-integer-overflow -fsanitize-recover=address -fsanitize-recover=all
        -fno-sanitize-coverage=inline-8bit-counters "\"-fno-sanitize=address\"" "'-fsanitize-recover=all'")
        replace_flag("${target}" "-fno-sanitize-recover=all" "-fno-sanitize-recover=all ${override}"
            "Sanitizer or coverage opt-out is forbidden")
    endforeach()
    replace_flag("${target}" "-fno-sanitize-recover=all"
        "-fno-sanitize-recover=all -DZCL_TEST_LABEL='-fno-sanitize=address'" "")
endforeach()
check_manifest(empty "[]" "No authored/provider compile commands were checked")
check_manifest(unrelated "[{\"file\":\"/fixture/test.c\",\"command\":\"cc /fixture/test.c\"}]"
    "No authored/provider compile commands were checked")
message(STATUS "All sanitizer/coverage/empty-scope manifest mutations refused")
