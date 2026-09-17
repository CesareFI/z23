/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "header_internal.h"
#include "zcl_keys.h"
#include "mbedtls/sha256.h"

static void display_bytes(const uint8_t raw[32], uint8_t displayed[32])
{
    for (size_t i = 0; i < 32; ++i) displayed[i] = raw[31 - i];
}

static uint32_t scalar(const uint8_t wire[4])
{
    return (uint32_t)wire[0] | ((uint32_t)wire[1] << 8) |
           ((uint32_t)wire[2] << 16) | ((uint32_t)wire[3] << 24);
}

static bool solution_header(const uint8_t *wire, size_t length, size_t solution)
{
    if (length != 143 + solution) return false;
    return wire[140] == 253 && wire[141] == (uint8_t)(solution & 255) &&
           wire[142] == (uint8_t)(solution >> 8);
}

zcl_status zcl_header_inspect(const uint8_t *wire, size_t length,
    zcl_network network, uint32_t height, zcl_header_view *view)
{
    if (wire == NULL || view == NULL) return ZCL_INVALID_ARGUMENT;
    if (network != ZCL_MAINNET && network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    const uint32_t bubbles = network == ZCL_MAINNET ? UINT32_C(585318) : UINT32_C(6350);
    const size_t solution = height < bubbles ? 1344 : 400;
    if (!solution_header(wire, length, solution)) return ZCL_INVALID_ENCODING;
    struct { zcl_header_view view; uint8_t first[32], second[32]; } work = {0};
    zcl_status status = ZCL_CRYPTO_FAILURE;
    if (mbedtls_sha256(wire, length, work.first, 0) != 0 ||
        mbedtls_sha256(work.first, sizeof(work.first), work.second, 0) != 0) goto done;
    display_bytes(work.second, work.view.hash);
    display_bytes(wire + 4, work.view.previous);
    display_bytes(wire + 36, work.view.merkle);
    display_bytes(wire + 68, work.view.sapling);
    display_bytes(wire + 108, work.view.nonce);
    work.view.version_bits = scalar(wire);
    work.view.timestamp = scalar(wire + 100);
    work.view.bits = scalar(wire + 104);
    work.view.solution_length = solution;
    *view = work.view;
    status = ZCL_OK;
done:
    zcl_secure_zero(&work, sizeof(work));
    return status;
}
