# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Mutate only emitted fixture manifests, never source or compiler configuration.
file(READ "${COMMANDS_FILE}" commands)
string(JSON count LENGTH "${commands}")
math(EXPR last "${count} - 1")
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
# Parse the source manifest once. Repeated full-array JSON parsing while
# locating each mutation otherwise dominates this bounded configuration test.
foreach(index RANGE 0 ${last})
    string(JSON entry GET "${commands}" ${index})
    string(JSON command_${index} GET "${entry}" command)
    string(JSON source_${index} GET "${entry}" file)
    string(JSON output_${index} GET "${entry}" output)
    set(entry_${index} "${entry}")
    if(NOT DEFINED core_control AND output_${index} MATCHES "^CMakeFiles/zcl_wallet_core\\.dir/")
        set(core_control "${entry}")
    endif()
    if(NOT DEFINED harness_control AND source_${index} MATCHES "/native/tests/" AND
        output_${index} MATCHES "^CMakeFiles/fuzz_[^/]+\\.dir/")
        set(harness_control "${entry}")
    endif()
endforeach()
if(NOT DEFINED core_control OR NOT DEFINED harness_control)
    message(FATAL_ERROR "Mutation fixtures require actual core and fuzz-harness controls")
endif()

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
    set(source_filter "")
    if(ARGC GREATER 4)
        set(source_filter "${ARGV4}")
    endif()
    foreach(index RANGE 0 ${last})
        set(command "${command_${index}}")
        set(source "${source_${index}}")
        if(NOT source MATCHES "${source_filter}")
            continue()
        endif()
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
    string(JSON mutated_entry SET "${entry_${selected}}" command "\"${escaped}\"")
    # The parent gate already checks the COMPLETE emitted manifest. A local
    # flag mutation needs its exact emitted entry plus valid core/harness
    # controls, not hundreds of unrelated compilations reparsed on every case.
    # Retain controls even if the target is one of them: other valid entries
    # must not conceal a malformed/incorrectly classified mutated command.
    set(mutant "[${core_control},${harness_control},${mutated_entry}]")
    string(MAKE_C_IDENTIFIER "${target}_${source_filter}_${flag}_${replacement}" label)
    check_manifest("${label}" "${mutant}" "${reason}")
endfunction()

check_manifest(mutation_controls "[${core_control},${harness_control}]" "")

foreach(target zcl_wallet_core zcl_hash_provider zcl_blake2_provider zcl_qr_provider
    zcl_scan_provider zcl_json_provider secp256k1 secp256k1_precomputed)
    replace_flag("${target}" "-fsanitize=fuzzer-no-link" "" "Required fuzz coverage instrumentation missing")
endforeach()
foreach(source_filter test_blake2.c fuzz_sync.c sync_fixture.c)
    if(source_filter STREQUAL "test_blake2.c")
        set(target fuzz_blake2)
    else()
        set(target fuzz_sync)
    endif()
    # The harness and its helpers need qualification too, even when their
    # filenames are shared with standalone tests or seed writers.
    replace_flag("${target}" "-fsanitize=fuzzer,address,undefined"
        "-fsanitize=address,undefined" "Required fuzz coverage instrumentation missing" "${source_filter}")
    replace_flag("${target}" "-fno-sanitize-recover=all" ""
        "Required sanitizer or fail-on-finding flags missing" "${source_filter}")
    replace_flag("${target}" "-fno-sanitize-recover=all"
        "-fno-sanitize-recover=all -fno-sanitize=address"
        "Sanitizer or coverage opt-out is forbidden" "${source_filter}")
    replace_flag("${target}" "-fsanitize=fuzzer,address,undefined"
        "'-fsanitize=fuzzer,address,undefined'" "" "${source_filter}")
endforeach()
replace_flag(zcl_wallet_core "-fsanitize=address,undefined" "" "Required sanitizer or fail-on-finding flags missing")
replace_flag(zcl_wallet_core "-fno-sanitize-recover=all" "" "Required sanitizer or fail-on-finding flags missing")
replace_flag(zcl_wallet_core
    "-fsanitize=unsigned-integer-overflow,implicit-integer-truncation,implicit-integer-sign-change"
    ""
    "Required authored integer checks missing")
# Text inside a macro value is not an enabling compiler argument.
foreach(flag -fsanitize=address,undefined -fno-sanitize-recover=all)
    replace_flag(zcl_wallet_core "${flag}" "-DZCL_TEST_LABEL='${flag}'"
        "Required sanitizer or fail-on-finding flags missing")
endforeach()
replace_flag(zcl_wallet_core "-fsanitize=fuzzer-no-link" "-DZCL_TEST_LABEL='-fsanitize=fuzzer-no-link'"
    "Required fuzz coverage instrumentation missing")
# A test-shaped macro/include path does not make this library object a
# standalone registered test or exempt it from coverage instrumentation.
foreach(label "-DZCL_TEST_LABEL=CMakeFiles/pretend_tests.dir/fixture.c.o"
    "'-DZCL_TEST_LABEL=CMakeFiles/pretend_tests.dir/fixture.c.o'"
    "-I/fixture/CMakeFiles/pretend_tests.dir/include")
    replace_flag(zcl_wallet_core "-fsanitize=fuzzer-no-link" "${label}"
        "Required fuzz coverage instrumentation missing")
endforeach()
replace_flag(zcl_wallet_core "-fsanitize=fuzzer-no-link"
    "-fsanitize=fuzzer-no-link -DZCL_TEST_LABEL=CMakeFiles/pretend_tests.dir/fixture.c.o" "")
replace_flag(zcl_wallet_core "-o " "-o CMakeFiles/pretend_tests.dir/fixture.c.o -o "
    "Exactly one compiler output is required")
replace_flag(zcl_wallet_core "-o " "-DZCL_TEST_OUTPUT=" "Exactly one compiler output is required")
replace_flag(zcl_wallet_core "-o " "'-o' " "")
replace_flag(zcl_wallet_core
    "-fsanitize=unsigned-integer-overflow,implicit-integer-truncation,implicit-integer-sign-change"
    "-DZCL_TEST_LABEL='-fsanitize=unsigned-integer-overflow,implicit-integer-truncation,implicit-integer-sign-change'"
    "Required authored integer checks missing")
# Quoting the actual option still passes the same compiler argument.
replace_flag(zcl_wallet_core "-fsanitize=address,undefined" "\"-fsanitize=address,undefined\"" "")
replace_flag(zcl_wallet_core "-fsanitize=fuzzer-no-link" "'-fsanitize=fuzzer-no-link'" "")
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
# These copies share source files with fuzz-related code but are not fuzz
# targets. Their instrumentation is outside this checker, not inferred from
# a filename. Manifest-only controls must remain accepted.
replace_flag(blake2_tests "-fno-sanitize-recover=all" "" "")
replace_flag(seed_scan_qr "-fno-sanitize-recover=all" "" "")
set(no_harnesses "${commands}")
foreach(index RANGE ${last} 0 -1)
    set(source "${source_${index}}")
    set(output "${output_${index}}")
    if(source MATCHES "/native/tests/" AND output MATCHES "^CMakeFiles/fuzz_[^/]+\\.dir/")
        string(JSON no_harnesses REMOVE "${no_harnesses}" ${index})
    endif()
endforeach()
check_manifest(no_harnesses "${no_harnesses}" "No fuzz harness compile commands were checked")
check_manifest(empty "[]" "No authored/provider compile commands were checked")
check_manifest(unrelated "[{\"file\":\"/fixture/test.c\",\"command\":\"cc /fixture/test.c\"}]"
    "No authored/provider compile commands were checked")
message(STATUS "All sanitizer/coverage/empty-scope manifest mutations refused")
