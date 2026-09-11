# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
cmake_minimum_required(VERSION 3.22)
project(zclassic_wallet VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ZCL_SANITIZE "Enable host ASan and UBSan" OFF)
option(ZCL_FUZZ "Build Clang libFuzzer harnesses" OFF)
option(ZCL_JNI "Build the thin JNI bridge" OFF)

set(ZCL_MBEDTLS_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../vendor/android-mbedtls")
add_library(zcl_hash_provider STATIC
    ${ZCL_MBEDTLS_SOURCE}/library/sha256.c ${ZCL_MBEDTLS_SOURCE}/library/sha512.c
    ${ZCL_MBEDTLS_SOURCE}/library/ripemd160.c ${ZCL_MBEDTLS_SOURCE}/library/platform_util.c)
target_include_directories(zcl_hash_provider PUBLIC ${ZCL_MBEDTLS_SOURCE}/include PRIVATE include)
target_compile_definitions(zcl_hash_provider PUBLIC MBEDTLS_CONFIG_FILE="zcl_mbedtls_config.h")
set_target_properties(zcl_hash_provider PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(zcl_wallet_core STATIC src/amount.c src/base58.c src/address.c
    src/payment.c src/uri_text.c src/utf8_text.c)
target_include_directories(zcl_wallet_core PUBLIC include)
target_link_libraries(zcl_wallet_core PRIVATE zcl_hash_provider)
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
    target_compile_options(zcl_wallet_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(zcl_hash_provider PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(zcl_wallet_core PUBLIC -fsanitize=address,undefined)
endif()

if(ZCL_JNI OR ANDROID)
    add_library(zclwallet_jni SHARED src/jni_amount.c src/jni_codecs.c src/jni_payment.c src/jni_support.c)
    target_link_libraries(zclwallet_jni PRIVATE zcl_wallet_core)
    target_compile_options(zclwallet_jni PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wsign-conversion -Wformat=2 -Wshadow -Wvla -fstack-protector-strong)
    target_compile_options(zclwallet_jni PRIVATE -Wframe-larger-than=4096)
    if(NOT ANDROID)
        find_package(JNI REQUIRED COMPONENTS JVM)
        target_include_directories(zclwallet_jni PRIVATE ${JNI_INCLUDE_DIRS})
    endif()
    target_link_options(zclwallet_jni PRIVATE -Wl,-z,relro,-z,now,-z,noexecstack)
endif()

if(ZCL_FUZZ)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR ANDROID)
        message(FATAL_ERROR "Fuzzing requires host Clang")
    endif()
    target_compile_options(zcl_wallet_core PRIVATE -fsanitize=fuzzer-no-link)
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
    add_executable(fuzz_payment tests/fuzz_payment.c)
    target_link_libraries(fuzz_payment PRIVATE zcl_wallet_core)
    target_compile_options(fuzz_payment PRIVATE -fsanitize=fuzzer,address,undefined
        -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    target_link_options(fuzz_payment PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(NOT ANDROID)
    enable_testing()
    add_executable(wallet_tests tests/test_amount.c)
    target_link_libraries(wallet_tests PRIVATE zcl_wallet_core)
    target_compile_options(wallet_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_core COMMAND wallet_tests)
    add_executable(codec_tests tests/test_codecs.c)
    target_link_libraries(codec_tests PRIVATE zcl_wallet_core zcl_hash_provider)
    target_compile_options(codec_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_codecs COMMAND codec_tests)
    add_executable(payment_tests tests/test_payment.c)
    target_link_libraries(payment_tests PRIVATE zcl_wallet_core)
    target_include_directories(payment_tests PRIVATE src)
    target_compile_options(payment_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion)
    add_test(NAME wallet_payment COMMAND payment_tests)
endif()
