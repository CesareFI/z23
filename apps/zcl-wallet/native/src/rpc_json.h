/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#ifndef ZCL_RPC_JSON_H
#define ZCL_RPC_JSON_H
#include "zcl_wallet.h"
#include "zjsonp/zjsonp.h"

#define ZCL_RPC_JSON_MAX ((size_t)16384)
#define ZCL_RPC_TOKENS_MAX 128
#define ZCL_RPC_KEY_MAX 256

/* Private, invocation-owned index over a stable caller frame. No allocation or
 * retained ownership. All offsets are validated before narrowing to uint16. */
typedef struct {
    uint16_t offset, length, next;
    uint8_t kind;
} zcl_rpc_token;
typedef struct {
    const uint8_t *text;
    size_t length;
    uint16_t count;
    zcl_rpc_token tokens[ZCL_RPC_TOKENS_MAX];
} zcl_rpc_json;

zcl_status zcl_rpc_json_parse(const uint8_t *text, size_t length, zcl_rpc_json *document);
const zcl_rpc_token *zcl_rpc_member(const zcl_rpc_json *document, const zcl_rpc_token *object,
                                    const uint8_t *key, size_t key_len);
const zcl_rpc_token *zcl_rpc_child(const zcl_rpc_json *document, const zcl_rpc_token *container,
                                   size_t position);
zcl_status zcl_rpc_ascii(const zcl_rpc_json *document, const zcl_rpc_token *token,
                          uint8_t *output, size_t capacity, size_t *length);
bool zcl_rpc_string_is(const zcl_rpc_json *document, const zcl_rpc_token *token,
                        const uint8_t *text, size_t length);
zcl_status zcl_rpc_integer(const zcl_rpc_json *document, const zcl_rpc_token *token, int64_t *value);
zcl_status zcl_rpc_result(const zcl_rpc_json *document, uint32_t id, const zcl_rpc_token **result);
#endif
