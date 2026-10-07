# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
cmake_minimum_required(VERSION 3.22)
project(zclassic_wallet VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_VISIBILITY_PRESET hidden)

# Keep Android debug paths and content-derived build IDs independent of the
# checkout location, including sources compiled by provider subdirectories.
if(ANDROID)
    get_filename_component(ZCL_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
    add_compile_options("-ffile-prefix-map=${ZCL_SOURCE_ROOT}=.")
endif()

# Reuse the existing allocation-free JSON/UTF-8 packages as C17 providers.
# Only the bounded lexer and escape checker are used, never floating-point
# conversion or Unicode display decoding. See docs/READ_ONLY_SYNC.md.
set(ZCL_COMMONS_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../contexts/commons/packages")
add_library(zcl_json_provider STATIC ${ZCL_COMMONS_SOURCE}/zjsonp/src/zjsonp.c
    ${ZCL_COMMONS_SOURCE}/zutf8/src/zutf8.c)
target_include_directories(zcl_json_provider PUBLIC ${ZCL_COMMONS_SOURCE}/zjsonp/include
    ${ZCL_COMMONS_SOURCE}/zutf8/include)
target_compile_options(zcl_json_provider PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-protector-strong)
set_target_properties(zcl_json_provider PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(ZCL_SANITIZE "Enable host ASan and UBSan" OFF)
option(ZCL_FUZZ "Build Clang libFuzzer harnesses" OFF)
option(ZCL_JNI "Build the thin JNI bridge" OFF)
option(ZCL_ORACLE "Build host-only OpenSSL/libsodium differential tests" OFF)
option(ZCL_TLS_REVIEW "Build quarantined TLS candidate for explicit host security review only" OFF)
if(ZCL_TLS_REVIEW AND (ANDROID OR ZCL_JNI))
    message(FATAL_ERROR "TLS is BLOCKED — REQUIRES FURTHER SECURITY REVIEW; no Android/JNI binding is permitted")
endif()
if(ZCL_FUZZ AND NOT ZCL_SANITIZE)
    message(FATAL_ERROR "ZCL_FUZZ requires ZCL_SANITIZE=ON for core and provider instrumentation")
endif()

set(ZCL_MBEDTLS_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/android-mbedtls")
add_library(zcl_hash_provider STATIC
    ${ZCL_MBEDTLS_SOURCE}/library/sha256.c ${ZCL_MBEDTLS_SOURCE}/library/sha512.c
    ${ZCL_MBEDTLS_SOURCE}/library/ripemd160.c ${ZCL_MBEDTLS_SOURCE}/library/platform_util.c)
if(ZCL_TLS_REVIEW)
  target_compile_definitions(zcl_hash_provider PUBLIC ZCL_TLS_REVIEW=1)
  foreach(unit asn1parse asn1write bignum bignum_core bignum_mod bignum_mod_raw
        chacha20 chachapoly cipher cipher_wrap constant_time ecdh ecdsa ecp ecp_curves
        md oid pk pk_ecc pkparse pk_wrap platform poly1305 rsa rsa_alt_helpers ssl_ciphersuites
        ssl_client ssl_msg ssl_tls ssl_tls12_client x509 x509_crt)
    target_sources(zcl_hash_provider PRIVATE ${ZCL_MBEDTLS_SOURCE}/library/${unit}.c)
  endforeach()
  target_sources(zcl_hash_provider PRIVATE src/transport_alloc.c)
  set_source_files_properties(src/transport_alloc.c PROPERTIES COMPILE_OPTIONS
    "-Wconversion;-Wsign-conversion;-Wshadow;-Wformat=2;-Wvla;-Wframe-larger-than=4096")
endif()
target_include_directories(zcl_hash_provider SYSTEM PUBLIC ${ZCL_MBEDTLS_SOURCE}/include)
target_include_directories(zcl_hash_provider PRIVATE include)
target_compile_definitions(zcl_hash_provider PUBLIC MBEDTLS_CONFIG_FILE="zcl_mbedtls_config.h")
target_compile_options(zcl_hash_provider PRIVATE -Wall -Wextra -Wpedantic -Werror
    -Os -fstack-protector-strong)
set_target_properties(zcl_hash_provider PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(ZCL_BLAKE2_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/android-blake2")
add_library(zcl_blake2_provider STATIC ${ZCL_BLAKE2_SOURCE}/blake2b-ref.c)
target_include_directories(zcl_blake2_provider PUBLIC ${ZCL_BLAKE2_SOURCE})
target_compile_options(zcl_blake2_provider PRIVATE -Wall -Wextra -Wpedantic -Werror
    -Os -fstack-protector-strong)
set_target_properties(zcl_blake2_provider PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(ZCL_QR_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/android-qrcodegen")
add_library(zcl_qr_provider STATIC ${ZCL_QR_SOURCE}/qrcodegen.c)
target_include_directories(zcl_qr_provider PUBLIC ${ZCL_QR_SOURCE})
target_compile_options(zcl_qr_provider PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-protector-strong)
set_target_properties(zcl_qr_provider PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(ZCL_SCAN_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/android-quirc")
add_library(zcl_scan_provider STATIC ${ZCL_SCAN_SOURCE}/quirc.c
    ${ZCL_SCAN_SOURCE}/identify.c ${ZCL_SCAN_SOURCE}/decode.c ${ZCL_SCAN_SOURCE}/version_db.c)
target_include_directories(zcl_scan_provider PUBLIC ${ZCL_SCAN_SOURCE})
target_compile_options(zcl_scan_provider PRIVATE -Wall -Wextra -fstack-protector-strong)
target_link_libraries(zcl_scan_provider PRIVATE m)
set_target_properties(zcl_scan_provider PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(ZCL_SECP_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/android-secp256k1")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(SECP256K1_INSTALL OFF CACHE BOOL "No dependency installation" FORCE)
set(SECP256K1_ENABLE_API_VISIBILITY_ATTRIBUTES OFF CACHE BOOL "Expose only JNI from the Android library" FORCE)
foreach(module ECDH RECOVERY EXTRAKEYS SCHNORRSIG MUSIG ELLSWIFT SILENTPAYMENTS)
    set(SECP256K1_ENABLE_MODULE_${module} OFF CACHE BOOL "Unused wallet module" FORCE)
endforeach()
foreach(program BENCHMARK TESTS EXHAUSTIVE_TESTS CTIME_TESTS EXAMPLES)
    set(SECP256K1_BUILD_${program} OFF CACHE BOOL "Test provider in its separate host build" FORCE)
endforeach()
set(SECP256K1_ASM OFF CACHE STRING "Portable C implementation" FORCE)
set(SECP256K1_VALGRIND OFF CACHE STRING "Separate instrumented provider build" FORCE)
set(SECP256K1_ECMULT_WINDOW_SIZE 8 CACHE STRING "Bounded mobile verification table" FORCE)
set(SECP256K1_ECMULT_GEN_KB 22 CACHE STRING "Bounded mobile signing table" FORCE)
add_subdirectory("${ZCL_SECP_SOURCE}" "secp256k1" EXCLUDE_FROM_ALL)

add_library(zcl_wallet_core STATIC src/amount.c src/base58.c src/address.c
    src/payment.c src/uri_text.c src/utf8_text.c src/secret_hash.c src/blake2_hash.c
    src/mnemonic.c src/mnemonic_words.c src/mnemonic_seed.c src/mnemonic_confirm.c src/ec_context.c src/bip32.c
    src/signature.c src/signature_script.c
    src/random.c src/receive_key.c src/network.c src/wallet_header.c src/wallet_record.c src/wallet_change.c
    src/change_state.c src/change_state_key.c src/change_custody.c src/change_reservation.c src/change_recovery.c
    src/change_ownership.c
    src/storage_posix.c src/storage_read.c src/storage_write.c
    src/storage_change_read.c src/storage_change_write.c src/storage_change_repair.c
    src/custody_policy.c src/receive_qr.c src/scan_qr.c src/camera_frame.c
    src/rpc_json.c src/rpc_values.c src/electrum_request.c src/electrum_line.c src/electrum_balance.c
    src/electrum_identity.c src/electrum_header.c src/header_inspect.c src/electrum_history.c
    src/sync.c src/sync_watch.c src/sync_owners.c
    src/transaction_check.c src/transaction_read.c src/transaction_write.c src/transaction_id.c src/transaction_sighash.c
    src/transaction_prevout.c src/transaction_assess.c src/transaction_review.c
    src/transaction_review_prepare.c src/transaction_review_sighash.c src/transaction_review_wallet.c
    src/transaction_context.c src/transaction_review_context.c src/transaction_review_wire.c
    src/transaction_review_wire_encode.c src/transaction_review_complete.c src/transaction_review_sign.c
    src/transaction_draft.c src/transaction_draft_check.c src/transaction_source.c
    src/transaction_merkle.c src/source_commitment.c)
# The TLS candidate remains confined to defensive host fixtures until the
# certificate-parser finding and provider analysis receive a completed review.
# No Android socket/transport entry point is compiled during that quarantine.
if(ZCL_TLS_REVIEW)
    target_sources(zcl_wallet_core PRIVATE src/transport.c src/transport_limit.c
        src/transport_resolve.c src/transport_socket.c src/transport_bio.c
        src/transport_tls.c src/transport_io.c)
endif()
target_include_directories(zcl_wallet_core PUBLIC include)
target_include_directories(zcl_wallet_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/android-bip39")
target_link_libraries(zcl_wallet_core PRIVATE zcl_hash_provider zcl_blake2_provider secp256k1 zcl_qr_provider zcl_scan_provider zcl_json_provider)
set_target_properties(zcl_wallet_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_options(zcl_wallet_core PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
    -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wformat=2
    -Wundef -Wvla -Wcast-align -Wwrite-strings -Wdouble-promotion
    -Wframe-larger-than=4096
    -fstack-protector-strong)

if(ZCL_SANITIZE)
    if(ANDROID)
        message(FATAL_ERROR "Use a separately configured on-device sanitizer build")
    endif()
    target_compile_options(zcl_wallet_core PUBLIC -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_compile_options(zcl_hash_provider PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_compile_options(zcl_blake2_provider PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_compile_options(zcl_qr_provider PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_compile_options(zcl_scan_provider PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_compile_options(zcl_json_provider PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_compile_options(secp256k1 PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_compile_options(secp256k1_precomputed PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(zcl_wallet_core PUBLIC -fsanitize=address,undefined)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(ZCL_AUTHORED_INTEGER_CHECKS
            -fsanitize=unsigned-integer-overflow,implicit-integer-truncation,implicit-integer-sign-change)
        # Source properties also cover JNI/fault-test copies of authored C.
        # Providers retain ASan/UBSan; their modular arithmetic is separate.
        file(GLOB ZCL_AUTHORED_C CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
        set_property(SOURCE ${ZCL_AUTHORED_C} APPEND PROPERTY COMPILE_OPTIONS ${ZCL_AUTHORED_INTEGER_CHECKS})
        target_link_options(zcl_wallet_core PUBLIC ${ZCL_AUTHORED_INTEGER_CHECKS})
    endif()
endif()

if(ZCL_JNI OR ANDROID)
    add_library(zclwallet_jni SHARED src/jni_amount.c src/jni_codecs.c src/jni_payment.c src/jni_support.c
        src/jni_keys.c src/jni_wallet_record.c src/jni_storage.c src/jni_custody.c src/jni_qr.c src/jni_scan.c src/jni_camera.c
        src/jni_sync.c src/jni_review.c src/jni_review_packet.c src/jni_full_review.c src/jni_full_prepare.c
        src/jni_draft.c src/jni_draft_fields.c src/jni_draft_wire.c)
    target_link_libraries(zclwallet_jni PRIVATE zcl_wallet_core)
    target_compile_options(zclwallet_jni PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -fstack-protector-strong)
    target_compile_options(zclwallet_jni PRIVATE -Wframe-larger-than=4096)
    if(NOT ANDROID)
        find_package(JNI REQUIRED COMPONENTS JVM)
        target_include_directories(zclwallet_jni PRIVATE ${JNI_INCLUDE_DIRS})
        # Real-VM exception bridge used only by host JVM tests. It calls the
        # production JNI library; no fault interface is compiled for Android.
        add_library(zclwallet_secret_fixture SHARED tests/jni_secret_output_fixture.c)
        target_include_directories(zclwallet_secret_fixture PRIVATE ${JNI_INCLUDE_DIRS})
        target_link_libraries(zclwallet_secret_fixture PRIVATE zclwallet_jni)
        target_compile_options(zclwallet_secret_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    endif()
    find_package(Threads REQUIRED)
    if(ANDROID)
        # Size-optimize thin adapters; measured phrase conversion retains -O2.
        target_compile_options(zclwallet_jni PRIVATE -Os)
        set_property(SOURCE src/jni_keys.c APPEND PROPERTY COMPILE_OPTIONS -O2)
    endif()
    target_link_libraries(zclwallet_jni PRIVATE Threads::Threads)
    target_link_options(zclwallet_jni PRIVATE -Wl,-z,relro,-z,now,-z,noexecstack)
endif()

if(ZCL_FUZZ)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR ANDROID)
        message(FATAL_ERROR "Fuzzing requires host Clang")
    endif()
    target_compile_options(zcl_wallet_core PRIVATE -fsanitize=fuzzer-no-link)
    target_compile_options(zcl_qr_provider PRIVATE -fsanitize=fuzzer-no-link)
    target_compile_options(zcl_scan_provider PRIVATE -fsanitize=fuzzer-no-link)
    target_compile_options(zcl_json_provider PRIVATE -fsanitize=fuzzer-no-link)
    target_compile_options(zcl_hash_provider PRIVATE -fsanitize=fuzzer-no-link)
    target_compile_options(zcl_blake2_provider PRIVATE -fsanitize=fuzzer-no-link)
    target_compile_options(secp256k1 PRIVATE -fsanitize=fuzzer-no-link)
    target_compile_options(secp256k1_precomputed PRIVATE -fsanitize=fuzzer-no-link)
    add_executable(fuzz_blake2 tests/test_blake2.c)
    target_include_directories(fuzz_blake2 PRIVATE src)
    target_link_libraries(fuzz_blake2 PRIVATE zcl_wallet_core)
    target_compile_definitions(fuzz_blake2 PRIVATE ZCL_BLAKE2_FUZZ=1)
    target_compile_options(fuzz_blake2 PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
    target_link_options(fuzz_blake2 PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_sighash tests/fuzz_sighash.c)
    target_include_directories(fuzz_sighash PRIVATE src)
    target_link_libraries(fuzz_sighash PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_sighash PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
    target_link_options(fuzz_sighash PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_authentication_window tests/fuzz_authentication_window.c)
    target_link_libraries(fuzz_authentication_window PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_authentication_window PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_authentication_window PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_sync tests/fuzz_sync.c tests/sync_fixture.c)
    target_link_libraries(fuzz_sync PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_sync PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_sync PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_sync_watch tests/fuzz_sync_watch.c tests/sync_fixture.c)
    target_link_libraries(fuzz_sync_watch PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_sync_watch PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_sync_watch PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_sync_owners tests/fuzz_sync_owners.c tests/sync_fixture.c)
    target_link_libraries(fuzz_sync_owners PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_sync_owners PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_sync_owners PRIVATE -fsanitize=fuzzer,address,undefined)
    if(ZCL_TLS_REVIEW)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    add_executable(fuzz_tls tests/fuzz_tls.c)
    target_include_directories(fuzz_tls PRIVATE src)
    target_link_libraries(fuzz_tls PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(fuzz_tls PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_tls PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(seed_tls tests/seed_tls.c tests/tls_fixture.c tests/fuzz_tls.c)
    target_link_libraries(seed_tls PRIVATE zcl_wallet_core zcl_hash_provider OpenSSL::SSL Threads::Threads)
    target_include_directories(seed_tls PRIVATE src)
    target_compile_options(seed_tls PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    target_link_options(seed_tls PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    endif()
    add_executable(fuzz_electrum tests/fuzz_electrum.c)
    target_link_libraries(fuzz_electrum PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_electrum PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
    target_link_options(fuzz_electrum PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_electrum_line tests/fuzz_electrum_line.c)
    target_link_libraries(fuzz_electrum_line PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_electrum_line PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
        -Wshadow -Wvla -Wframe-larger-than=4096)
    target_link_options(fuzz_electrum_line PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_transaction tests/fuzz_transaction.c)
    target_link_libraries(fuzz_transaction PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_transaction PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_transaction PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_assessment tests/fuzz_assessment.c tests/assessment_fixture.c)
    target_link_libraries(fuzz_assessment PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_assessment PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_assessment PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_draft tests/fuzz_draft.c tests/draft_fixture.c tests/assessment_fixture.c)
    target_link_libraries(fuzz_draft PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_draft PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow)
    target_link_options(fuzz_draft PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_review tests/fuzz_review.c tests/assessment_fixture.c tests/context_reference.c)
    target_include_directories(fuzz_review PRIVATE src)
    target_link_libraries(fuzz_review PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_review PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow)
    target_link_options(fuzz_review PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_signature tests/fuzz_signature.c)
    target_include_directories(fuzz_signature PRIVATE src)
    target_link_libraries(fuzz_signature PRIVATE zcl_wallet_core secp256k1)
    target_compile_options(fuzz_signature PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wframe-larger-than=4096)
    target_link_options(fuzz_signature PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_change tests/fuzz_change.c)
    target_link_libraries(fuzz_change PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_change PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_change PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(seed_assessment tests/seed_assessment.c tests/assessment_fixture.c)
    target_link_libraries(seed_assessment PRIVATE zcl_wallet_core)
    target_compile_options(seed_assessment PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    target_link_options(seed_assessment PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    add_executable(seed_full_review tests/seed_full_review.c tests/source_assessment_fixture.c tests/assessment_fixture.c)
    target_link_libraries(seed_full_review PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(seed_full_review PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    target_link_options(seed_full_review PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    add_executable(seed_electrum tests/seed_electrum.c)
    target_compile_options(seed_electrum PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    add_executable(fuzz_camera_frame tests/fuzz_camera_frame.c tests/camera_reference.c)
    target_link_libraries(fuzz_camera_frame PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_camera_frame PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_camera_frame PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_scan_qr tests/fuzz_scan_qr.c)
    target_link_libraries(fuzz_scan_qr PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_scan_qr PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_scan_qr PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(seed_scan_qr tests/seed_scan_qr.c tests/scan_fixture.c)
    target_link_libraries(seed_scan_qr PRIVATE zcl_wallet_core zcl_qr_provider)
    target_compile_options(seed_scan_qr PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(seed_scan_qr PRIVATE -fsanitize=fuzzer-no-link)
    add_executable(fuzz_receive_qr tests/fuzz_receive_qr.c)
    target_link_libraries(fuzz_receive_qr PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_receive_qr PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_receive_qr PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_amount tests/fuzz_amount.c)
    target_link_libraries(fuzz_amount PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_amount PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_amount PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_codecs tests/fuzz_codecs.c)
    target_link_libraries(fuzz_codecs PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_codecs PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_codecs PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_address_encode tests/fuzz_address_encode.c)
    target_link_libraries(fuzz_address_encode PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_address_encode PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_address_encode PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_payment tests/fuzz_payment.c tests/utf8_reference.c)
    target_include_directories(fuzz_payment PRIVATE src)
    target_link_libraries(fuzz_payment PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_payment PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_payment PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_mnemonic tests/fuzz_mnemonic.c)
    target_link_libraries(fuzz_mnemonic PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_mnemonic PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_mnemonic PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_wallet_change tests/fuzz_wallet_change.c)
    target_link_libraries(fuzz_wallet_change PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_wallet_change PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
    target_link_options(fuzz_wallet_change PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz_change_state tests/fuzz_change_state.c)
    target_link_libraries(fuzz_change_state PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_change_state PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
    target_link_options(fuzz_change_state PRIVATE -fsanitize=fuzzer,address,undefined)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(fuzz_change_storage tests/fuzz_change_storage.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(fuzz_change_storage PRIVATE zcl_wallet_core)
        target_compile_options(fuzz_change_storage PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(fuzz_change_storage PRIVATE -fsanitize=fuzzer,address,undefined)
        add_executable(fuzz_wallet_storage tests/fuzz_wallet_storage.c tests/storage_fixture.c)
        target_link_libraries(fuzz_wallet_storage PRIVATE zcl_wallet_core)
        target_compile_options(fuzz_wallet_storage PRIVATE -fsanitize=fuzzer,address,undefined
            -fno-sanitize-recover=all -Wall -Wextra -Wpedantic -Werror -Wconversion
            -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(fuzz_wallet_storage PRIVATE -fsanitize=fuzzer,address,undefined)
        add_executable(fuzz_change_probe tests/fuzz_change_probe.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(fuzz_change_probe PRIVATE zcl_wallet_core)
        target_compile_options(fuzz_change_probe PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(fuzz_change_probe PRIVATE -fsanitize=fuzzer,address,undefined)
        add_executable(fuzz_change_repair tests/fuzz_change_repair.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_include_directories(fuzz_change_repair PRIVATE src)
        target_link_libraries(fuzz_change_repair PRIVATE zcl_wallet_core)
        target_compile_options(fuzz_change_repair PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(fuzz_change_repair PRIVATE -fsanitize=fuzzer,address,undefined)
        add_executable(fuzz_change_recovery tests/fuzz_change_recovery.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(fuzz_change_recovery PRIVATE zcl_wallet_core)
        target_compile_options(fuzz_change_recovery PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(fuzz_change_recovery PRIVATE -fsanitize=fuzzer,address,undefined)
        add_executable(fuzz_change_reservation tests/fuzz_change_reservation.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(fuzz_change_reservation PRIVATE zcl_wallet_core)
        target_compile_options(fuzz_change_reservation PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(fuzz_change_reservation PRIVATE -fsanitize=fuzzer,address,undefined)
        add_executable(fuzz_change_ownership tests/fuzz_change_reservation.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(fuzz_change_ownership PRIVATE zcl_wallet_core)
        target_compile_definitions(fuzz_change_ownership PRIVATE ZCL_CHANGE_RECONSTRUCT=1)
        target_compile_options(fuzz_change_ownership PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow)
        target_link_options(fuzz_change_ownership PRIVATE -fsanitize=fuzzer,address,undefined)
        add_executable(seed_change_reservation tests/seed_change_reservation.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(seed_change_reservation PRIVATE zcl_wallet_core)
        target_compile_options(seed_change_reservation PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(seed_change_reservation PRIVATE -fsanitize=fuzzer-no-link)
    endif()
    add_executable(fuzz_wallet_record tests/fuzz_wallet_record.c)
    target_link_libraries(fuzz_wallet_record PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_wallet_record PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_wallet_record PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

# Real JVM/device tests qualify the VM boundary separately. This host fixture
# injects JNI allocation/region failures and checks native cleanup under ASan.
# Its allocator substitution affects only this target's JNI source, never the
# shipped library or libFuzzer's runtime allocator.
if(NOT ANDROID AND NOT ZCL_TLS_REVIEW AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(JNI QUIET COMPONENTS JVM)
    if(JNI_FOUND)
        add_executable(jni_custody_tests tests/test_jni_custody.c src/jni_custody.c)
        target_include_directories(jni_custody_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_custody_tests PRIVATE zcl_wallet_core)
        target_compile_options(jni_custody_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_custody COMMAND jni_custody_tests)
        set_tests_properties(wallet_jni_custody PROPERTIES TIMEOUT 15)
        find_package(Threads REQUIRED)
        add_executable(jni_amount_tests tests/test_jni_amount.c src/jni_amount.c src/jni_support.c)
        target_include_directories(jni_amount_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_amount_tests PRIVATE zcl_wallet_core)
        target_compile_options(jni_amount_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        enable_testing()
        add_test(NAME wallet_jni_amount COMMAND jni_amount_tests)
        set_tests_properties(wallet_jni_amount PROPERTIES TIMEOUT 15)
        add_executable(jni_camera_tests tests/test_jni_camera.c src/jni_camera.c src/jni_scan.c
            src/jni_payment.c src/jni_support.c)
        target_include_directories(jni_camera_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_camera_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(jni_camera_tests PRIVATE
            malloc=zcl_jni_camera_test_malloc free=zcl_jni_camera_test_free)
        target_compile_options(jni_camera_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        enable_testing()
        add_test(NAME wallet_jni_camera COMMAND jni_camera_tests)
        set_tests_properties(wallet_jni_camera PROPERTIES TIMEOUT 15)
        add_executable(jni_sync_tests tests/test_jni_sync.c tests/sync_fixture.c src/jni_sync.c src/jni_support.c)
        target_include_directories(jni_sync_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_sync_tests PRIVATE zcl_wallet_core Threads::Threads)
        target_compile_definitions(jni_sync_tests PRIVATE malloc=zcl_jni_test_malloc free=zcl_jni_test_free)
        target_compile_definitions(jni_sync_tests PRIVATE zcl_secure_zero=zcl_jni_sync_test_zero
            zcl_sync_watch_snapshot=zcl_jni_sync_test_snapshot
            zcl_sync_watch_request=zcl_jni_sync_test_request)
        target_compile_options(jni_sync_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        enable_testing()
        add_executable(jni_storage_tests tests/test_jni_storage.c tests/change_storage_fixture.c
            tests/storage_fixture.c src/jni_storage.c src/jni_support.c)
        target_include_directories(jni_storage_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_storage_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(jni_storage_tests PRIVATE
            zcl_secure_zero=zcl_jni_storage_test_zero zcl_wallet_change_create=zcl_jni_storage_test_create)
        target_compile_options(jni_storage_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_storage COMMAND jni_storage_tests)
        set_tests_properties(wallet_jni_storage PROPERTIES TIMEOUT 15)
        add_executable(jni_storage_fuzz_tests tests/test_jni_storage_fuzz.c tests/fuzz_jni_storage.c
            tests/change_storage_fixture.c tests/storage_fixture.c src/jni_storage.c src/jni_support.c)
        target_include_directories(jni_storage_fuzz_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_storage_fuzz_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(jni_storage_fuzz_tests PRIVATE zcl_secure_zero=zcl_jni_storage_fuzz_zero)
        target_compile_options(jni_storage_fuzz_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_storage_fuzz_regression COMMAND jni_storage_fuzz_tests)
        set_tests_properties(wallet_jni_storage_fuzz_regression PROPERTIES TIMEOUT 30)
        if(ZCL_FUZZ)
            add_executable(fuzz_jni_storage tests/fuzz_jni_storage.c tests/change_storage_fixture.c
                tests/storage_fixture.c src/jni_storage.c src/jni_support.c)
            target_include_directories(fuzz_jni_storage PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_storage PRIVATE zcl_wallet_core)
            target_compile_definitions(fuzz_jni_storage PRIVATE zcl_secure_zero=zcl_jni_storage_fuzz_zero)
            target_compile_options(fuzz_jni_storage PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_storage PRIVATE -fsanitize=fuzzer,address,undefined)
        endif()
        add_executable(jni_key_tests tests/test_jni_keys.c src/jni_wallet_record.c src/jni_keys.c src/jni_support.c)
        target_include_directories(jni_key_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_key_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(jni_key_tests PRIVATE
            zcl_secure_zero=zcl_jni_key_test_zero zcl_random_bytes=zcl_jni_key_test_random
            zcl_mnemonic_decode=zcl_jni_key_test_decode)
        target_compile_options(jni_key_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_keys COMMAND jni_key_tests)
        set_tests_properties(wallet_jni_keys PROPERTIES TIMEOUT 15)
        add_executable(jni_entropy_tests tests/test_jni_entropy.c src/jni_keys.c src/jni_support.c)
        target_include_directories(jni_entropy_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_entropy_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(jni_entropy_tests PRIVATE
            zcl_secure_zero=zcl_entropy_test_zero zcl_random_bytes=zcl_entropy_test_random)
        target_compile_options(jni_entropy_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_entropy COMMAND jni_entropy_tests)
        set_tests_properties(wallet_jni_entropy PROPERTIES TIMEOUT 15)
        add_executable(jni_record_tests tests/test_jni_record.c src/jni_wallet_record.c src/jni_support.c)
        target_include_directories(jni_record_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_record_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(jni_record_tests PRIVATE zcl_secure_zero=zcl_jni_record_test_zero
            zcl_wallet_record_pack=zcl_jni_record_test_pack zcl_wallet_record_parse=zcl_jni_record_test_parse)
        target_compile_options(jni_record_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_record COMMAND jni_record_tests)
        set_tests_properties(wallet_jni_record PROPERTIES TIMEOUT 15)
        add_executable(jni_public_tests tests/test_jni_public.c src/jni_qr.c src/jni_payment.c src/jni_support.c)
        target_include_directories(jni_public_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_public_tests PRIVATE zcl_wallet_core)
        target_compile_options(jni_public_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_public COMMAND jni_public_tests)
        set_tests_properties(wallet_jni_public PROPERTIES TIMEOUT 15)
        add_test(NAME wallet_jni_sync COMMAND jni_sync_tests)
        set_tests_properties(wallet_jni_sync PROPERTIES TIMEOUT 15)
        add_executable(jni_sync_race_tests tests/test_jni_sync_races.c src/jni_sync.c src/jni_support.c)
        target_include_directories(jni_sync_race_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_sync_race_tests PRIVATE zcl_wallet_core Threads::Threads)
        target_compile_options(jni_sync_race_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_sync_races COMMAND jni_sync_race_tests)
        set_tests_properties(wallet_jni_sync_races PROPERTIES TIMEOUT 15)
        add_executable(jni_review_tests tests/test_jni_review.c tests/assessment_fixture.c tests/source_assessment_fixture.c
            src/jni_review.c src/jni_review_packet.c src/jni_full_review.c src/jni_full_prepare.c src/jni_draft.c src/jni_draft_fields.c src/jni_draft_wire.c src/jni_support.c)
        target_include_directories(jni_review_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_review_tests PRIVATE zcl_wallet_core zcl_hash_provider Threads::Threads)
        target_compile_definitions(jni_review_tests PRIVATE malloc=zcl_jni_review_test_malloc free=zcl_jni_review_test_free)
        target_compile_definitions(jni_review_tests PRIVATE zcl_secure_zero=zcl_jni_review_test_zero
            zcl_review_open_full_sources=zcl_jni_review_test_full_open
            zcl_transaction_draft_full_sources=zcl_jni_review_test_construct
            zcl_review_snapshot_get=zcl_jni_review_test_snapshot)
        target_compile_options(jni_review_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_review COMMAND jni_review_tests)
        set_tests_properties(wallet_jni_review PROPERTIES TIMEOUT 15)
        add_executable(jni_address_tests tests/test_jni_address.c src/jni_codecs.c src/jni_support.c)
        target_include_directories(jni_address_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_address_tests PRIVATE zcl_wallet_core)
        target_compile_options(jni_address_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_address COMMAND jni_address_tests)
        set_tests_properties(wallet_jni_address PROPERTIES TIMEOUT 15)
        add_executable(jni_draft_tests tests/test_jni_draft.c tests/draft_fixture.c tests/assessment_fixture.c
            src/jni_draft.c src/jni_draft_fields.c src/jni_draft_wire.c src/jni_support.c)
        target_include_directories(jni_draft_tests PRIVATE src ${JNI_INCLUDE_DIRS})
        target_link_libraries(jni_draft_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(jni_draft_tests PRIVATE malloc=zcl_jni_draft_test_malloc free=zcl_jni_draft_test_free)
        target_compile_definitions(jni_draft_tests PRIVATE zcl_secure_zero=zcl_jni_draft_test_zero
            zcl_transaction_draft=zcl_jni_draft_test_construct)
        target_compile_options(jni_draft_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_jni_draft COMMAND jni_draft_tests)
        set_tests_properties(wallet_jni_draft PROPERTIES TIMEOUT 15)
        if(ZCL_FUZZ)
            add_executable(fuzz_jni_camera tests/test_jni_camera.c src/jni_camera.c src/jni_scan.c
                src/jni_payment.c src/jni_support.c)
            target_include_directories(fuzz_jni_camera PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_camera PRIVATE zcl_wallet_core)
            target_compile_definitions(fuzz_jni_camera PRIVATE ZCL_JNI_CAMERA_FUZZ=1
                malloc=zcl_jni_camera_test_malloc free=zcl_jni_camera_test_free)
            target_compile_options(fuzz_jni_camera PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
                -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_camera PRIVATE -fsanitize=fuzzer,address,undefined)
            add_executable(fuzz_jni_keys tests/test_jni_keys.c src/jni_wallet_record.c src/jni_keys.c src/jni_support.c)
            target_include_directories(fuzz_jni_keys PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_keys PRIVATE zcl_wallet_core)
            target_compile_definitions(fuzz_jni_keys PRIVATE ZCL_JNI_KEYS_FUZZ=1
                zcl_secure_zero=zcl_jni_key_test_zero zcl_random_bytes=zcl_jni_key_test_random
                zcl_mnemonic_decode=zcl_jni_key_test_decode)
            target_compile_options(fuzz_jni_keys PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_keys PRIVATE -fsanitize=fuzzer,address,undefined)
            add_executable(fuzz_jni_record tests/test_jni_record.c src/jni_wallet_record.c src/jni_support.c)
            target_include_directories(fuzz_jni_record PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_record PRIVATE zcl_wallet_core)
            target_compile_definitions(fuzz_jni_record PRIVATE ZCL_JNI_RECORD_FUZZ=1)
            target_compile_definitions(fuzz_jni_record PRIVATE zcl_secure_zero=zcl_jni_record_test_zero
                zcl_wallet_record_pack=zcl_jni_record_test_pack zcl_wallet_record_parse=zcl_jni_record_test_parse)
            target_compile_options(fuzz_jni_record PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_record PRIVATE -fsanitize=fuzzer,address,undefined)
            add_executable(fuzz_jni_public tests/test_jni_public.c src/jni_qr.c src/jni_payment.c src/jni_support.c)
            target_include_directories(fuzz_jni_public PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_public PRIVATE zcl_wallet_core)
            target_compile_definitions(fuzz_jni_public PRIVATE ZCL_JNI_PUBLIC_FUZZ=1)
            target_compile_options(fuzz_jni_public PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_public PRIVATE -fsanitize=fuzzer,address,undefined)
            add_executable(fuzz_jni_draft tests/test_jni_draft.c tests/draft_fixture.c tests/assessment_fixture.c
                src/jni_draft.c src/jni_draft_fields.c src/jni_draft_wire.c src/jni_support.c)
            target_include_directories(fuzz_jni_draft PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_draft PRIVATE zcl_wallet_core)
            target_compile_definitions(fuzz_jni_draft PRIVATE ZCL_JNI_DRAFT_FUZZ=1
                malloc=zcl_jni_draft_test_malloc free=zcl_jni_draft_test_free)
            target_compile_definitions(fuzz_jni_draft PRIVATE zcl_secure_zero=zcl_jni_draft_test_zero
                zcl_transaction_draft=zcl_jni_draft_test_construct)
            target_compile_options(fuzz_jni_draft PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_draft PRIVATE -fsanitize=fuzzer,address,undefined)
            add_executable(fuzz_jni_address tests/test_jni_address.c src/jni_codecs.c src/jni_support.c)
            target_include_directories(fuzz_jni_address PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_address PRIVATE zcl_wallet_core)
            target_compile_definitions(fuzz_jni_address PRIVATE ZCL_JNI_ADDRESS_FUZZ=1)
            target_compile_options(fuzz_jni_address PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_address PRIVATE -fsanitize=fuzzer,address,undefined)
            add_executable(fuzz_jni_review tests/test_jni_review.c tests/assessment_fixture.c tests/source_assessment_fixture.c
                src/jni_review.c src/jni_review_packet.c src/jni_full_review.c src/jni_full_prepare.c src/jni_draft.c src/jni_draft_fields.c src/jni_draft_wire.c src/jni_support.c)
            target_include_directories(fuzz_jni_review PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_review PRIVATE zcl_wallet_core zcl_hash_provider Threads::Threads)
            target_compile_definitions(fuzz_jni_review PRIVATE ZCL_JNI_REVIEW_FUZZ=1
                malloc=zcl_jni_review_test_malloc free=zcl_jni_review_test_free)
            target_compile_definitions(fuzz_jni_review PRIVATE zcl_secure_zero=zcl_jni_review_test_zero
                zcl_review_open_full_sources=zcl_jni_review_test_full_open
                zcl_transaction_draft_full_sources=zcl_jni_review_test_construct
                zcl_review_snapshot_get=zcl_jni_review_test_snapshot)
            target_compile_options(fuzz_jni_review PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_review PRIVATE -fsanitize=fuzzer,address,undefined)
            add_executable(fuzz_jni_sync tests/test_jni_sync.c tests/sync_fixture.c src/jni_sync.c src/jni_support.c)
            target_include_directories(fuzz_jni_sync PRIVATE src ${JNI_INCLUDE_DIRS})
            target_link_libraries(fuzz_jni_sync PRIVATE zcl_wallet_core Threads::Threads)
            target_compile_definitions(fuzz_jni_sync PRIVATE ZCL_JNI_FUZZ=1 malloc=zcl_jni_test_malloc free=zcl_jni_test_free)
            target_compile_definitions(fuzz_jni_sync PRIVATE zcl_secure_zero=zcl_jni_sync_test_zero
                zcl_sync_watch_snapshot=zcl_jni_sync_test_snapshot
                zcl_sync_watch_request=zcl_jni_sync_test_request)
            target_compile_options(fuzz_jni_sync PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
            target_link_options(fuzz_jni_sync PRIVATE -fsanitize=fuzzer,address,undefined)
        endif()
    else()
        message(STATUS "JNI headers unavailable: native JNI sync failure fixture not qualified")
    endif()
endif()

if(NOT ANDROID)
    enable_testing()
    add_executable(blake2_tests tests/test_blake2.c)
    target_include_directories(blake2_tests PRIVATE src)
    target_link_libraries(blake2_tests PRIVATE zcl_wallet_core)
    target_compile_options(blake2_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_blake2 COMMAND blake2_tests)
    set_tests_properties(wallet_blake2 PROPERTIES TIMEOUT 15)
    # Compile this wrapper separately: no replacement provider or wipe symbols
    # enter zcl_wallet_core, the real provider, or any Android shared library.
    add_executable(blake2_failure_tests tests/test_blake2_failures.c src/blake2_hash.c)
    target_include_directories(blake2_failure_tests PRIVATE src include ${ZCL_BLAKE2_SOURCE})
    target_compile_definitions(blake2_failure_tests PRIVATE
        blake2b_init_param=zcl_blake2_test_init blake2b_update=zcl_blake2_test_update
        blake2b_final=zcl_blake2_test_final zcl_secure_zero=zcl_blake2_test_zero)
    target_compile_options(blake2_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    if(ZCL_SANITIZE)
        target_compile_options(blake2_failure_tests PRIVATE -fsanitize=address,undefined
            -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(blake2_failure_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME wallet_blake2_failures COMMAND blake2_failure_tests)
    set_tests_properties(wallet_blake2_failures PROPERTIES TIMEOUT 15)
    add_executable(sighash_tests tests/test_sighash.c)
    target_include_directories(sighash_tests PRIVATE src)
    target_link_libraries(sighash_tests PRIVATE zcl_wallet_core)
    target_compile_options(sighash_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_sighash COMMAND sighash_tests)
    set_tests_properties(wallet_sighash PROPERTIES TIMEOUT 15)
    add_executable(sighash_failure_tests tests/test_sighash_failures.c src/transaction_sighash.c)
    target_include_directories(sighash_failure_tests PRIVATE src)
    target_link_libraries(sighash_failure_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(sighash_failure_tests PRIVATE
        zcl_blake2b256=zcl_sighash_test_hash zcl_secure_zero=zcl_sighash_test_zero)
    target_compile_options(sighash_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_sighash_failures COMMAND sighash_failure_tests)
    set_tests_properties(wallet_sighash_failures PROPERTIES TIMEOUT 15)
    # Explicit host-only camera scene generator; no node or wallet authority.
    add_executable(seed_camera_scene tests/seed_camera_scene.c)
    target_link_libraries(seed_camera_scene PRIVATE zcl_qr_provider)
    target_compile_options(seed_camera_scene PRIVATE -Wall -Wextra -Wpedantic -Werror -Wvla -Wframe-larger-than=4096)
    set_source_files_properties(tests/seed_camera_scene.c PROPERTIES COMPILE_OPTIONS
        "-Wconversion;-Wsign-conversion;-Wformat=2;-Wshadow;-Wstrict-prototypes;-Wmissing-prototypes")
    if(ZCL_SANITIZE)
        target_compile_options(seed_camera_scene PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(seed_camera_scene PRIVATE -fsanitize=address,undefined)
    endif()
    if(ZCL_FUZZ)
        target_link_options(seed_camera_scene PRIVATE -fsanitize=fuzzer-no-link)
    endif()
    find_program(ZCL_TEST_FUZZ_CLANG NAMES clang-20 clang)
    set(ZCL_TEST_FUZZ_COMPILER "")
    if(ZCL_TEST_FUZZ_CLANG)
        set(ZCL_TEST_FUZZ_COMPILER "${ZCL_TEST_FUZZ_CLANG}")
    endif()
    add_test(NAME wallet_fuzz_profile COMMAND ${CMAKE_COMMAND}
        "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}" "-DCOMPILER=${CMAKE_C_COMPILER}"
        "-DCLANG=${ZCL_TEST_FUZZ_COMPILER}"
        "-DNEGATIVE_BUILD=${CMAKE_CURRENT_BINARY_DIR}/refuse-unsanitized-fuzz"
        "-DPOSITIVE_BUILD=${CMAKE_CURRENT_BINARY_DIR}/verify-sanitized-fuzz"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_fuzz_profile.cmake")
    set_tests_properties(wallet_fuzz_profile PROPERTIES TIMEOUT 120)
    if(ZCL_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "Clang")
        foreach(profile control probe)
            add_executable(integer_sanitizer_${profile} tests/test_integer_sanitizers.c)
            target_compile_options(integer_sanitizer_${profile} PRIVATE
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
                -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096
                -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            target_link_options(integer_sanitizer_${profile} PRIVATE -fsanitize=address,undefined)
        endforeach()
        target_compile_options(integer_sanitizer_probe PRIVATE ${ZCL_AUTHORED_INTEGER_CHECKS})
        target_link_options(integer_sanitizer_probe PRIVATE ${ZCL_AUTHORED_INTEGER_CHECKS})
        add_test(NAME wallet_integer_sanitizers COMMAND ${CMAKE_COMMAND}
            -DCONTROL=$<TARGET_FILE:integer_sanitizer_control> -DPROBE=$<TARGET_FILE:integer_sanitizer_probe>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_integer_sanitizers.cmake")
        set_tests_properties(wallet_integer_sanitizers PROPERTIES TIMEOUT 30)
    endif()
    if(NOT ZCL_TLS_REVIEW)
        add_test(NAME wallet_tls_quarantine COMMAND ${CMAKE_COMMAND}
            -DNM=${CMAKE_NM} -DCORE=$<TARGET_FILE:zcl_wallet_core>
            -DHASH=$<TARGET_FILE:zcl_hash_provider> -DSOURCE=${CMAKE_CURRENT_LIST_DIR}
            -DNEGATIVE_BUILD=${CMAKE_CURRENT_BINARY_DIR}/refuse-tls-jni
            -P ${CMAKE_CURRENT_LIST_DIR}/tests/check_quarantine.cmake)
        set_tests_properties(wallet_tls_quarantine PROPERTIES TIMEOUT 15)
    endif()
    add_executable(sync_tests tests/test_sync.c tests/sync_fixture.c src/sync.c)
    target_link_libraries(sync_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(sync_tests PRIVATE zcl_secure_zero=zcl_sync_test_zero
        zcl_address_parse=zcl_sync_test_parse zcl_electrum_tip_reply=zcl_sync_test_tip)
    target_compile_options(sync_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
        -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_sync COMMAND sync_tests)
    set_tests_properties(wallet_sync PROPERTIES TIMEOUT 15)
    add_executable(sync_watch_tests tests/test_sync_watch.c tests/sync_fixture.c src/sync_watch.c)
    target_link_libraries(sync_watch_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(sync_watch_tests PRIVATE zcl_secure_zero=zcl_watch_test_zero
        zcl_address_parse=zcl_watch_test_parse zcl_sync_get_report=zcl_watch_test_report)
    target_compile_options(sync_watch_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    add_test(NAME wallet_sync_watch COMMAND sync_watch_tests)
    set_tests_properties(wallet_sync_watch PROPERTIES TIMEOUT 15)
    add_executable(sync_owner_tests tests/test_sync_owners.c tests/sync_fixture.c)
    target_link_libraries(sync_owner_tests PRIVATE zcl_wallet_core)
    target_compile_options(sync_owner_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    add_test(NAME wallet_sync_owners COMMAND sync_owner_tests)
    set_tests_properties(wallet_sync_owners PROPERTIES TIMEOUT 15)
    add_executable(sync_owner_retirement_tests tests/test_sync_owner_retirement.c tests/sync_fixture.c
        src/sync_owners.c)
    target_link_libraries(sync_owner_retirement_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(sync_owner_retirement_tests PRIVATE
        zcl_sync_watch_init=zcl_owner_test_init zcl_sync_watch_init_with_history=zcl_owner_test_init_history
        zcl_secure_zero=zcl_owner_test_zero)
    target_compile_options(sync_owner_retirement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_sync_owner_retirement COMMAND sync_owner_retirement_tests)
    set_tests_properties(wallet_sync_owner_retirement PROPERTIES TIMEOUT 15)
    if(ZCL_TLS_REVIEW)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    add_executable(transport_tests tests/test_transport.c tests/tls_fixture.c)
    target_include_directories(transport_tests PRIVATE src)
    target_link_libraries(transport_tests PRIVATE zcl_wallet_core zcl_hash_provider OpenSSL::SSL Threads::Threads)
    target_compile_options(transport_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_transport COMMAND transport_tests)
    set_tests_properties(wallet_transport PROPERTIES TIMEOUT 30)
    add_executable(transport_allocation_tests tests/test_transport_allocations.c tests/tls_fixture.c)
    target_link_libraries(transport_allocation_tests PRIVATE zcl_wallet_core OpenSSL::SSL Threads::Threads)
    target_compile_options(transport_allocation_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(transport_allocation_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free)
    add_test(NAME wallet_transport_allocations COMMAND transport_allocation_tests)
    set_tests_properties(wallet_transport_allocations PROPERTIES TIMEOUT 90)
    endif()
    add_executable(electrum_tests tests/test_electrum.c)
    target_link_libraries(electrum_tests PRIVATE zcl_wallet_core zcl_json_provider)
    target_include_directories(electrum_tests PRIVATE src)
    target_compile_options(electrum_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_electrum COMMAND electrum_tests)
    set_tests_properties(wallet_electrum PROPERTIES TIMEOUT 15)
    add_executable(electrum_line_tests tests/test_electrum_line.c tests/fuzz_electrum_line.c)
    target_link_libraries(electrum_line_tests PRIVATE zcl_wallet_core)
    target_compile_options(electrum_line_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_electrum_line COMMAND electrum_line_tests)
    set_tests_properties(wallet_electrum_line PROPERTIES TIMEOUT 30)
    add_executable(electrum_fuzz_tests tests/test_electrum_fuzz.c tests/fuzz_electrum.c)
    target_link_libraries(electrum_fuzz_tests PRIVATE zcl_wallet_core)
    target_compile_options(electrum_fuzz_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_electrum_fuzz_regression COMMAND electrum_fuzz_tests)
    set_tests_properties(wallet_electrum_fuzz_regression PROPERTIES TIMEOUT 30)
    add_executable(electrum_history_tests tests/test_electrum_history.c)
    target_link_libraries(electrum_history_tests PRIVATE zcl_wallet_core)
    target_compile_options(electrum_history_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_electrum_history COMMAND electrum_history_tests)
    set_tests_properties(wallet_electrum_history PROPERTIES TIMEOUT 15)
    add_executable(transaction_tests tests/test_transaction.c)
    target_link_libraries(transaction_tests PRIVATE zcl_wallet_core)
    target_compile_options(transaction_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    add_test(NAME wallet_transaction COMMAND transaction_tests)
    set_tests_properties(wallet_transaction PROPERTIES TIMEOUT 15)
    add_executable(prevout_tests tests/test_prevout.c)
    target_link_libraries(prevout_tests PRIVATE zcl_wallet_core)
    target_compile_options(prevout_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    add_test(NAME wallet_prevout COMMAND prevout_tests)
    add_executable(transaction_retirement_tests tests/test_transaction_retirement.c
        src/transaction_id.c src/transaction_prevout.c)
    target_link_libraries(transaction_retirement_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_definitions(transaction_retirement_tests PRIVATE
        zcl_transaction_parse=zcl_tx_test_parse zcl_transaction_serialize=zcl_tx_test_serialize
        mbedtls_sha256=zcl_tx_test_sha256 zcl_secure_zero=zcl_tx_test_zero)
    target_compile_options(transaction_retirement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_transaction_retirement COMMAND transaction_retirement_tests)
    set_tests_properties(wallet_transaction_retirement PROPERTIES TIMEOUT 15)
    add_executable(codec_retirement_tests tests/test_codec_retirement.c
        src/transaction_read.c src/transaction_write.c)
    target_link_libraries(codec_retirement_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(codec_retirement_tests PRIVATE
        zcl_transaction_check=zcl_codec_test_check zcl_secure_zero=zcl_codec_test_zero)
    target_compile_options(codec_retirement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_codec_retirement COMMAND codec_retirement_tests)
    set_tests_properties(wallet_codec_retirement PROPERTIES TIMEOUT 15)
    set_tests_properties(wallet_prevout PROPERTIES TIMEOUT 15)
    add_executable(assessment_tests tests/test_assessment.c tests/assessment_fixture.c)
    target_link_libraries(assessment_tests PRIVATE zcl_wallet_core)
    target_compile_options(assessment_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    add_test(NAME wallet_assessment COMMAND assessment_tests)
    set_tests_properties(wallet_assessment PROPERTIES TIMEOUT 15)
    add_executable(assessment_retirement_tests tests/test_assessment_retirement.c src/transaction_assess.c)
    target_link_libraries(assessment_retirement_tests PRIVATE assessment_test_fixture)
    target_include_directories(assessment_retirement_tests PRIVATE src)
    target_compile_definitions(assessment_retirement_tests PRIVATE
        zcl_transaction_prevout=zcl_assess_test_prevout zcl_v4_source_prevout=zcl_assess_test_prevout
        zcl_source_prevout=zcl_assess_test_prevout
        zcl_address_from_script=zcl_assess_test_address
        zcl_transaction_id=zcl_assess_test_id zcl_secure_zero=zcl_assess_test_zero)
    target_compile_options(assessment_retirement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_assessment_retirement COMMAND assessment_retirement_tests)
    set_tests_properties(wallet_assessment_retirement PROPERTIES TIMEOUT 15)
    add_executable(draft_tests tests/test_draft.c tests/draft_fixture.c tests/assessment_fixture.c)
    target_link_libraries(draft_tests PRIVATE zcl_wallet_core)
    target_compile_options(draft_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow)
    add_test(NAME wallet_draft COMMAND draft_tests)
    set_tests_properties(wallet_draft PROPERTIES TIMEOUT 15)
    add_executable(draft_retirement_tests tests/test_draft_retirement.c tests/draft_fixture.c
        src/transaction_draft.c src/transaction_draft_check.c)
    target_include_directories(draft_retirement_tests PRIVATE src)
    target_link_libraries(draft_retirement_tests PRIVATE assessment_test_fixture)
    target_compile_definitions(draft_retirement_tests PRIVATE
        zcl_transaction_parse=zcl_draft_test_parse zcl_transaction_id=zcl_draft_test_id
        zcl_v4_source_assess=zcl_draft_test_assess zcl_v4_source_inspect=zcl_draft_test_source
        zcl_transaction_assess=zcl_draft_test_assess zcl_secure_zero=zcl_draft_test_zero)
    target_compile_options(draft_retirement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_draft_retirement COMMAND draft_retirement_tests)
    set_tests_properties(wallet_draft_retirement PROPERTIES TIMEOUT 15)
    add_executable(review_tests tests/test_review.c tests/assessment_fixture.c)
    target_link_libraries(review_tests PRIVATE zcl_wallet_core)
    target_compile_options(review_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow)
    add_test(NAME wallet_review COMMAND review_tests)
    set_tests_properties(wallet_review PROPERTIES TIMEOUT 15)
    # Keep fixture construction outside fault-provider name substitutions.
    add_library(assessment_test_fixture STATIC tests/assessment_fixture.c)
    target_link_libraries(assessment_test_fixture PUBLIC zcl_wallet_core)
    target_compile_options(assessment_test_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow)
    add_executable(review_retirement_tests tests/test_review_retirement.c
        src/transaction_review.c src/transaction_review_prepare.c)
    target_include_directories(review_retirement_tests PRIVATE src)
    target_link_libraries(review_retirement_tests PRIVATE assessment_test_fixture)
    target_compile_definitions(review_retirement_tests PRIVATE
        zcl_transaction_parse=zcl_review_test_parse zcl_transaction_assess=zcl_review_test_assess
        zcl_v4_source_assess=zcl_review_test_assess
        zcl_transaction_serialize=zcl_review_test_serialize zcl_secure_zero=zcl_review_test_zero)
    target_compile_options(review_retirement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_retirement COMMAND review_retirement_tests)
    set_tests_properties(wallet_review_retirement PROPERTIES TIMEOUT 15)
    add_executable(review_sighash_tests tests/test_review_sighash.c)
    target_include_directories(review_sighash_tests PRIVATE src)
    target_link_libraries(review_sighash_tests PRIVATE assessment_test_fixture)
    target_compile_options(review_sighash_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_sighash COMMAND review_sighash_tests)
    set_tests_properties(wallet_review_sighash PROPERTIES TIMEOUT 15)
    add_executable(review_sighash_failure_tests tests/test_review_sighash_failures.c
        src/transaction_review_sighash.c)
    target_include_directories(review_sighash_failure_tests PRIVATE src)
    target_link_libraries(review_sighash_failure_tests PRIVATE assessment_test_fixture)
    target_compile_definitions(review_sighash_failure_tests PRIVATE
        zcl_transaction_parse=zcl_review_hash_test_parse zcl_address_script=zcl_review_hash_test_script
        zcl_transaction_sighash_all=zcl_review_hash_test_digest zcl_secure_zero=zcl_review_hash_test_zero)
    target_compile_options(review_sighash_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_sighash_failures COMMAND review_sighash_failure_tests)
    set_tests_properties(wallet_review_sighash_failures PROPERTIES TIMEOUT 15)
    add_executable(transaction_context_tests tests/test_transaction_context.c tests/context_reference.c)
    target_include_directories(transaction_context_tests PRIVATE src)
    target_link_libraries(transaction_context_tests PRIVATE zcl_wallet_core)
    target_compile_options(transaction_context_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_transaction_context COMMAND transaction_context_tests)
    set_tests_properties(wallet_transaction_context PROPERTIES TIMEOUT 30)
    add_executable(review_context_tests tests/test_review_context.c tests/context_reference.c)
    target_include_directories(review_context_tests PRIVATE src)
    target_link_libraries(review_context_tests PRIVATE assessment_test_fixture)
    target_compile_options(review_context_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_context COMMAND review_context_tests)
    set_tests_properties(wallet_review_context PROPERTIES TIMEOUT 30)
    add_executable(review_context_failure_tests tests/test_review_context_failures.c src/transaction_review_context.c)
    target_include_directories(review_context_failure_tests PRIVATE src)
    target_link_libraries(review_context_failure_tests PRIVATE assessment_test_fixture)
    target_compile_definitions(review_context_failure_tests PRIVATE
        zcl_transaction_v4_branch=zcl_review_context_test_branch
        zcl_review_sighash_p2pkh=zcl_review_context_test_hash zcl_secure_zero=zcl_review_context_test_zero)
    target_compile_options(review_context_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_context_failures COMMAND review_context_failure_tests)
    set_tests_properties(wallet_review_context_failures PROPERTIES TIMEOUT 15)
    add_executable(signature_tests tests/test_signature.c)
    target_include_directories(signature_tests PRIVATE src)
    target_link_libraries(signature_tests PRIVATE zcl_wallet_core secp256k1)
    target_compile_options(signature_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_signature COMMAND signature_tests)
    set_tests_properties(wallet_signature PROPERTIES TIMEOUT 30)
    add_executable(signature_script_tests tests/test_signature_script.c)
    target_include_directories(signature_script_tests PRIVATE src)
    target_link_libraries(signature_script_tests PRIVATE zcl_wallet_core secp256k1 zcl_hash_provider)
    target_compile_options(signature_script_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_signature_script COMMAND signature_script_tests)
    set_tests_properties(wallet_signature_script PROPERTIES TIMEOUT 30)
    add_executable(signature_script_failure_tests tests/test_signature_script_failures.c src/signature_script.c)
    target_include_directories(signature_script_failure_tests PRIVATE src)
    target_link_libraries(signature_script_failure_tests PRIVATE zcl_wallet_core secp256k1 zcl_hash_provider)
    target_compile_definitions(signature_script_failure_tests PRIVATE
        zcl_secure_zero=zcl_script_test_zero mbedtls_sha256=zcl_script_test_sha256
        mbedtls_ripemd160=zcl_script_test_ripemd160 secp256k1_ec_pubkey_parse=zcl_script_test_key
        secp256k1_ecdsa_signature_parse_der=zcl_script_test_parse
        secp256k1_ecdsa_signature_normalize=zcl_script_test_normalize
        secp256k1_ecdsa_signature_serialize_der=zcl_script_test_serialize
        secp256k1_ecdsa_verify=zcl_script_test_verify)
    target_compile_options(signature_script_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_signature_script_failures COMMAND signature_script_failure_tests)
    set_tests_properties(wallet_signature_script_failures PROPERTIES TIMEOUT 15)
    add_library(signed_review_test_fixture STATIC tests/signed_review_fixture.c)
    target_include_directories(signed_review_test_fixture PUBLIC src)
    target_link_libraries(signed_review_test_fixture PUBLIC zcl_wallet_core zcl_hash_provider)
    target_compile_options(signed_review_test_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_executable(review_wire_tests tests/test_review_wire.c)
    target_link_libraries(review_wire_tests PRIVATE signed_review_test_fixture secp256k1)
    target_compile_options(review_wire_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_wire COMMAND review_wire_tests)
    set_tests_properties(wallet_review_wire PROPERTIES TIMEOUT 30)
    add_executable(review_complete_tests tests/test_review_complete.c)
    target_link_libraries(review_complete_tests PRIVATE signed_review_test_fixture)
    target_compile_options(review_complete_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_complete COMMAND review_complete_tests)
    set_tests_properties(wallet_review_complete PROPERTIES TIMEOUT 30)
    add_executable(review_complete_failure_tests tests/test_review_complete_failures.c
        src/transaction_review_complete.c)
    target_link_libraries(review_complete_failure_tests PRIVATE zcl_wallet_core)
    target_include_directories(review_complete_failure_tests PRIVATE src)
    target_compile_definitions(review_complete_failure_tests PRIVATE
        zcl_review_p2pkh_wire=zcl_complete_test_wire zcl_secure_zero=zcl_complete_test_zero)
    target_compile_options(review_complete_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_complete_failures COMMAND review_complete_failure_tests)
    set_tests_properties(wallet_review_complete_failures PROPERTIES TIMEOUT 15)
    add_executable(review_wire_failure_tests tests/test_review_wire_failures.c
        src/transaction_review_wire.c src/transaction_review_wire_encode.c)
    target_link_libraries(review_wire_failure_tests PRIVATE signed_review_test_fixture)
    target_compile_definitions(review_wire_failure_tests PRIVATE
        zcl_transaction_parse=zcl_wire_test_parse zcl_review_sighash_context=zcl_wire_test_hash
        zcl_signature_p2pkh=zcl_wire_test_script zcl_transaction_check=zcl_wire_test_check
        zcl_transaction_serialize=zcl_wire_test_serialize zcl_secure_zero=zcl_wire_test_zero)
    target_compile_options(review_wire_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_review_wire_failures COMMAND review_wire_failure_tests)
    set_tests_properties(wallet_review_wire_failures PROPERTIES TIMEOUT 30)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        # Host SDK reaping equipment never links into the wallet or Android.
        find_package(Threads REQUIRED)
        add_executable(emulator_reap_tests ../tools/emulator_reap.c ../tools/test_emulator_reap.c)
        target_link_libraries(emulator_reap_tests PRIVATE Threads::Threads)
        add_executable(emulator_reap_adapter_tests ../tools/emulator_reap.c
            ../tools/emulator_reap_preload.c ../tools/test_emulator_reap_preload.c)
        target_compile_definitions(emulator_reap_adapter_tests PRIVATE
            dlsym=zcl_fixture_dlsym dladdr=zcl_fixture_dladdr)
        target_link_libraries(emulator_reap_adapter_tests PRIVATE ${CMAKE_DL_LIBS})
        foreach(target emulator_reap_tests emulator_reap_adapter_tests)
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror
                -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
            if(ZCL_SANITIZE)
                target_compile_options(${target} PRIVATE -fsanitize=address,undefined
                    -fno-sanitize-recover=all -fno-omit-frame-pointer)
                target_link_options(${target} PRIVATE -fsanitize=address,undefined)
            endif()
        endforeach()
        add_test(NAME wallet_emulator_reap COMMAND emulator_reap_tests)
        add_test(NAME wallet_emulator_reap_adapter COMMAND emulator_reap_adapter_tests)
        set_tests_properties(wallet_emulator_reap wallet_emulator_reap_adapter PROPERTIES TIMEOUT 30)
        if(ZCL_FUZZ)
            add_executable(fuzz_emulator_reap ../tools/emulator_reap.c ../tools/fuzz_emulator_reap.c)
            target_compile_options(fuzz_emulator_reap PRIVATE -fsanitize=fuzzer,address,undefined
                -fno-sanitize-recover=all -Wall -Wextra -Wpedantic -Werror -Wconversion
                -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
            target_link_options(fuzz_emulator_reap PRIVATE -fsanitize=fuzzer,address,undefined)
        endif()
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(signature_failure_tests tests/test_signature_failures.c src/signature.c)
    target_include_directories(signature_failure_tests PRIVATE src)
    target_link_libraries(signature_failure_tests PRIVATE zcl_wallet_core secp256k1)
    target_compile_definitions(signature_failure_tests PRIVATE
        zcl_random_bytes=zcl_signature_test_random zcl_secure_zero=zcl_signature_test_zero
        secp256k1_ec_pubkey_create=zcl_signature_test_public
        secp256k1_ecdsa_sign=zcl_signature_test_sign secp256k1_ecdsa_signature_normalize=zcl_signature_test_normalize
        secp256k1_ec_pubkey_serialize=zcl_signature_test_public_serialize
        secp256k1_ecdsa_signature_serialize_der=zcl_signature_test_der_serialize
        secp256k1_ec_pubkey_parse=zcl_signature_test_public_parse secp256k1_ec_pubkey_cmp=zcl_signature_test_compare
        secp256k1_ecdsa_signature_parse_der=zcl_signature_test_der_parse secp256k1_ecdsa_verify=zcl_signature_test_verify
        secp256k1_nonce_function_rfc6979=zcl_signature_test_nonce)
    target_compile_options(signature_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    target_link_options(signature_failure_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=free
        -Wl,--wrap=secp256k1_context_preallocated_size -Wl,--wrap=secp256k1_context_preallocated_create
        -Wl,--wrap=secp256k1_context_randomize)
    add_test(NAME wallet_signature_failures COMMAND signature_failure_tests)
    set_tests_properties(wallet_signature_failures PROPERTIES TIMEOUT 30)
    endif()
    add_executable(sync_history_tests tests/test_sync_history.c tests/sync_fixture.c)
    target_link_libraries(sync_history_tests PRIVATE zcl_wallet_core)
    target_compile_options(sync_history_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_sync_history COMMAND sync_history_tests)
    set_tests_properties(wallet_sync_history PROPERTIES TIMEOUT 15)
    add_executable(rpc_provider_tests ${ZCL_COMMONS_SOURCE}/zjsonp/tests/test_zjsonp.c)
    target_link_libraries(rpc_provider_tests PRIVATE zcl_json_provider)
    if(ZCL_SANITIZE)
        target_compile_options(rpc_provider_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(rpc_provider_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME wallet_rpc_provider COMMAND rpc_provider_tests)
    set_tests_properties(wallet_rpc_provider PROPERTIES TIMEOUT 15)
    add_executable(camera_frame_tests tests/test_camera_frame.c tests/scan_fixture.c)
    target_link_libraries(camera_frame_tests PRIVATE zcl_wallet_core zcl_qr_provider)
    target_compile_options(camera_frame_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_camera_frame COMMAND camera_frame_tests)
    set_tests_properties(wallet_camera_frame PROPERTIES TIMEOUT 10)
    add_executable(camera_sampling_tests tests/test_camera_sampling.c tests/camera_reference.c)
    target_link_libraries(camera_sampling_tests PRIVATE zcl_wallet_core)
    target_compile_options(camera_sampling_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_camera_sampling COMMAND camera_sampling_tests)
    set_tests_properties(wallet_camera_sampling PROPERTIES TIMEOUT 20)
    add_executable(scan_provider_tests tests/test_scan_provider.c
        ${ZCL_SCAN_SOURCE}/quirc.c ${ZCL_SCAN_SOURCE}/version_db.c)
    target_include_directories(scan_provider_tests PRIVATE ${ZCL_SCAN_SOURCE})
    target_link_libraries(scan_provider_tests PRIVATE m)
    if(ZCL_SANITIZE)
        target_compile_options(scan_provider_tests PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_link_options(scan_provider_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME wallet_scan_provider COMMAND scan_provider_tests)
    set_tests_properties(wallet_scan_provider PROPERTIES TIMEOUT 10)
    add_executable(scan_qr_tests tests/test_scan_qr.c tests/scan_fixture.c)
    target_link_libraries(scan_qr_tests PRIVATE zcl_wallet_core zcl_qr_provider)
    target_compile_options(scan_qr_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_scan_qr COMMAND scan_qr_tests)
    set_tests_properties(wallet_scan_qr PROPERTIES TIMEOUT 30)
    add_executable(receive_qr_tests tests/test_receive_qr.c)
    target_link_libraries(receive_qr_tests PRIVATE zcl_wallet_core)
    target_compile_options(receive_qr_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_receive_qr COMMAND receive_qr_tests)
    if(ZCL_ORACLE)
        find_path(ZCL_SODIUM_INCLUDE sodium.h REQUIRED)
        find_library(ZCL_SODIUM_LIBRARY sodium REQUIRED)
        target_include_directories(blake2_tests PRIVATE ${ZCL_SODIUM_INCLUDE})
        target_link_libraries(blake2_tests PRIVATE ${ZCL_SODIUM_LIBRARY})
        target_compile_definitions(blake2_tests PRIVATE ZCL_BLAKE2_ORACLE=1)
        add_executable(seed_blake2_vectors tests/seed_blake2_vectors.c)
        target_include_directories(seed_blake2_vectors PRIVATE ${ZCL_SODIUM_INCLUDE})
        target_link_libraries(seed_blake2_vectors PRIVATE ${ZCL_SODIUM_LIBRARY})
        target_compile_options(seed_blake2_vectors PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        if(ZCL_SANITIZE)
            target_compile_options(seed_blake2_vectors PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(seed_blake2_vectors PRIVATE -fsanitize=address,undefined)
        endif()
        if(ZCL_FUZZ)
            target_include_directories(fuzz_blake2 PRIVATE ${ZCL_SODIUM_INCLUDE})
            target_link_libraries(fuzz_blake2 PRIVATE ${ZCL_SODIUM_LIBRARY})
            target_compile_definitions(fuzz_blake2 PRIVATE ZCL_BLAKE2_ORACLE=1)
        endif()
        # This reference-fixture oracle uses libsodium and its own bounded
        # byte reader; it includes and links no wallet parser/hash provider.
        add_executable(seed_sighash_vectors tests/seed_sighash_vectors.c)
        add_library(sighash_oracle STATIC tests/seed_sighash_vectors.c)
        target_compile_definitions(sighash_oracle PRIVATE ZCL_SIGHASH_ORACLE_LIBRARY=1)
        target_include_directories(sighash_oracle PRIVATE ${ZCL_SODIUM_INCLUDE})
        target_link_libraries(sighash_oracle PUBLIC ${ZCL_SODIUM_LIBRARY})
        target_compile_options(sighash_oracle PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_libraries(sighash_tests PRIVATE sighash_oracle)
        target_compile_definitions(sighash_tests PRIVATE ZCL_SIGHASH_ORACLE=1)
        target_link_libraries(review_sighash_tests PRIVATE sighash_oracle)
        target_compile_definitions(review_sighash_tests PRIVATE ZCL_SIGHASH_ORACLE=1)
        target_link_libraries(review_context_tests PRIVATE sighash_oracle)
        target_compile_definitions(review_context_tests PRIVATE ZCL_SIGHASH_ORACLE=1)
        target_include_directories(seed_sighash_vectors PRIVATE ${ZCL_SODIUM_INCLUDE})
        target_link_libraries(seed_sighash_vectors PRIVATE ${ZCL_SODIUM_LIBRARY})
        target_compile_options(seed_sighash_vectors PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        if(ZCL_SANITIZE)
            target_compile_options(seed_sighash_vectors PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(seed_sighash_vectors PRIVATE -fsanitize=address,undefined)
            target_compile_options(sighash_oracle PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(sighash_oracle PUBLIC -fsanitize=address,undefined)
        endif()
        if(ZCL_FUZZ)
            target_compile_options(sighash_oracle PRIVATE -fsanitize=fuzzer-no-link)
            target_link_libraries(fuzz_sighash PRIVATE sighash_oracle)
            target_compile_definitions(fuzz_sighash PRIVATE ZCL_SIGHASH_ORACLE=1)
            target_link_libraries(fuzz_review PRIVATE sighash_oracle)
            target_compile_definitions(fuzz_review PRIVATE ZCL_SIGHASH_ORACLE=1)
        endif()
        find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
        add_executable(hmac_differential_tests tests/fuzz_hmac.c tests/test_hmac_fuzz.c)
        target_include_directories(hmac_differential_tests PRIVATE src)
        target_link_libraries(hmac_differential_tests PRIVATE zcl_wallet_core OpenSSL::Crypto)
        target_compile_options(hmac_differential_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_hmac_differential COMMAND hmac_differential_tests)
        if(ZCL_FUZZ)
            add_executable(fuzz_hmac tests/fuzz_hmac.c)
            target_include_directories(fuzz_hmac PRIVATE src)
            target_link_libraries(fuzz_hmac PRIVATE zcl_wallet_core OpenSSL::Crypto)
            target_compile_options(fuzz_hmac PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
                -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
            target_link_options(fuzz_hmac PRIVATE -fsanitize=fuzzer,address,undefined)
        endif()
        add_library(bip32_oracle STATIC tests/bip32_oracle.c)
        target_include_directories(bip32_oracle PUBLIC include tests)
        target_link_libraries(bip32_oracle PUBLIC OpenSSL::Crypto)
        target_compile_options(bip32_oracle PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        if(ZCL_SANITIZE)
            target_compile_options(bip32_oracle PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(bip32_oracle PUBLIC -fsanitize=address,undefined)
        endif()
        add_executable(bip32_differential_tests tests/fuzz_bip32.c tests/test_bip32_fuzz.c)
        target_link_libraries(bip32_differential_tests PRIVATE zcl_wallet_core bip32_oracle)
        target_compile_options(bip32_differential_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_bip32_differential COMMAND bip32_differential_tests)
        if(ZCL_FUZZ)
            target_compile_options(bip32_oracle PRIVATE -fsanitize=fuzzer-no-link)
            add_executable(fuzz_bip32 tests/fuzz_bip32.c)
            target_link_libraries(fuzz_bip32 PRIVATE zcl_wallet_core bip32_oracle)
            target_compile_options(fuzz_bip32 PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
                -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
            target_link_options(fuzz_bip32 PRIVATE -fsanitize=fuzzer,address,undefined)
            add_executable(fuzz_mnemonic_seed tests/fuzz_mnemonic_seed.c)
            target_link_libraries(fuzz_mnemonic_seed PRIVATE zcl_wallet_core OpenSSL::Crypto)
            target_compile_options(fuzz_mnemonic_seed PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
                -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
            target_link_options(fuzz_mnemonic_seed PRIVATE -fsanitize=fuzzer,address,undefined)
        endif()
        add_library(signature_oracle STATIC tests/signature_oracle.c)
        target_include_directories(signature_oracle PUBLIC include src)
        target_link_libraries(signature_oracle PUBLIC OpenSSL::Crypto)
        target_compile_options(signature_oracle PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        if(ZCL_SANITIZE)
            target_compile_options(signature_oracle PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(signature_oracle PUBLIC -fsanitize=address,undefined)
        endif()
        target_link_libraries(signature_tests PRIVATE signature_oracle)
        target_compile_definitions(signature_tests PRIVATE ZCL_SIGNATURE_ORACLE=1)
        target_link_libraries(signature_script_tests PRIVATE signature_oracle)
        target_compile_definitions(signature_script_tests PRIVATE ZCL_SIGNATURE_ORACLE=1)
        target_link_libraries(review_wire_tests PRIVATE signature_oracle sighash_oracle)
        target_compile_definitions(review_wire_tests PRIVATE ZCL_SIGNATURE_ORACLE=1)
        add_library(signed_review_reference STATIC tests/signed_review_reference.c tests/context_reference.c)
        target_include_directories(signed_review_reference PUBLIC src)
        target_link_libraries(signed_review_reference PUBLIC signature_oracle sighash_oracle)
        target_compile_options(signed_review_reference PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        if(ZCL_SANITIZE)
            target_compile_options(signed_review_reference PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(signed_review_reference PUBLIC -fsanitize=address,undefined)
        endif()
        if(ZCL_FUZZ)
            target_compile_options(signature_oracle PRIVATE -fsanitize=fuzzer-no-link)
            target_link_libraries(fuzz_signature PRIVATE signature_oracle)
            target_compile_definitions(fuzz_signature PRIVATE ZCL_SIGNATURE_ORACLE=1)
            add_executable(fuzz_signature_script tests/fuzz_signature_script.c)
            target_include_directories(fuzz_signature_script PRIVATE src)
            target_link_libraries(fuzz_signature_script PRIVATE zcl_wallet_core signature_oracle)
            target_compile_options(fuzz_signature_script PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wframe-larger-than=4096)
            target_link_options(fuzz_signature_script PRIVATE -fsanitize=fuzzer,address,undefined)
            target_compile_options(signed_review_reference PRIVATE -fsanitize=fuzzer-no-link)
            add_executable(fuzz_review_wire tests/fuzz_review_wire.c)
            target_link_libraries(fuzz_review_wire PRIVATE signed_review_test_fixture signed_review_reference)
            target_compile_options(fuzz_review_wire PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wframe-larger-than=4096)
            target_link_options(fuzz_review_wire PRIVATE -fsanitize=fuzzer,address,undefined)
        endif()
        add_executable(receive_oracle_tests tests/test_receive_oracle.c)
        target_link_libraries(receive_oracle_tests PRIVATE zcl_wallet_core bip32_oracle)
        target_compile_options(receive_oracle_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
        add_test(NAME wallet_receive_oracle COMMAND receive_oracle_tests)
        add_library(change_state_oracle STATIC tests/change_state_oracle.c)
        target_include_directories(change_state_oracle PUBLIC tests)
        target_link_libraries(change_state_oracle PUBLIC OpenSSL::Crypto)
        target_compile_options(change_state_oracle PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        if(ZCL_SANITIZE)
            target_compile_options(change_state_oracle PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
            target_link_options(change_state_oracle PUBLIC -fsanitize=address,undefined)
        endif()
        if(ZCL_FUZZ)
            target_compile_options(change_state_oracle PRIVATE -fsanitize=fuzzer-no-link)
            target_link_libraries(fuzz_change_state PRIVATE change_state_oracle)
            target_compile_definitions(fuzz_change_state PRIVATE ZCL_CHANGE_STATE_ORACLE=1)
        endif()
        add_executable(change_state_oracle_tests tests/test_change_state_oracle.c)
        target_link_libraries(change_state_oracle_tests PRIVATE zcl_wallet_core change_state_oracle)
        target_compile_options(change_state_oracle_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_change_state_oracle COMMAND change_state_oracle_tests)
        set_tests_properties(wallet_change_state_oracle PROPERTIES TIMEOUT 30)
    endif()
    add_executable(wallet_tests tests/test_amount.c)
    target_link_libraries(wallet_tests PRIVATE zcl_wallet_core)
    target_compile_options(wallet_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_core COMMAND wallet_tests)
    add_executable(codec_tests tests/test_codecs.c)
    target_link_libraries(codec_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(codec_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_codecs COMMAND codec_tests)
    add_executable(address_encode_tests tests/test_address_encode.c)
    target_link_libraries(address_encode_tests PRIVATE zcl_wallet_core)
    target_compile_options(address_encode_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow)
    add_test(NAME wallet_address_encode COMMAND address_encode_tests)
    set_tests_properties(wallet_address_encode PROPERTIES TIMEOUT 15)
    add_executable(payment_tests tests/test_payment.c)
    target_link_libraries(payment_tests PRIVATE zcl_wallet_core)
    target_include_directories(payment_tests PRIVATE src)
    target_compile_options(payment_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_payment COMMAND payment_tests)
    add_executable(utf8_reference_tests tests/test_utf8_reference.c tests/utf8_reference.c)
    target_include_directories(utf8_reference_tests PRIVATE src)
    target_link_libraries(utf8_reference_tests PRIVATE zcl_wallet_core)
    target_compile_options(utf8_reference_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wshadow -Wframe-larger-than=4096)
    add_test(NAME wallet_utf8_reference COMMAND utf8_reference_tests)
    set_tests_properties(wallet_utf8_reference PROPERTIES TIMEOUT 20)
    add_executable(mnemonic_tests tests/test_mnemonic.c)
    target_link_libraries(mnemonic_tests PRIVATE zcl_wallet_core)
    target_compile_options(mnemonic_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_mnemonic COMMAND mnemonic_tests)
    add_executable(hmac_tests tests/test_hmac.c)
    target_link_libraries(hmac_tests PRIVATE zcl_wallet_core)
    target_include_directories(hmac_tests PRIVATE src)
    target_compile_options(hmac_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
        -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    if(ZCL_ORACLE)
        target_compile_definitions(hmac_tests PRIVATE ZCL_HMAC_ORACLE=1)
        target_link_libraries(hmac_tests PRIVATE OpenSSL::Crypto)
    endif()
    add_test(NAME wallet_hmac COMMAND hmac_tests)
    if(UNIX)
        # Explicit public-vector timing tool, not a ctest runtime threshold.
        add_executable(bench_mnemonic_seed tests/bench_mnemonic_seed.c)
        target_link_libraries(bench_mnemonic_seed PRIVATE zcl_wallet_core)
        target_compile_options(bench_mnemonic_seed PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_executable(bench_wallet_change tests/bench_wallet_change.c)
        target_link_libraries(bench_wallet_change PRIVATE zcl_wallet_core)
        target_compile_options(bench_wallet_change PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_executable(bench_read_only_sync tests/bench_read_only_sync.c tests/sync_fixture.c)
        target_link_libraries(bench_read_only_sync PRIVATE zcl_wallet_core)
        target_compile_options(bench_read_only_sync PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    endif()
    add_executable(bip32_tests tests/test_bip32.c)
    target_link_libraries(bip32_tests PRIVATE zcl_wallet_core)
    target_compile_options(bip32_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_bip32 COMMAND bip32_tests)
    if(ZCL_ORACLE)
        target_link_libraries(bip32_tests PRIVATE bip32_oracle)
        target_compile_definitions(bip32_tests PRIVATE ZCL_BIP32_ORACLE=1)
    endif()
    add_executable(change_tests tests/test_change.c)
    target_link_libraries(change_tests PRIVATE zcl_wallet_core)
    target_compile_options(change_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion)
    add_test(NAME wallet_change COMMAND change_tests)
    set_tests_properties(wallet_change PROPERTIES TIMEOUT 30)
    add_executable(wallet_change_tests tests/test_wallet_change.c)
    target_link_libraries(wallet_change_tests PRIVATE zcl_wallet_core)
    target_compile_options(wallet_change_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_recovered_change COMMAND wallet_change_tests)
    add_executable(transaction_source_tests tests/test_transaction_source.c)
    target_link_libraries(transaction_source_tests PRIVATE zcl_wallet_core)
    target_include_directories(transaction_source_tests PRIVATE src)
    target_compile_options(transaction_source_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_transaction_source COMMAND transaction_source_tests)
    set_tests_properties(wallet_transaction_source PROPERTIES TIMEOUT 60)
    add_executable(source_assessment_tests tests/test_source_assessment.c
        tests/source_assessment_fixture.c tests/assessment_fixture.c)
    target_link_libraries(source_assessment_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_include_directories(source_assessment_tests PRIVATE src)
    target_compile_options(source_assessment_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_source_assessment COMMAND source_assessment_tests)
    set_tests_properties(wallet_source_assessment PROPERTIES TIMEOUT 30)
    add_executable(source_assessment_contract_tests tests/test_assessment.c tests/assessment_fixture.c)
    target_link_libraries(source_assessment_contract_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(source_assessment_contract_tests PRIVATE zcl_transaction_assess=zcl_v4_source_assess)
    target_compile_options(source_assessment_contract_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_source_assessment_contract COMMAND source_assessment_contract_tests)
    set_tests_properties(wallet_source_assessment_contract PROPERTIES TIMEOUT 30)
    add_executable(source_review_tests tests/test_source_review.c
        tests/source_assessment_fixture.c tests/assessment_fixture.c)
    target_link_libraries(source_review_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_include_directories(source_review_tests PRIVATE src)
    target_compile_options(source_review_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_source_review COMMAND source_review_tests)
    set_tests_properties(wallet_source_review PROPERTIES TIMEOUT 30)
    add_executable(source_review_contract_tests tests/test_review.c tests/assessment_fixture.c)
    target_link_libraries(source_review_contract_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(source_review_contract_tests PRIVATE zcl_review_open=zcl_review_open_full_sources)
    target_compile_options(source_review_contract_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla)
    add_test(NAME wallet_source_review_contract COMMAND source_review_contract_tests)
    set_tests_properties(wallet_source_review_contract PROPERTIES TIMEOUT 30)
    add_executable(source_draft_tests tests/test_source_draft.c tests/source_assessment_fixture.c
        tests/draft_fixture.c tests/assessment_fixture.c)
    target_link_libraries(source_draft_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_include_directories(source_draft_tests PRIVATE src)
    target_compile_options(source_draft_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_source_draft COMMAND source_draft_tests)
    set_tests_properties(wallet_source_draft PROPERTIES TIMEOUT 30)
    add_executable(source_draft_contract_tests tests/test_draft.c tests/draft_fixture.c tests/assessment_fixture.c)
    target_link_libraries(source_draft_contract_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(source_draft_contract_tests PRIVATE zcl_transaction_draft=zcl_transaction_draft_full_sources)
    target_compile_options(source_draft_contract_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla)
    add_test(NAME wallet_source_draft_contract COMMAND source_draft_contract_tests)
    set_tests_properties(wallet_source_draft_contract PROPERTIES TIMEOUT 30)
    add_executable(legacy_source_tests tests/test_legacy_source.c)
    target_include_directories(legacy_source_tests PRIVATE src)
    target_link_libraries(legacy_source_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(legacy_source_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_legacy_source COMMAND legacy_source_tests)
    set_tests_properties(wallet_legacy_source PROPERTIES TIMEOUT 30)
    add_executable(legacy_source_failure_tests tests/test_legacy_source_failures.c src/transaction_source.c)
    target_include_directories(legacy_source_failure_tests PRIVATE src)
    target_link_libraries(legacy_source_failure_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_definitions(legacy_source_failure_tests PRIVATE mbedtls_sha256=zcl_legacy_test_sha
        zcl_secure_zero=zcl_legacy_test_zero)
    target_compile_options(legacy_source_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_legacy_source_failures COMMAND legacy_source_failure_tests)
    set_tests_properties(wallet_legacy_source_failures PROPERTIES TIMEOUT 30)
    if(ZCL_FUZZ)
        add_executable(fuzz_legacy_source tests/fuzz_legacy_source.c)
        target_include_directories(fuzz_legacy_source PRIVATE src)
        target_link_libraries(fuzz_legacy_source PRIVATE zcl_wallet_core zcl_hash_provider)
        target_compile_options(fuzz_legacy_source PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(fuzz_legacy_source PRIVATE -fsanitize=fuzzer,address,undefined)
        if(ZCL_ORACLE)
            target_compile_definitions(fuzz_legacy_source PRIVATE ZCL_LEGACY_ORACLE=1)
            target_link_libraries(fuzz_legacy_source PRIVATE OpenSSL::Crypto)
        endif()
    endif()
    set(ZCL_COMMITMENT_FIXTURE tests/commitment_fixture.c tests/source_assessment_fixture.c tests/assessment_fixture.c)
    set(ZCL_MIXED_SOURCE_FIXTURE tests/mixed_source_fixture.c tests/source_assessment_fixture.c tests/assessment_fixture.c)
    add_executable(mixed_source_tests tests/test_mixed_source.c ${ZCL_MIXED_SOURCE_FIXTURE})
    target_include_directories(mixed_source_tests PRIVATE src)
    target_link_libraries(mixed_source_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(mixed_source_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_mixed_source COMMAND mixed_source_tests)
    set_tests_properties(wallet_mixed_source PROPERTIES TIMEOUT 30)
    add_executable(mixed_source_failure_tests tests/test_legacy_source_failures.c src/transaction_source.c)
    target_include_directories(mixed_source_failure_tests PRIVATE src)
    target_link_libraries(mixed_source_failure_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_definitions(mixed_source_failure_tests PRIVATE ZCL_MIXED_SOURCE_TEST=1
        mbedtls_sha256=zcl_legacy_test_sha zcl_secure_zero=zcl_legacy_test_zero)
    target_compile_options(mixed_source_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_mixed_source_failures COMMAND mixed_source_failure_tests)
    set_tests_properties(wallet_mixed_source_failures PROPERTIES TIMEOUT 30)
    add_executable(mixed_commitment_tests tests/test_mixed_commitment.c ${ZCL_COMMITMENT_FIXTURE})
    target_include_directories(mixed_commitment_tests PRIVATE src)
    target_link_libraries(mixed_commitment_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(mixed_commitment_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_mixed_commitment COMMAND mixed_commitment_tests)
    set_tests_properties(wallet_mixed_commitment PROPERTIES TIMEOUT 30)
    add_executable(mixed_commitment_contract_tests tests/test_commitment.c ${ZCL_COMMITMENT_FIXTURE})
    target_include_directories(mixed_commitment_contract_tests PRIVATE src)
    target_link_libraries(mixed_commitment_contract_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_definitions(mixed_commitment_contract_tests PRIVATE zcl_v4_source_commitment_check=zcl_source_commitment_check)
    target_compile_options(mixed_commitment_contract_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_mixed_commitment_contract COMMAND mixed_commitment_contract_tests)
    set_tests_properties(wallet_mixed_commitment_contract PROPERTIES TIMEOUT 30)
    add_executable(mixed_commitment_failure_tests tests/test_commitment_failures.c ${ZCL_COMMITMENT_FIXTURE}
        src/source_commitment.c)
    target_include_directories(mixed_commitment_failure_tests PRIVATE src)
    target_link_libraries(mixed_commitment_failure_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_definitions(mixed_commitment_failure_tests PRIVATE ZCL_MIXED_SOURCE_TEST=1
        zcl_header_inspect=zcl_commitment_test_header zcl_v4_source_inspect=zcl_commitment_test_source
        zcl_source_inspect=zcl_commitment_test_source zcl_merkle_branch_check=zcl_commitment_test_branch
        zcl_secure_zero=zcl_commitment_test_zero)
    target_compile_options(mixed_commitment_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_mixed_commitment_failures COMMAND mixed_commitment_failure_tests)
    set_tests_properties(wallet_mixed_commitment_failures PROPERTIES TIMEOUT 30)
    if(ZCL_ORACLE)
        target_compile_definitions(mixed_source_tests PRIVATE ZCL_MIXED_SOURCE_ORACLE=1)
        target_link_libraries(mixed_source_tests PRIVATE OpenSSL::Crypto)
        target_compile_definitions(mixed_commitment_tests PRIVATE ZCL_COMMITMENT_ORACLE=1)
        target_link_libraries(mixed_commitment_tests PRIVATE OpenSSL::Crypto)
        target_compile_definitions(mixed_commitment_contract_tests PRIVATE ZCL_COMMITMENT_ORACLE=1)
        target_link_libraries(mixed_commitment_contract_tests PRIVATE OpenSSL::Crypto)
    endif()
    if(ZCL_FUZZ)
        add_executable(fuzz_mixed_source tests/fuzz_transaction_source.c)
        target_include_directories(fuzz_mixed_source PRIVATE src)
        target_link_libraries(fuzz_mixed_source PRIVATE zcl_wallet_core zcl_hash_provider)
        target_compile_definitions(fuzz_mixed_source PRIVATE ZCL_MIXED_SOURCE_TEST=1)
        target_compile_options(fuzz_mixed_source PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(fuzz_mixed_source PRIVATE -fsanitize=fuzzer,address,undefined)
        if(ZCL_ORACLE)
            target_compile_definitions(fuzz_mixed_source PRIVATE ZCL_SOURCE_ORACLE=1)
            target_link_libraries(fuzz_mixed_source PRIVATE OpenSSL::Crypto)
        endif()
    endif()
    add_executable(commitment_tests tests/test_commitment.c ${ZCL_COMMITMENT_FIXTURE})
    target_include_directories(commitment_tests PRIVATE src)
    target_link_libraries(commitment_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(commitment_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_source_commitment COMMAND commitment_tests)
    set_tests_properties(wallet_source_commitment PROPERTIES TIMEOUT 30)
    add_executable(commitment_failure_tests tests/test_commitment_failures.c ${ZCL_COMMITMENT_FIXTURE}
        src/source_commitment.c)
    target_include_directories(commitment_failure_tests PRIVATE src)
    target_link_libraries(commitment_failure_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_definitions(commitment_failure_tests PRIVATE zcl_header_inspect=zcl_commitment_test_header
        zcl_v4_source_inspect=zcl_commitment_test_source zcl_merkle_branch_check=zcl_commitment_test_branch
        zcl_secure_zero=zcl_commitment_test_zero)
    target_compile_options(commitment_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_source_commitment_failures COMMAND commitment_failure_tests)
    set_tests_properties(wallet_source_commitment_failures PROPERTIES TIMEOUT 30)
    if(ZCL_ORACLE)
        target_compile_definitions(commitment_tests PRIVATE ZCL_COMMITMENT_ORACLE=1)
        target_link_libraries(commitment_tests PRIVATE OpenSSL::Crypto)
    endif()
    if(ZCL_FUZZ)
        add_executable(fuzz_commitment tests/fuzz_commitment.c ${ZCL_COMMITMENT_FIXTURE})
        target_include_directories(fuzz_commitment PRIVATE src)
        target_link_libraries(fuzz_commitment PRIVATE zcl_wallet_core zcl_hash_provider)
        target_compile_options(fuzz_commitment PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(fuzz_commitment PRIVATE -fsanitize=fuzzer,address,undefined)
        if(ZCL_ORACLE)
            target_compile_definitions(fuzz_commitment PRIVATE ZCL_COMMITMENT_ORACLE=1)
            target_link_libraries(fuzz_commitment PRIVATE OpenSSL::Crypto)
        endif()
    endif()
    add_executable(header_tests tests/test_header.c tests/header_fixture.c)
    target_include_directories(header_tests PRIVATE src)
    target_link_libraries(header_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(header_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_header_inspection COMMAND header_tests)
    set_tests_properties(wallet_header_inspection PROPERTIES TIMEOUT 30)
    add_executable(header_failure_tests tests/test_header_failures.c tests/header_fixture.c
        src/header_inspect.c src/electrum_header.c)
    target_include_directories(header_failure_tests PRIVATE src)
    target_link_libraries(header_failure_tests PRIVATE zcl_wallet_core zcl_hash_provider zcl_json_provider)
    target_compile_definitions(header_failure_tests PRIVATE mbedtls_sha256=zcl_header_test_sha
        zcl_secure_zero=zcl_header_test_zero)
    target_compile_options(header_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_header_failures COMMAND header_failure_tests)
    set_tests_properties(wallet_header_failures PROPERTIES TIMEOUT 30)
    if(ZCL_ORACLE)
        target_compile_definitions(header_tests PRIVATE ZCL_HEADER_ORACLE=1)
        target_link_libraries(header_tests PRIVATE OpenSSL::Crypto)
    endif()
    if(ZCL_FUZZ)
        add_executable(fuzz_header tests/fuzz_header.c tests/header_fixture.c)
        target_include_directories(fuzz_header PRIVATE src)
        target_link_libraries(fuzz_header PRIVATE zcl_wallet_core zcl_hash_provider)
        target_compile_options(fuzz_header PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(fuzz_header PRIVATE -fsanitize=fuzzer,address,undefined)
        if(ZCL_ORACLE)
            target_compile_definitions(fuzz_header PRIVATE ZCL_HEADER_ORACLE=1)
            target_link_libraries(fuzz_header PRIVATE OpenSSL::Crypto)
        endif()
    endif()
    add_executable(merkle_tests tests/test_merkle.c tests/merkle_fixture.c)
    target_include_directories(merkle_tests PRIVATE src)
    target_link_libraries(merkle_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(merkle_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_merkle_branch COMMAND merkle_tests)
    set_tests_properties(wallet_merkle_branch PROPERTIES TIMEOUT 30)
    add_executable(merkle_failure_tests tests/test_merkle_failures.c tests/merkle_fixture.c src/transaction_merkle.c)
    target_include_directories(merkle_failure_tests PRIVATE src)
    target_link_libraries(merkle_failure_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_definitions(merkle_failure_tests PRIVATE mbedtls_sha256=zcl_merkle_test_sha
        zcl_secure_zero=zcl_merkle_test_zero)
    target_compile_options(merkle_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_merkle_failures COMMAND merkle_failure_tests)
    set_tests_properties(wallet_merkle_failures PROPERTIES TIMEOUT 30)
    if(ZCL_ORACLE)
        target_compile_definitions(merkle_tests PRIVATE ZCL_MERKLE_ORACLE=1)
        target_link_libraries(merkle_tests PRIVATE OpenSSL::Crypto)
        add_executable(seed_merkle_vectors tests/seed_merkle_vectors.c tests/merkle_fixture.c)
        target_include_directories(seed_merkle_vectors PRIVATE src)
        target_compile_definitions(seed_merkle_vectors PRIVATE ZCL_MERKLE_ORACLE=1)
        target_link_libraries(seed_merkle_vectors PRIVATE zcl_wallet_core OpenSSL::Crypto)
        target_compile_options(seed_merkle_vectors PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    endif()
    if(ZCL_FUZZ)
        add_executable(fuzz_merkle tests/fuzz_merkle.c tests/merkle_fixture.c)
        target_include_directories(fuzz_merkle PRIVATE src)
        target_link_libraries(fuzz_merkle PRIVATE zcl_wallet_core zcl_hash_provider)
        target_compile_options(fuzz_merkle PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(fuzz_merkle PRIVATE -fsanitize=fuzzer,address,undefined)
        if(ZCL_ORACLE)
            target_compile_definitions(fuzz_merkle PRIVATE ZCL_MERKLE_ORACLE=1)
            target_link_libraries(fuzz_merkle PRIVATE OpenSSL::Crypto)
        endif()
        add_executable(fuzz_source_draft tests/fuzz_draft.c tests/source_assessment_fixture.c
            tests/draft_fixture.c tests/assessment_fixture.c)
        target_link_libraries(fuzz_source_draft PRIVATE zcl_wallet_core zcl_hash_provider)
        target_include_directories(fuzz_source_draft PRIVATE src)
        target_compile_definitions(fuzz_source_draft PRIVATE ZCL_FULL_SOURCE_DRAFT=1)
        target_compile_options(fuzz_source_draft PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla)
        target_link_options(fuzz_source_draft PRIVATE -fsanitize=fuzzer,address,undefined)
        add_executable(fuzz_source_review tests/fuzz_review.c tests/source_assessment_fixture.c
            tests/assessment_fixture.c tests/context_reference.c)
        target_link_libraries(fuzz_source_review PRIVATE zcl_wallet_core zcl_hash_provider)
        target_include_directories(fuzz_source_review PRIVATE src)
        target_compile_definitions(fuzz_source_review PRIVATE ZCL_FULL_SOURCE_REVIEW=1)
        target_compile_options(fuzz_source_review PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla)
        target_link_options(fuzz_source_review PRIVATE -fsanitize=fuzzer,address,undefined)
        if(ZCL_ORACLE)
            target_link_libraries(fuzz_source_review PRIVATE sighash_oracle)
            target_compile_definitions(fuzz_source_review PRIVATE ZCL_SIGHASH_ORACLE=1)
        endif()
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(source_failure_tests tests/test_source_failures.c src/transaction_source.c)
        target_link_libraries(source_failure_tests PRIVATE zcl_wallet_core zcl_hash_provider)
        target_include_directories(source_failure_tests PRIVATE src)
        target_compile_definitions(source_failure_tests PRIVATE mbedtls_sha256=zcl_source_test_sha
            zcl_secure_zero=zcl_source_test_zero)
        target_compile_options(source_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_source_failures COMMAND source_failure_tests)
    endif()
    if(ZCL_FUZZ)
        add_executable(fuzz_transaction_source tests/fuzz_transaction_source.c)
        target_link_libraries(fuzz_transaction_source PRIVATE zcl_wallet_core)
        target_include_directories(fuzz_transaction_source PRIVATE src)
        target_compile_options(fuzz_transaction_source PRIVATE -fsanitize=fuzzer,address,undefined
            -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla)
        target_link_options(fuzz_transaction_source PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()
    set_tests_properties(wallet_recovered_change PROPERTIES TIMEOUT 30)
    add_executable(change_state_tests tests/test_change_state.c)
    target_link_libraries(change_state_tests PRIVATE zcl_wallet_core)
    target_compile_options(change_state_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_change_state COMMAND change_state_tests)
    set_tests_properties(wallet_change_state PROPERTIES TIMEOUT 30)
    add_executable(change_state_failure_tests tests/test_change_state_failures.c
        src/change_state.c src/change_state_key.c)
    target_include_directories(change_state_failure_tests PRIVATE src)
    target_link_libraries(change_state_failure_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(change_state_failure_tests PRIVATE
        zcl_hmac_sha512=zcl_change_state_test_hmac zcl_secure_zero=zcl_change_state_test_zero)
    target_compile_options(change_state_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_change_state_failures COMMAND change_state_failure_tests)
    set_tests_properties(wallet_change_state_failures PROPERTIES TIMEOUT 30)
    add_executable(wallet_record_tests tests/test_wallet_record.c src/wallet_record.c)
    target_link_libraries(wallet_record_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(wallet_record_tests PRIVATE zcl_secure_zero=zcl_record_test_zero)
    target_compile_options(wallet_record_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_record COMMAND wallet_record_tests)
    add_executable(wallet_record_fuzz_tests tests/test_wallet_record_fuzz.c tests/fuzz_wallet_record.c)
    target_link_libraries(wallet_record_fuzz_tests PRIVATE zcl_wallet_core)
    target_compile_options(wallet_record_fuzz_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_record_fuzz_contract COMMAND wallet_record_fuzz_tests)
    set_tests_properties(wallet_record_fuzz_contract PROPERTIES TIMEOUT 15)
    add_executable(wallet_header_tests tests/test_wallet_header.c src/wallet_header.c)
    target_link_libraries(wallet_header_tests PRIVATE zcl_wallet_core)
    target_compile_definitions(wallet_header_tests PRIVATE zcl_secure_zero=zcl_header_test_zero
        zcl_network_genesis=zcl_header_test_genesis zcl_address_parse=zcl_header_test_parse
        zcl_receive_from_entropy=zcl_header_test_receive)
    target_compile_options(wallet_header_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
    add_test(NAME wallet_header COMMAND wallet_header_tests)
    set_tests_properties(wallet_header PROPERTIES TIMEOUT 15)
    add_executable(custody_policy_tests tests/test_custody_policy.c)
    target_link_libraries(custody_policy_tests PRIVATE zcl_wallet_core)
    target_compile_options(custody_policy_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_custody_policy COMMAND custody_policy_tests)
    add_executable(mnemonic_confirm_tests tests/test_mnemonic_confirm.c)
    target_link_libraries(mnemonic_confirm_tests PRIVATE zcl_wallet_core)
    target_compile_options(mnemonic_confirm_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_mnemonic_confirm COMMAND mnemonic_confirm_tests)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(scan_failure_tests tests/test_scan_failures.c tests/scan_fixture.c)
        target_link_libraries(scan_failure_tests PRIVATE zcl_wallet_core zcl_qr_provider)
        target_compile_options(scan_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
        target_link_options(scan_failure_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=free)
        add_test(NAME wallet_scan_failures COMMAND scan_failure_tests)
        set_tests_properties(wallet_scan_failures PROPERTIES TIMEOUT 10)
        add_executable(storage_tests tests/test_storage.c tests/storage_fixture.c)
        target_link_libraries(storage_tests PRIVATE zcl_wallet_core)
        target_compile_options(storage_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
        add_test(NAME wallet_storage COMMAND storage_tests)
        set_tests_properties(wallet_storage PROPERTIES TIMEOUT 30)
        add_executable(storage_link_tests tests/test_storage_links.c tests/storage_fixture.c)
        target_link_libraries(storage_link_tests PRIVATE zcl_wallet_core)
        target_compile_options(storage_link_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(storage_link_tests PRIVATE -Wl,--wrap=fstat)
        add_test(NAME wallet_storage_links COMMAND storage_link_tests)
        set_tests_properties(wallet_storage_links PROPERTIES TIMEOUT 30)
        add_executable(storage_alias_tests tests/test_storage_aliases.c tests/storage_fixture.c)
        target_link_libraries(storage_alias_tests PRIVATE zcl_wallet_core)
        target_compile_options(storage_alias_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_storage_aliases COMMAND storage_alias_tests)
        set_tests_properties(wallet_storage_aliases PROPERTIES TIMEOUT 30)
        add_executable(change_storage_tests tests/test_change_storage.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(change_storage_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_storage_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        add_test(NAME wallet_change_storage COMMAND change_storage_tests)
        set_tests_properties(wallet_change_storage PROPERTIES TIMEOUT 30)
        # Keep independent cases in separate translation units: optimized
        # sanitizer builds must retain the same 4096-byte frame bound.
        add_executable(storage_orphan_tests tests/test_storage_orphan.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(storage_orphan_tests PRIVATE zcl_wallet_core)
        target_compile_options(storage_orphan_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_storage_orphan COMMAND storage_orphan_tests)
        set_tests_properties(wallet_storage_orphan PROPERTIES TIMEOUT 30)
        add_executable(change_probe_tests tests/test_change_probe.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(change_probe_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_probe_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        add_test(NAME wallet_change_probe COMMAND change_probe_tests)
        set_tests_properties(wallet_change_probe PROPERTIES TIMEOUT 30)
        add_executable(change_probe_fault_tests tests/test_change_probe_faults.c
            tests/change_storage_fixture.c tests/storage_fixture.c tests/storage_faults.c)
        target_link_libraries(change_probe_fault_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_probe_fault_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(change_probe_fault_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk -Wl,--wrap=fstat
            -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_change_probe_faults COMMAND change_probe_fault_tests)
        set_tests_properties(wallet_change_probe_faults PROPERTIES TIMEOUT 30)
        add_executable(change_repair_tests tests/test_change_repair.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_include_directories(change_repair_tests PRIVATE src)
        target_link_libraries(change_repair_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_repair_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        add_test(NAME wallet_change_repair COMMAND change_repair_tests)
        set_tests_properties(wallet_change_repair PROPERTIES TIMEOUT 30)
        add_executable(change_repair_fault_tests tests/test_change_repair_faults.c
            tests/change_storage_fixture.c tests/storage_fixture.c tests/storage_faults.c)
        target_include_directories(change_repair_fault_tests PRIVATE src)
        target_link_libraries(change_repair_fault_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_repair_fault_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(change_repair_fault_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk
            -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_change_repair_faults COMMAND change_repair_fault_tests)
        set_tests_properties(wallet_change_repair_faults PROPERTIES TIMEOUT 30)
        add_executable(change_repair_crash_tests tests/test_change_repair_crash.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_include_directories(change_repair_crash_tests PRIVATE src)
        target_link_libraries(change_repair_crash_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_repair_crash_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(change_repair_crash_tests PRIVATE -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=close)
        add_test(NAME wallet_change_repair_crash COMMAND change_repair_crash_tests)
        set_tests_properties(wallet_change_repair_crash PROPERTIES TIMEOUT 30)
        add_executable(change_recovery_tests tests/test_change_recovery.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(change_recovery_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_recovery_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        # Each exhaustive matrix retains every case and the original deadline.
        add_test(NAME wallet_change_recovery COMMAND change_recovery_tests bounds)
        add_test(NAME wallet_change_recovery_partial COMMAND change_recovery_tests partial)
        add_test(NAME wallet_change_recovery_corrupt COMMAND change_recovery_tests corrupt)
        set_tests_properties(wallet_change_recovery wallet_change_recovery_partial
            wallet_change_recovery_corrupt PROPERTIES TIMEOUT 30)
        add_executable(change_recovery_failure_tests tests/test_change_recovery_failures.c
            src/change_recovery.c src/change_custody.c tests/change_custody_retirement.c
            tests/change_storage_fixture.c tests/storage_fixture.c tests/storage_faults.c)
        target_link_libraries(change_recovery_failure_tests PRIVATE zcl_wallet_core)
        target_link_options(change_recovery_failure_tests PRIVATE -Wl,--wrap=zcl_change_custody_prepare)
        target_compile_definitions(change_recovery_failure_tests PRIVATE
            zcl_random_bytes=zcl_recovery_test_random zcl_secure_zero=zcl_recovery_test_zero
            zcl_change_state_encode=zcl_recovery_test_encode zcl_change_state_decode=zcl_recovery_test_decode)
        target_compile_options(change_recovery_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(change_recovery_failure_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk
            -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_change_recovery_failures COMMAND change_recovery_failure_tests)
        set_tests_properties(wallet_change_recovery_failures PROPERTIES TIMEOUT 30)
        add_executable(change_recovery_crash_tests tests/test_change_recovery_crash.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(change_recovery_crash_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_recovery_crash_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(change_recovery_crash_tests PRIVATE -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=close)
        add_test(NAME wallet_change_recovery_crash COMMAND change_recovery_crash_tests)
        set_tests_properties(wallet_change_recovery_crash PROPERTIES TIMEOUT 30)
        add_executable(change_reservation_tests tests/test_change_reservation.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(change_reservation_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_reservation_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        add_test(NAME wallet_change_reservation COMMAND change_reservation_tests)
        set_tests_properties(wallet_change_reservation PROPERTIES TIMEOUT 30)
        add_executable(change_ownership_tests tests/test_change_ownership.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(change_ownership_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_ownership_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_change_ownership COMMAND change_ownership_tests)
        set_tests_properties(wallet_change_ownership PROPERTIES TIMEOUT 60)
        add_library(review_wallet_test_fixture STATIC tests/review_wallet_fixture.c
            tests/assessment_fixture.c tests/change_storage_fixture.c tests/storage_fixture.c)
        target_include_directories(review_wallet_test_fixture PUBLIC src)
        target_link_libraries(review_wallet_test_fixture PUBLIC zcl_wallet_core)
        target_compile_options(review_wallet_test_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096)
        add_executable(review_wallet_tests tests/test_review_wallet.c)
        target_link_libraries(review_wallet_tests PRIVATE review_wallet_test_fixture)
        target_compile_options(review_wallet_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_review_wallet COMMAND review_wallet_tests)
        set_tests_properties(wallet_review_wallet PROPERTIES TIMEOUT 60)
        add_executable(review_wallet_sign_tests tests/test_review_wallet_sign.c)
        target_link_libraries(review_wallet_sign_tests PRIVATE review_wallet_test_fixture)
        target_compile_options(review_wallet_sign_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        if(ZCL_ORACLE)
            target_link_libraries(review_wallet_sign_tests PRIVATE signature_oracle sighash_oracle)
            target_compile_definitions(review_wallet_sign_tests PRIVATE ZCL_SIGNATURE_ORACLE=1)
        endif()
        add_test(NAME wallet_review_wallet_sign COMMAND review_wallet_sign_tests)
        set_tests_properties(wallet_review_wallet_sign PROPERTIES TIMEOUT 60)
        add_executable(review_transaction_sign_tests tests/test_review_transaction_sign.c)
        add_executable(review_change_tests tests/test_review_change.c)
        target_link_libraries(review_change_tests PRIVATE review_wallet_test_fixture)
        target_compile_options(review_change_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_review_change COMMAND review_change_tests)
        set_tests_properties(wallet_review_change PROPERTIES TIMEOUT 60)
        target_link_libraries(review_transaction_sign_tests PRIVATE review_wallet_test_fixture)
        target_compile_options(review_transaction_sign_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_review_transaction_sign COMMAND review_transaction_sign_tests)
        set_tests_properties(wallet_review_transaction_sign PROPERTIES TIMEOUT 60)
        add_executable(review_transaction_sign_failure_tests tests/test_review_transaction_sign_failures.c
            src/transaction_review_sign.c)
        target_link_libraries(review_transaction_sign_failure_tests PRIVATE zcl_wallet_core)
        target_include_directories(review_transaction_sign_failure_tests PRIVATE src)
        target_compile_definitions(review_transaction_sign_failure_tests PRIVATE
            zcl_review_input_wallet_check=zcl_batch_test_check zcl_review_input_wallet_sign=zcl_batch_test_sign
            zcl_review_p2pkh_complete=zcl_batch_test_complete zcl_secure_zero=zcl_batch_test_zero)
        target_compile_options(review_transaction_sign_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_review_transaction_sign_failures COMMAND review_transaction_sign_failure_tests)
        add_executable(review_wallet_sign_failure_tests tests/test_review_wallet_sign_failures.c
            src/transaction_review_wallet.c)
        target_link_libraries(review_wallet_sign_failure_tests PRIVATE review_wallet_test_fixture secp256k1)
        target_compile_definitions(review_wallet_sign_failure_tests PRIVATE
            zcl_entropy_seed=zcl_sign_test_seed zcl_seed_private=zcl_sign_test_private
            zcl_signature_create=zcl_sign_test_create zcl_signature_p2pkh=zcl_sign_test_verify
            zcl_random_bytes=zcl_sign_test_random zcl_ec_begin=zcl_sign_test_begin zcl_ec_end=zcl_sign_test_end
            zcl_secure_zero=zcl_sign_test_zero)
        target_compile_options(review_wallet_sign_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_review_wallet_sign_failures COMMAND review_wallet_sign_failure_tests)
        set_tests_properties(wallet_review_wallet_sign_failures PROPERTIES TIMEOUT 60)
        add_executable(review_wallet_failure_tests tests/test_review_wallet_failures.c src/transaction_review_wallet.c)
        target_link_libraries(review_wallet_failure_tests PRIVATE review_wallet_test_fixture secp256k1)
        target_compile_definitions(review_wallet_failure_tests PRIVATE
            zcl_change_custody_prepare=zcl_review_wallet_test_prepare zcl_address_encode=zcl_review_wallet_test_encode
            zcl_storage_read=zcl_review_wallet_test_read zcl_random_bytes=zcl_review_wallet_test_random
            zcl_wallet_recovered_address=zcl_review_wallet_test_receive
            zcl_wallet_change_reserved_address=zcl_review_wallet_test_change zcl_secure_zero=zcl_review_wallet_test_zero)
        target_compile_options(review_wallet_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_review_wallet_failures COMMAND review_wallet_failure_tests)
        set_tests_properties(wallet_review_wallet_failures PROPERTIES TIMEOUT 30)
        add_executable(review_wallet_fuzz_tests tests/fuzz_review_wallet.c)
        target_link_libraries(review_wallet_fuzz_tests PRIVATE review_wallet_test_fixture)
        target_compile_definitions(review_wallet_fuzz_tests PRIVATE ZCL_REVIEW_WALLET_REGRESSION=1)
        target_compile_options(review_wallet_fuzz_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096)
        add_test(NAME wallet_review_wallet_fuzz_regression COMMAND review_wallet_fuzz_tests)
        set_tests_properties(wallet_review_wallet_fuzz_regression PROPERTIES TIMEOUT 30)
        if(ZCL_FUZZ)
            add_executable(fuzz_review_wallet tests/fuzz_review_wallet.c)
            target_link_libraries(fuzz_review_wallet PRIVATE review_wallet_test_fixture)
            target_compile_options(fuzz_review_wallet PRIVATE -fsanitize=fuzzer,address,undefined
                -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wframe-larger-than=4096)
            target_link_options(fuzz_review_wallet PRIVATE -fsanitize=fuzzer,address,undefined)
        endif()
        add_executable(change_ownership_failure_tests tests/test_change_ownership_failures.c
            src/change_ownership.c src/change_custody.c tests/change_custody_retirement.c
            tests/change_storage_fixture.c tests/storage_fixture.c tests/storage_faults.c)
        target_link_libraries(change_ownership_failure_tests PRIVATE zcl_wallet_core)
        target_link_options(change_ownership_failure_tests PRIVATE -Wl,--wrap=zcl_change_custody_prepare)
        target_compile_definitions(change_ownership_failure_tests PRIVATE
            zcl_random_bytes=zcl_ownership_test_random zcl_secure_zero=zcl_ownership_test_zero
            zcl_change_state_decode=zcl_ownership_test_decode zcl_wallet_recovered_change=zcl_ownership_test_derive)
        target_compile_options(change_ownership_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wvla -Wframe-larger-than=4096)
        target_link_options(change_ownership_failure_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk
            -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=renameat2 -Wl,--wrap=close -Wl,--wrap=fstat
            -Wl,--wrap=zcl_storage_change_observe)
        add_test(NAME wallet_change_ownership_failures COMMAND change_ownership_failure_tests)
        set_tests_properties(wallet_change_ownership_failures PROPERTIES TIMEOUT 30)
        add_executable(change_reservation_failure_tests tests/test_change_reservation_failures.c
            src/change_reservation.c src/change_custody.c tests/change_custody_retirement.c
            tests/change_storage_fixture.c tests/storage_fixture.c tests/storage_faults.c)
        target_link_libraries(change_reservation_failure_tests PRIVATE zcl_wallet_core)
        target_link_options(change_reservation_failure_tests PRIVATE -Wl,--wrap=zcl_change_custody_prepare)
        target_compile_definitions(change_reservation_failure_tests PRIVATE
            zcl_random_bytes=zcl_reservation_test_random zcl_secure_zero=zcl_reservation_test_zero
            zcl_change_state_encode=zcl_reservation_test_encode zcl_change_state_decode=zcl_reservation_test_decode
            zcl_wallet_recovered_change=zcl_reservation_test_derive)
        target_compile_options(change_reservation_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(change_reservation_failure_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk
            -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_change_reservation_failures COMMAND change_reservation_failure_tests)
        set_tests_properties(wallet_change_reservation_failures PROPERTIES TIMEOUT 30)
        add_executable(storage_fault_tests tests/test_storage_faults.c tests/storage_fixture.c tests/storage_faults.c)
        target_link_libraries(storage_fault_tests PRIVATE zcl_wallet_core)
        target_compile_options(storage_fault_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(storage_fault_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk -Wl,--wrap=write -Wl,--wrap=fsync
            -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_storage_faults COMMAND storage_fault_tests)
        set_tests_properties(wallet_storage_faults PROPERTIES TIMEOUT 30)
        add_executable(storage_retirement_tests tests/test_storage_retirement.c
            tests/storage_fixture.c tests/storage_faults.c src/storage_read.c src/storage_write.c)
        target_include_directories(storage_retirement_tests PRIVATE src)
        target_link_libraries(storage_retirement_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(storage_retirement_tests PRIVATE zcl_secure_zero=zcl_storage_test_zero)
        target_compile_options(storage_retirement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(storage_retirement_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread
            -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk -Wl,--wrap=write -Wl,--wrap=fsync
            -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_storage_retirement COMMAND storage_retirement_tests)
        set_tests_properties(wallet_storage_retirement PROPERTIES TIMEOUT 15)
        add_executable(change_storage_retirement_tests tests/test_change_storage_retirement.c
            tests/change_storage_fixture.c tests/storage_fixture.c tests/storage_faults.c
            src/storage_change_read.c src/storage_change_write.c src/storage_change_repair.c)
        target_include_directories(change_storage_retirement_tests PRIVATE src)
        target_link_libraries(change_storage_retirement_tests PRIVATE zcl_wallet_core)
        target_compile_definitions(change_storage_retirement_tests PRIVATE
            zcl_secure_zero=zcl_change_storage_test_zero zcl_store_write_bytes=zcl_change_storage_test_write_bytes)
        target_compile_options(change_storage_retirement_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(change_storage_retirement_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread
            -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk -Wl,--wrap=write -Wl,--wrap=fsync
            -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_change_storage_retirement COMMAND change_storage_retirement_tests)
        set_tests_properties(wallet_change_storage_retirement PROPERTIES TIMEOUT 15)
        add_executable(change_storage_fault_tests tests/test_change_storage_faults.c
            tests/change_storage_fixture.c tests/storage_fixture.c tests/storage_faults.c)
        target_link_libraries(change_storage_fault_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_storage_fault_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(change_storage_fault_tests PRIVATE -Wl,--wrap=read -Wl,--wrap=pread -Wl,--wrap=__read_chk -Wl,--wrap=__pread_chk
            -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_change_storage_faults COMMAND change_storage_fault_tests)
        set_tests_properties(wallet_change_storage_faults PROPERTIES TIMEOUT 30)
        add_executable(change_storage_crash_tests tests/test_change_storage_crash.c
            tests/change_storage_fixture.c tests/storage_fixture.c)
        target_link_libraries(change_storage_crash_tests PRIVATE zcl_wallet_core)
        target_compile_options(change_storage_crash_tests PRIVATE -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wsign-conversion -Wframe-larger-than=4096)
        target_link_options(change_storage_crash_tests PRIVATE -Wl,--wrap=write
            -Wl,--wrap=fsync -Wl,--wrap=renameat2 -Wl,--wrap=close)
        add_test(NAME wallet_change_storage_crash COMMAND change_storage_crash_tests)
        set_tests_properties(wallet_change_storage_crash PROPERTIES TIMEOUT 30)
        add_executable(storage_crash_tests tests/test_storage_crash.c tests/storage_fixture.c)
        target_link_libraries(storage_crash_tests PRIVATE zcl_wallet_core)
        target_compile_options(storage_crash_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
        target_link_options(storage_crash_tests PRIVATE -Wl,--wrap=write -Wl,--wrap=fsync -Wl,--wrap=renameat2)
        add_test(NAME wallet_storage_crash COMMAND storage_crash_tests)
        set_tests_properties(wallet_storage_crash PROPERTIES TIMEOUT 30)
        add_executable(secret_failure_tests tests/test_secret_failures.c)
        target_link_libraries(secret_failure_tests PRIVATE zcl_wallet_core zcl_hash_provider secp256k1)
        target_include_directories(secret_failure_tests PRIVATE src)
        target_compile_options(secret_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
            -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(secret_failure_tests PRIVATE
            -Wl,--wrap=mbedtls_sha512_init -Wl,--wrap=mbedtls_sha512_free -Wl,--wrap=mbedtls_sha512_clone
            -Wl,--wrap=mbedtls_sha512_starts -Wl,--wrap=mbedtls_sha512_update
            -Wl,--wrap=mbedtls_sha512_finish -Wl,--wrap=mbedtls_sha256 -Wl,--wrap=mbedtls_ripemd160
            -Wl,--wrap=mbedtls_platform_zeroize -Wl,--wrap=zcl_entropy_seed -Wl,--wrap=zcl_seed_address
            -Wl,--wrap=zcl_ec_public -Wl,--wrap=zcl_ec_end)
        add_test(NAME wallet_secret_failures COMMAND secret_failure_tests)
        add_executable(random_tests tests/test_random.c)
        target_link_libraries(random_tests PRIVATE zcl_wallet_core)
        target_compile_options(random_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
            -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(random_tests PRIVATE -Wl,--wrap=getrandom -Wl,--wrap=zcl_secure_zero)
        add_test(NAME wallet_random COMMAND random_tests)
        add_executable(key_failure_tests tests/test_key_failures.c)
        target_link_libraries(key_failure_tests PRIVATE zcl_wallet_core secp256k1)
        target_include_directories(key_failure_tests PRIVATE src)
        target_compile_options(key_failure_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
            -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(key_failure_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=free
            -Wl,--wrap=secp256k1_context_preallocated_size -Wl,--wrap=secp256k1_context_preallocated_create
            -Wl,--wrap=secp256k1_context_randomize -Wl,--wrap=zcl_hmac_sha512
            -Wl,--wrap=zcl_secure_zero -Wl,--wrap=secp256k1_ec_seckey_tweak_add)
        add_test(NAME wallet_key_failures COMMAND key_failure_tests)
        add_executable(ec_lifetime_tests tests/test_ec_lifetime.c)
        target_link_libraries(ec_lifetime_tests PRIVATE zcl_wallet_core secp256k1)
        target_include_directories(ec_lifetime_tests PRIVATE src)
        target_compile_options(ec_lifetime_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion
            -Wformat=2 -Wshadow -Wvla -Wframe-larger-than=4096)
        target_link_options(ec_lifetime_tests PRIVATE -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=zcl_secure_zero
            -Wl,--wrap=secp256k1_context_preallocated_size -Wl,--wrap=secp256k1_context_preallocated_create
            -Wl,--wrap=secp256k1_context_preallocated_destroy -Wl,--wrap=secp256k1_context_randomize
            -Wl,--wrap=secp256k1_ec_pubkey_create -Wl,--wrap=secp256k1_ec_pubkey_serialize)
        add_test(NAME wallet_ec_lifetime COMMAND ec_lifetime_tests)
    endif()

    add_test(NAME wallet_test_deadlines COMMAND ${CMAKE_COMMAND}
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR} -DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}
        -P ${CMAKE_CURRENT_LIST_DIR}/tests/check_test_deadlines.cmake)
    set_tests_properties(wallet_test_deadlines PROPERTIES TIMEOUT 15)
    add_test(NAME wallet_test_deadline_contract COMMAND ${CMAKE_COMMAND}
        -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-deadline-contract
        -DCHECK_SCRIPT=${CMAKE_CURRENT_LIST_DIR}/tests/check_test_deadlines.cmake
        -DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}
        -P ${CMAKE_CURRENT_LIST_DIR}/tests/test_test_deadlines.cmake)
    set_tests_properties(wallet_test_deadline_contract PROPERTIES TIMEOUT 15)
    # Bound newly registered fixtures too. Explicit per-group limits remain
    # authoritative; an absent limit receives one minute, never no timeout.
    get_property(_zcl_tests DIRECTORY PROPERTY TESTS)
    foreach(_zcl_test IN LISTS _zcl_tests)
        get_property(_zcl_deadline_set TEST "${_zcl_test}" PROPERTY TIMEOUT SET)
        if(NOT _zcl_deadline_set)
            set_tests_properties("${_zcl_test}" PROPERTIES TIMEOUT 60)
        endif()
        get_test_property("${_zcl_test}" TIMEOUT _zcl_deadline)
        if(NOT _zcl_deadline MATCHES "^[1-9][0-9]*$")
            message(FATAL_ERROR "${_zcl_test} needs a positive whole-second execution deadline")
        endif()
    endforeach()
endif()
