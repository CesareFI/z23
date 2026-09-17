/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "electrum_internal.h"
#include "header_internal.h"
#include "zcl_keys.h"
#include <string.h>

static int nibble(uint8_t byte)
{
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
}

zcl_status zcl_rpc_hex(const zcl_rpc_json *doc, const zcl_rpc_token *token,
                        uint8_t *output, size_t capacity, size_t *length)
{
    uint8_t text[2974]; /* Two hex bytes per largest supported main/test header. */
    size_t count = 0;
    const zcl_status status = zcl_rpc_ascii(doc, token, text, sizeof(text), &count);
    if (status != ZCL_OK) return status;
    if (count % 2 != 0) return ZCL_INVALID_ENCODING;
    if (count / 2 > capacity) return ZCL_BUFFER_TOO_SMALL;
    for (size_t i = 0; i < count / 2; ++i) {
        const int high = nibble(text[2 * i]), low = nibble(text[2 * i + 1]);
        if (high < 0 || low < 0) return ZCL_INVALID_ENCODING;
        output[i] = (uint8_t)(high * 16 + low);
    }
    *length = count / 2;
    return ZCL_OK;
}

zcl_status zcl_rpc_header_hash(const zcl_rpc_json *doc, const zcl_rpc_token *token,
                                zcl_network network, uint32_t height, uint8_t hash[32])
{
    if (network != ZCL_MAINNET && network != ZCL_TESTNET) return ZCL_UNSUPPORTED;
    if (hash == NULL) return ZCL_INVALID_ARGUMENT;
    uint8_t header[1487] = {0};
    zcl_header_view view = {0};
    size_t length = 0;
    zcl_status status = zcl_rpc_hex(doc, token, header, sizeof(header), &length);
    if (status == ZCL_OK) status = zcl_header_inspect(header, length, network, height, &view);
    if (status == ZCL_OK) memcpy(hash, view.hash, sizeof(view.hash));
    zcl_secure_zero(&view, sizeof(view));
    zcl_secure_zero(header, sizeof(header));
    return status;
}

zcl_status zcl_electrum_tip_reply(const uint8_t *frame, size_t length, uint32_t id,
                                  zcl_network network, zcl_reported_tip *tip)
{
    if (tip == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_rpc_json doc;
    zcl_status status = zcl_rpc_json_parse(frame, length, &doc);
    if (status != ZCL_OK) return status;
    const zcl_rpc_token *result = NULL;
    status = zcl_rpc_result(&doc, id, &result);
    if (status != ZCL_OK) return status;
    int64_t height = 0;
    status = zcl_rpc_integer(&doc, zcl_rpc_member(&doc, result, (const uint8_t *)"height", 6), &height);
    if (status != ZCL_OK) return status;
    if (height < 0 || height > INT32_MAX) return ZCL_OUT_OF_RANGE;
    zcl_reported_tip parsed = {0};
    parsed.height = (uint32_t)height;
    status = zcl_rpc_header_hash(&doc, zcl_rpc_member(&doc, result, (const uint8_t *)"hex", 3),
                                  network, parsed.height, parsed.hash);
    if (status != ZCL_OK) return status;
    *tip = parsed;
    return ZCL_OK;
}
