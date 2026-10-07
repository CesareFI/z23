# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Mutate only emitted fixture manifests, never source or compiler configuration.
file(READ "${COMMANDS_FILE}" commands)
string(JSON count LENGTH "${commands}")
math(EXPR last "${count} - 1")
file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")
# Each checker reads its own immutable full manifest. Bound parallelism to two
# so adding a native test target need not exhaust the unchanged outer deadline.
file(WRITE "${OUTPUT_DIRECTORY}/CTestTestfile.cmake" "# Generated full-manifest checks.\n")
set_property(GLOBAL PROPERTY ZCL_FUZZ_MUTATION_COUNT 0)
# Parse the source manifest once. Repeated full-array JSON parsing while
# locating each mutation otherwise dominates this bounded configuration test.
foreach(index RANGE 0 ${last})
    string(JSON entry GET "${commands}" ${index})
    string(JSON command_${index} GET "${entry}" command)
    string(JSON source_${index} GET "${entry}" file)
    string(JSON output_${index} GET "${entry}" output)
endforeach()

function(check_manifest label content reason)
    get_property(ordinal GLOBAL PROPERTY ZCL_FUZZ_MUTATION_COUNT)
    math(EXPR ordinal "${ordinal} + 1")
    set_property(GLOBAL PROPERTY ZCL_FUZZ_MUTATION_COUNT "${ordinal}")
    set(path "${OUTPUT_DIRECTORY}/${ordinal}-${label}.json")
    file(WRITE "${path}" "${content}")
    file(APPEND "${OUTPUT_DIRECTORY}/CTestTestfile.cmake"
        "add_test(manifest_${ordinal} [==[${CMAKE_COMMAND}]==] [==[-DCOMMANDS_FILE=${path}]==] [==[-DCHECKER=${CHECKER}]==] [==[-DREASON=${reason}]==] -P [==[${CMAKE_CURRENT_LIST_DIR}/check_fuzz_mutation.cmake]==])\n"
        "set_tests_properties(manifest_${ordinal} PROPERTIES TIMEOUT 20)\n")
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
    string(JSON mutant SET "${commands}" ${selected} command "\"${escaped}\"")
    string(MAKE_C_IDENTIFIER "${target}_${source_filter}_${flag}_${replacement}" label)
    check_manifest("${label}" "${mutant}" "${reason}")
endfunction()

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
find_program(ctest_command NAMES ctest REQUIRED)
execute_process(COMMAND "${ctest_command}" --test-dir "${OUTPUT_DIRECTORY}"
    --parallel 2 --output-on-failure --no-tests=error
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 60)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Manifest mutation checks failed: ${output} ${error}")
endif()
message(STATUS "${output}")
message(STATUS "All sanitizer/coverage/empty-scope manifest mutations refused")
