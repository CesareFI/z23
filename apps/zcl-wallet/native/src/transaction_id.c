/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_transaction.h"
#include "mbedtls/sha256.h"
#include <string.h>

zcl_status zcl_transaction_id(const zcl_transparent_tx *transaction,
                              uint8_t *txid, size_t capacity)
{
    if (txid == NULL) return ZCL_INVALID_ARGUMENT;
    if (capacity < 32) return ZCL_BUFFER_TOO_SMALL;
    uint8_t wire[ZCL_TX_WIRE_MAX] = {0};
    size_t length = 0;
    const zcl_status status = zcl_transaction_serialize(transaction, wire, sizeof(wire), &length);
    if (status != ZCL_OK) return status;
    uint8_t first[32] = {0}, second[32] = {0};
    if (mbedtls_sha256(wire, length, first, 0) != 0) return ZCL_CRYPTO_FAILURE;
    if (mbedtls_sha256(first, sizeof(first), second, 0) != 0) return ZCL_CRYPTO_FAILURE;
    for (size_t i = 0; i < sizeof(second); ++i) txid[i] = second[sizeof(second) - i - 1];
    return ZCL_OK;
}
