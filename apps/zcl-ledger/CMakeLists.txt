# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
cmake_minimum_required(VERSION 3.21)
project(zcl_ledger C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(ledger_hid STATIC src/ledger_hid.c src/ledger_probe.c)
target_include_directories(ledger_hid PUBLIC include)
target_compile_options(ledger_hid PRIVATE -Wall -Wextra -Werror -pedantic)

find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
find_package(PNG REQUIRED)
add_library(zcl_base58 STATIC src/zcl_base58.c)
target_include_directories(zcl_base58 PUBLIC include)
target_compile_options(zcl_base58 PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_address STATIC src/zcl_address.c)
target_include_directories(zcl_address PUBLIC include)
target_link_libraries(zcl_address PUBLIC zcl_base58 OpenSSL::Crypto)
target_compile_options(zcl_address PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_sign_test STATIC src/zcl_sign_test.c)
target_include_directories(zcl_sign_test PUBLIC include)
target_link_libraries(zcl_sign_test PUBLIC OpenSSL::Crypto)
target_compile_options(zcl_sign_test PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(zcl-blue-sign-test src/blue_sign_test_cli.c)
target_link_libraries(zcl-blue-sign-test PRIVATE ledger_hid zcl_address zcl_sign_test)
target_compile_options(zcl-blue-sign-test PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_tx_review STATIC src/zcl_tx_review.c)
target_include_directories(zcl_tx_review PUBLIC include)
target_compile_options(zcl_tx_review PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_tx_script_facts STATIC src/zcl_tx_script_facts.c)
target_include_directories(zcl_tx_script_facts PUBLIC include)
target_link_libraries(zcl_tx_script_facts PUBLIC zcl_tx_review)
target_compile_options(zcl_tx_script_facts PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_zip243 STATIC src/zcl_zip243.c)
target_include_directories(zcl_zip243 PUBLIC include)
target_link_libraries(zcl_zip243 PUBLIC zcl_tx_review)
target_compile_options(zcl_zip243 PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_zip243_host_hash STATIC
    src/zcl_zip243_host.c
    ../../core/modules/crypto/src/blake2b.c
    ../../platform/modules/base/src/log_level.c)
target_include_directories(zcl_zip243_host_hash PUBLIC
    include
    ../../core/modules/crypto/include
    ../../platform/modules/util/include
    ../../platform/modules/base/include)
target_compile_definitions(zcl_zip243_host_hash PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(zcl_zip243_host_hash PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_tx_prevout STATIC src/zcl_tx_prevout.c)
target_include_directories(zcl_tx_prevout PUBLIC include)
target_link_libraries(zcl_tx_prevout PUBLIC zcl_tx_script_facts zcl_zip243)
target_compile_options(zcl_tx_prevout PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_tx_stream STATIC src/zcl_tx_stream.c)
target_include_directories(zcl_tx_stream PUBLIC include)
target_compile_options(zcl_tx_stream PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_tx_stream_zip243 STATIC src/zcl_tx_stream_zip243.c)
target_include_directories(zcl_tx_stream_zip243 PUBLIC include)
target_link_libraries(zcl_tx_stream_zip243 PUBLIC zcl_tx_stream)
target_compile_options(zcl_tx_stream_zip243 PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(zcl_tx_replay_zip243 STATIC src/zcl_tx_replay_zip243.c)
target_include_directories(zcl_tx_replay_zip243 PUBLIC include)
target_link_libraries(zcl_tx_replay_zip243 PUBLIC zcl_tx_stream)
target_compile_options(zcl_tx_replay_zip243 PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_payment_review STATIC src/blue_payment_review.c)
target_include_directories(blue_payment_review PUBLIC include)
target_link_libraries(blue_payment_review PUBLIC zcl_tx_replay_zip243)
target_compile_options(blue_payment_review PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_payment_screen STATIC src/blue_payment_screen.c)
target_include_directories(blue_payment_screen PUBLIC include)
target_link_libraries(blue_payment_screen PUBLIC blue_payment_review zcl_base58)
target_compile_options(blue_payment_screen PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_payment_apdu STATIC src/blue_payment_apdu.c)
target_include_directories(blue_payment_apdu PUBLIC include)
target_link_libraries(blue_payment_apdu PUBLIC blue_payment_screen)
target_compile_options(blue_payment_apdu PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_review_protocol STATIC src/blue_review_protocol.c)
target_include_directories(blue_review_protocol PUBLIC include)
target_link_libraries(blue_review_protocol PUBLIC zcl_tx_review zcl_zip243)
target_compile_options(blue_review_protocol PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_review_screen STATIC src/blue_review_screen.c)
target_include_directories(blue_review_screen PUBLIC include)
target_link_libraries(blue_review_screen PUBLIC zcl_tx_script_facts zcl_base58)
target_compile_options(blue_review_screen PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_review_app STATIC device-blue-review/src/blue_review_app.c
    src/blue_review_accessible.c)
target_include_directories(blue_review_app PUBLIC include)
target_link_libraries(blue_review_app PUBLIC blue_review_protocol blue_review_screen)
target_compile_options(blue_review_app PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_review_simulate STATIC src/blue_review_simulate.c)
target_include_directories(blue_review_simulate PUBLIC include)
target_link_libraries(blue_review_simulate PUBLIC blue_review_app zcl_zip243_host_hash OpenSSL::Crypto)
target_compile_options(blue_review_simulate PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(zcl-tx-review src/tx_review_cli.c)
target_link_libraries(zcl-tx-review PRIVATE zcl_address zcl_tx_review zcl_tx_script_facts zcl_zip243_host_hash blue_review_protocol blue_review_simulate ledger_hid OpenSSL::Crypto)
target_compile_options(zcl-tx-review PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_bagl_canvas STATIC src/blue_bagl_canvas.c)
target_include_directories(blue_bagl_canvas PUBLIC include)
target_link_libraries(blue_bagl_canvas PUBLIC PNG::PNG)
target_compile_options(blue_bagl_canvas PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_payment_render STATIC src/blue_payment_render.c)
target_include_directories(blue_payment_render PUBLIC include)
target_link_libraries(blue_payment_render PUBLIC blue_payment_screen blue_bagl_canvas)
target_compile_options(blue_payment_render PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_payment_simulate STATIC src/blue_payment_simulate.c)
target_include_directories(blue_payment_simulate PUBLIC include)
target_link_libraries(blue_payment_simulate PUBLIC blue_payment_screen
    zcl_zip243_host_hash OpenSSL::Crypto)
target_compile_options(blue_payment_simulate PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_payment_live STATIC src/blue_payment_live.c)
target_include_directories(blue_payment_live PUBLIC include)
target_link_libraries(blue_payment_live PUBLIC blue_payment_simulate
    zcl_tx_stream OpenSSL::Crypto)
target_compile_options(blue_payment_live PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(zcl-blue-wallet-review src/blue_wallet_review_cli.c)
target_link_libraries(zcl-blue-wallet-review PRIVATE blue_payment_live
    ledger_hid)
target_compile_options(zcl-blue-wallet-review PRIVATE
    -Wall -Wextra -Werror -pedantic)

add_executable(zcl-blue-payment-sim src/blue_payment_sim_cli.c)
target_link_libraries(zcl-blue-payment-sim PRIVATE blue_payment_simulate
    blue_payment_render)
target_compile_options(zcl-blue-payment-sim PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_wallet_receive STATIC src/blue_wallet_receive.c)
target_include_directories(blue_wallet_receive PUBLIC include)
target_compile_options(blue_wallet_receive PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_wallet_protocol STATIC src/blue_wallet_protocol.c)
target_include_directories(blue_wallet_protocol PUBLIC include)
target_compile_options(blue_wallet_protocol PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_wallet_render STATIC src/blue_wallet_render.c)
target_include_directories(blue_wallet_render PUBLIC include)
target_link_libraries(blue_wallet_render PUBLIC blue_wallet_receive blue_bagl_canvas)
target_compile_options(blue_wallet_render PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(zcl-blue-wallet-preview src/blue_wallet_preview_cli.c)
target_link_libraries(zcl-blue-wallet-preview PRIVATE blue_wallet_render zcl_address)
target_compile_options(zcl-blue-wallet-preview PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_review_render STATIC src/blue_review_render.c)
target_include_directories(blue_review_render PUBLIC include)
target_link_libraries(blue_review_render PUBLIC blue_review_app blue_bagl_canvas)
target_compile_options(blue_review_render PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(zcl-blue-screen-sim src/blue_screen_sim_cli.c)
target_link_libraries(zcl-blue-screen-sim PRIVATE blue_review_render blue_review_simulate)
target_compile_options(zcl-blue-screen-sim PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(zcl-ledger src/main.c)
target_link_libraries(zcl-ledger PRIVATE ledger_hid zcl_address blue_wallet_protocol)
target_compile_options(zcl-ledger PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_secure STATIC src/blue_secure.c)
target_include_directories(blue_secure PUBLIC include)
target_link_libraries(blue_secure PUBLIC OpenSSL::Crypto)
target_compile_options(blue_secure PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_app_catalog STATIC src/blue_app_catalog.c)
target_include_directories(blue_app_catalog PUBLIC include)
target_compile_options(blue_app_catalog PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_install_params STATIC src/blue_install_params.c)
target_include_directories(blue_install_params PUBLIC include)
target_compile_options(blue_install_params PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(blue_ca STATIC src/blue_ca.c)
target_include_directories(blue_ca PUBLIC include)
target_link_libraries(blue_ca PUBLIC blue_secure)
target_compile_options(blue_ca PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(zcl-blue-ca src/blue_ca_cli.c)
target_link_libraries(zcl-blue-ca PRIVATE blue_ca)
target_compile_options(zcl-blue-ca PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(zcl-blue-install src/blue_install.c)
target_link_libraries(zcl-blue-install PRIVATE ledger_hid blue_secure blue_ca blue_install_params blue_app_catalog)
target_compile_options(zcl-blue-install PRIVATE -Wall -Wextra -Werror -pedantic)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    foreach(target ledger_hid zcl_base58 zcl_address zcl_sign_test zcl-blue-sign-test zcl_tx_review zcl_tx_script_facts zcl_zip243 zcl_zip243_host_hash zcl_tx_prevout zcl_tx_stream zcl_tx_stream_zip243 zcl_tx_replay_zip243 blue_payment_review blue_payment_screen blue_payment_apdu blue_payment_render blue_payment_simulate blue_payment_live zcl-blue-payment-sim zcl-blue-wallet-review blue_review_protocol blue_review_screen blue_review_app blue_review_simulate blue_bagl_canvas blue_wallet_receive blue_wallet_protocol blue_wallet_render zcl-blue-wallet-preview blue_review_render zcl-blue-screen-sim zcl-tx-review zcl-ledger blue_secure blue_app_catalog blue_install_params blue_ca zcl-blue-ca zcl-blue-install)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endforeach()
endif()

enable_testing()
add_executable(test-blue-bagl-canvas tests/test_blue_bagl_canvas.c)
target_link_libraries(test-blue-bagl-canvas PRIVATE blue_bagl_canvas)
target_compile_options(test-blue-bagl-canvas PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-bagl-canvas PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-bagl-canvas PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-bagl-canvas COMMAND test-blue-bagl-canvas)

add_executable(test-blue-wallet-receive tests/test_blue_wallet_receive.c)
target_link_libraries(test-blue-wallet-receive PRIVATE blue_wallet_receive)
target_compile_options(test-blue-wallet-receive PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-wallet-receive PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-wallet-receive PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-wallet-receive COMMAND test-blue-wallet-receive)

add_executable(test-blue-wallet-protocol tests/test_blue_wallet_protocol.c)
target_link_libraries(test-blue-wallet-protocol PRIVATE blue_wallet_protocol)
target_compile_options(test-blue-wallet-protocol PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-wallet-protocol PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-wallet-protocol PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-wallet-protocol COMMAND test-blue-wallet-protocol)

add_executable(test-ledger-hid tests/test_ledger_hid.c)
target_link_libraries(test-ledger-hid PRIVATE ledger_hid)
target_compile_options(test-ledger-hid PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-ledger-hid PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-ledger-hid PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME ledger-hid COMMAND test-ledger-hid)

add_executable(test-ledger-hid-io tests/test_ledger_hid_io.c)
target_link_libraries(test-ledger-hid-io PRIVATE ledger_hid)
target_compile_options(test-ledger-hid-io PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-ledger-hid-io PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-ledger-hid-io PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME ledger-hid-io COMMAND test-ledger-hid-io)

add_executable(test-blue-secure tests/test_blue_secure.c)
target_link_libraries(test-blue-secure PRIVATE blue_secure)
target_compile_options(test-blue-secure PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-secure PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-secure PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-secure COMMAND test-blue-secure)

add_executable(test-blue-app-catalog tests/test_blue_app_catalog.c)
target_link_libraries(test-blue-app-catalog PRIVATE blue_app_catalog)
target_compile_options(test-blue-app-catalog PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-app-catalog PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-app-catalog PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-app-catalog COMMAND test-blue-app-catalog)

add_executable(test-blue-ca tests/test_blue_ca.c)
target_link_libraries(test-blue-ca PRIVATE blue_ca)
target_compile_options(test-blue-ca PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-ca PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-ca PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-ca COMMAND test-blue-ca)

add_executable(test-blue-install-params tests/test_blue_install_params.c)
target_link_libraries(test-blue-install-params PRIVATE blue_install_params)
target_compile_options(test-blue-install-params PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-install-params PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-install-params PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-install-params COMMAND test-blue-install-params)

add_executable(test-zcl-address tests/test_zcl_address.c)
target_link_libraries(test-zcl-address PRIVATE zcl_address)
target_compile_options(test-zcl-address PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-zcl-address PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-zcl-address PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME zcl-address COMMAND test-zcl-address)

add_executable(test-zcl-sign-test tests/test_zcl_sign_test.c)
target_link_libraries(test-zcl-sign-test PRIVATE zcl_sign_test)
target_compile_options(test-zcl-sign-test PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-zcl-sign-test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-zcl-sign-test PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME zcl-sign-test COMMAND test-zcl-sign-test)

add_executable(test-zcl-tx-review tests/test_zcl_tx_review.c)
target_link_libraries(test-zcl-tx-review PRIVATE zcl_tx_review)
target_compile_options(test-zcl-tx-review PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-zcl-tx-review PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-zcl-tx-review PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME zcl-tx-review COMMAND test-zcl-tx-review)

add_executable(test-zcl-tx-script-facts tests/test_zcl_tx_script_facts.c)
target_link_libraries(test-zcl-tx-script-facts PRIVATE zcl_tx_script_facts)
target_compile_options(test-zcl-tx-script-facts PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-zcl-tx-script-facts PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-zcl-tx-script-facts PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME zcl-tx-script-facts COMMAND test-zcl-tx-script-facts)

add_executable(test-zcl-tx-prevout tests/test_zcl_tx_prevout.c)
target_link_libraries(test-zcl-tx-prevout PRIVATE zcl_tx_prevout
    zcl_zip243_host_hash OpenSSL::Crypto)
target_compile_options(test-zcl-tx-prevout PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-zcl-tx-prevout PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-zcl-tx-prevout PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME zcl-tx-prevout COMMAND test-zcl-tx-prevout
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/zip243-transparent-vector3.hex)

add_executable(test-zcl-tx-stream tests/test_zcl_tx_stream.c)
target_link_libraries(test-zcl-tx-stream PRIVATE zcl_tx_stream zcl_tx_review)
target_compile_options(test-zcl-tx-stream PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-zcl-tx-stream PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-zcl-tx-stream PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME zcl-tx-stream COMMAND test-zcl-tx-stream)

add_executable(test-blue-payment-review tests/test_blue_payment_review.c)
target_link_libraries(test-blue-payment-review PRIVATE blue_payment_review
    blue_payment_screen blue_payment_apdu blue_payment_simulate
    blue_payment_live zcl_zip243
    zcl_zip243_host_hash OpenSSL::Crypto)
target_compile_options(test-blue-payment-review PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-payment-review PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-payment-review PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-payment-review COMMAND test-blue-payment-review)

add_executable(test-blue-payment-screen tests/test_blue_payment_screen.c)
target_link_libraries(test-blue-payment-screen PRIVATE blue_payment_render
    zcl_address OpenSSL::Crypto)
target_compile_options(test-blue-payment-screen PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-payment-screen PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-payment-screen PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-payment-screen COMMAND test-blue-payment-screen)

option(ZCL_LEDGER_FUZZ "Build the local Clang transaction parser fuzzer" OFF)
if(ZCL_LEDGER_FUZZ)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR
       NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR "ZCL_LEDGER_FUZZ requires Clang Debug")
    endif()
    foreach(target zcl_tx_review zcl_tx_script_facts zcl_zip243 zcl_tx_stream
                   zcl_tx_stream_zip243 zcl_tx_replay_zip243
                   zcl_zip243_host_hash)
        target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
    endforeach()
    add_executable(fuzz-zcl-tx tests/fuzz_zcl_tx.c)
    target_link_libraries(fuzz-zcl-tx PRIVATE zcl_tx_script_facts
        zcl_zip243 zcl_zip243_host_hash)
    target_compile_options(fuzz-zcl-tx PRIVATE -Wall -Wextra -Werror -pedantic
        -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz-zcl-tx PRIVATE -fsanitize=fuzzer,address,undefined)
    add_executable(fuzz-zcl-tx-stream tests/fuzz_zcl_tx_stream.c)
    target_link_libraries(fuzz-zcl-tx-stream PRIVATE zcl_tx_stream
        zcl_tx_stream_zip243 zcl_tx_replay_zip243 zcl_tx_script_facts
        zcl_tx_review zcl_zip243 zcl_zip243_host_hash OpenSSL::Crypto)
    target_compile_options(fuzz-zcl-tx-stream PRIVATE -Wall -Wextra -Werror
        -pedantic -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz-zcl-tx-stream PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

add_executable(test-zcl-zip243 tests/test_zcl_zip243.c)
target_link_libraries(test-zcl-zip243 PRIVATE zcl_zip243 zcl_tx_stream_zip243 zcl_tx_replay_zip243 zcl_zip243_host_hash OpenSSL::Crypto)
target_compile_options(test-zcl-zip243 PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-zcl-zip243 PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-zcl-zip243 PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME zcl-zip243 COMMAND test-zcl-zip243
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/zip243-vector1.hex
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/zip243-transparent-vector3.hex)

add_executable(test-blue-review-protocol tests/test_blue_review_protocol.c)
target_link_libraries(test-blue-review-protocol PRIVATE blue_review_protocol blue_review_screen zcl_zip243_host_hash OpenSSL::Crypto)
target_compile_options(test-blue-review-protocol PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-review-protocol PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-review-protocol PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-review-protocol COMMAND test-blue-review-protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/zip243-transparent-vector3.hex)

add_executable(test-blue-review-screen tests/test_blue_review_screen.c)
target_link_libraries(test-blue-review-screen PRIVATE blue_review_screen blue_review_protocol zcl_zip243_host_hash zcl_address OpenSSL::Crypto)
target_compile_options(test-blue-review-screen PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-review-screen PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-review-screen PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-review-screen COMMAND test-blue-review-screen)

add_executable(test-blue-review-simulator tests/test_blue_review_simulator.c)
target_link_libraries(test-blue-review-simulator PRIVATE blue_review_simulate blue_review_render OpenSSL::Crypto)
target_compile_options(test-blue-review-simulator PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-blue-review-simulator PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-blue-review-simulator PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME blue-review-simulator COMMAND test-blue-review-simulator
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/zip243-transparent-vector3.hex)

add_executable(test-zcl-tx-review-cli tests/test_zcl_tx_review_cli.c)
target_compile_options(test-zcl-tx-review-cli PRIVATE -Wall -Wextra -Werror -pedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-zcl-tx-review-cli PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(test-zcl-tx-review-cli PRIVATE -fsanitize=address,undefined)
endif()
add_test(NAME zcl-tx-review-cli COMMAND test-zcl-tx-review-cli
    $<TARGET_FILE:zcl-tx-review>
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/zip243-transparent-vector3.hex
    $<TARGET_FILE:zcl-blue-screen-sim>)
