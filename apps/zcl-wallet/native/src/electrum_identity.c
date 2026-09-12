/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "electrum_internal.h"
#include <string.h>

zcl_status zcl_electrum_version_reply(const uint8_t *frame, size_t length, uint32_t id)
{
    zcl_rpc_json doc;
    zcl_status status = zcl_rpc_json_parse(frame, length, &doc);
    if (status != ZCL_OK) return status;
    const zcl_rpc_token *result = NULL;
    status = zcl_rpc_result(&doc, id, &result);
    if (status != ZCL_OK) return status;
    const zcl_rpc_token *server = zcl_rpc_child(&doc, result, 0);
    if (server == NULL || server->kind != ZJRP_STR || server->length == 0 || server->length > 256)
        return ZCL_INVALID_ENCODING;
    if (zcl_rpc_child(&doc, result, 2) != NULL) return ZCL_INVALID_ENCODING;
    return zcl_rpc_string_is(&doc, zcl_rpc_child(&doc, result, 1), (const uint8_t *)"1.2", 3)
        ? ZCL_OK : ZCL_UNSUPPORTED;
}

static zcl_status feature_identity(const zcl_rpc_json *doc, const zcl_rpc_token *result,
                                     zcl_network network)
{
    uint8_t expected[32], received[32];
    zcl_status status = zcl_network_genesis(network, expected, sizeof(expected));
    if (status != ZCL_OK) return status;
    size_t length = 0;
    status = zcl_rpc_hex(doc, zcl_rpc_member(doc, result, (const uint8_t *)"genesis_hash", 12),
                         received, sizeof(received), &length);
    if (status != ZCL_OK) return status;
    if (length != sizeof(expected) || memcmp(expected, received, sizeof(expected)) != 0)
        return ZCL_UNSUPPORTED;
    return zcl_rpc_string_is(doc, zcl_rpc_member(doc, result, (const uint8_t *)"hash_function", 13),
                              (const uint8_t *)"sha256", 6) ? ZCL_OK : ZCL_UNSUPPORTED;
}

zcl_status zcl_electrum_features_reply(const uint8_t *frame, size_t length, uint32_t id,
                                       zcl_network network)
{
    zcl_rpc_json doc;
    zcl_status status = zcl_rpc_json_parse(frame, length, &doc);
    if (status != ZCL_OK) return status;
    const zcl_rpc_token *result = NULL;
    status = zcl_rpc_result(&doc, id, &result);
    if (status != ZCL_OK) return status;
    if (result->kind != ZJRP_OBJ_OPEN) return ZCL_INVALID_ENCODING;
    return feature_identity(&doc, result, network);
}

zcl_status zcl_electrum_genesis_reply(const uint8_t *frame, size_t length, uint32_t id,
                                      zcl_network network)
{
    uint8_t expected[32], received[32];
    zcl_status status = zcl_network_genesis(network, expected, sizeof(expected));
    if (status != ZCL_OK) return status;
    zcl_rpc_json doc;
    status = zcl_rpc_json_parse(frame, length, &doc);
    if (status != ZCL_OK) return status;
    const zcl_rpc_token *result = NULL;
    status = zcl_rpc_result(&doc, id, &result);
    if (status != ZCL_OK) return status;
    int64_t count = 0;
    if (zcl_rpc_integer(&doc, zcl_rpc_member(&doc, result, (const uint8_t *)"count", 5), &count) != ZCL_OK || count != 1)
        return ZCL_INVALID_ENCODING;
    status = zcl_rpc_header_hash(&doc, zcl_rpc_member(&doc, result, (const uint8_t *)"hex", 3), network, 0, received);
    if (status != ZCL_OK) return status;
    return memcmp(expected, received, sizeof(expected)) == 0 ? ZCL_OK : ZCL_UNSUPPORTED;
}
