/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_WALLET_H
#define ZCL_WALLET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Explicit status only. No caller input or secret material enters messages. */
typedef enum {
    ZCL_OK = 0,
    ZCL_INVALID_ARGUMENT,
    ZCL_INVALID_ENCODING,
    ZCL_OUT_OF_RANGE,
    ZCL_BUFFER_TOO_SMALL,
    ZCL_UNSUPPORTED,
    ZCL_CRYPTO_FAILURE,
    ZCL_IO_FAILURE,
    ZCL_INVALID_CHILD,
    ZCL_RESOURCE_EXHAUSTED,
    ZCL_NOT_FOUND,
    ZCL_ALREADY_EXISTS,
    ZCL_BUSY,
    ZCL_IO_UNCERTAIN,
    ZCL_TIMED_OUT,
    ZCL_CANCELLED,
    ZCL_TLS_FAILURE
} zcl_status;

#define ZCL_ZATOSHIS_PER_COIN UINT64_C(100000000)
#define ZCL_MAX_MONEY (UINT64_C(21000000) * ZCL_ZATOSHIS_PER_COIN)
#define ZCL_AMOUNT_TEXT_MAX ((size_t)17)

/* No API below allocates, retains pointers, mutates input or uses global state.
 * Buffers are caller-owned for the call. Input/output must not overlap.
 * NULL is rejected even for zero-length spans. On error, outputs are unchanged.
 * Text spans have explicit byte lengths, never an implicit terminating NUL.
 * These contracts also apply to JNI adapters, which must check Java exceptions.
 */
zcl_status zcl_amount_parse(const uint8_t *text, size_t text_len, uint64_t *amount);
zcl_status zcl_amount_format(uint64_t amount, uint8_t *text, size_t text_capacity,
                             size_t *text_len);
zcl_status zcl_amount_add(uint64_t left, uint64_t right, uint64_t *result);
zcl_status zcl_amount_subtract(uint64_t left, uint64_t right, uint64_t *result);

#define ZCL_BASE58_PAYLOAD_MAX ((size_t)128)
#define ZCL_BASE58_TEXT_MAX ((size_t)184)
/* Public data codecs. Secret imports will use separate zeroizing storage. */
zcl_status zcl_base58check_encode(const uint8_t *payload, size_t payload_len,
                                 uint8_t *text, size_t text_capacity, size_t *text_len);
zcl_status zcl_base58check_decode(const uint8_t *text, size_t text_len,
                                 uint8_t *payload, size_t payload_capacity, size_t *payload_len);

typedef enum { ZCL_MAINNET = 0, ZCL_TESTNET = 1 } zcl_network;
/* 32-byte genesis hash in displayed big-endian order, not wire uint256 order. */
zcl_status zcl_network_genesis(zcl_network network, uint8_t *hash, size_t capacity);
typedef enum { ZCL_P2PKH = 1, ZCL_P2SH = 2 } zcl_address_kind;
typedef struct {
    zcl_network network;
    zcl_address_kind kind;
    uint8_t hash[20];
} zcl_address;

zcl_status zcl_address_parse(const uint8_t *text, size_t text_len,
                             zcl_network network, zcl_address *address);
zcl_status zcl_address_from_hash(const uint8_t *hash, size_t hash_len,
                                 zcl_network network, uint8_t *text,
                                 size_t text_capacity, size_t *text_len);
zcl_status zcl_address_script(const zcl_address *address, uint8_t *script,
                              size_t script_capacity, size_t *script_len);

#define ZCL_PAYMENT_TEXT_MAX ((size_t)1024)
#define ZCL_PAYMENT_FIELD_MAX ((size_t)200)
typedef struct {
    zcl_address address;
    uint8_t address_text[35];
    bool has_amount;
    bool has_label;
    bool has_message;
    uint64_t amount;
    uint8_t label[200];
    size_t label_len;
    uint8_t message[200];
    size_t message_len;
} zcl_payment_request;

/* Public request data only; never payment authorization. Labels/messages are
 * UTF-8, at most 200 bytes, without Unicode control/format characters. */
zcl_status zcl_payment_parse(const uint8_t *text, size_t text_len,
                             zcl_network network, zcl_payment_request *request);

#ifdef __cplusplus
}
#endif
#endif
