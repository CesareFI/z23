/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "rpc_json.h"
#include <string.h>

static bool printable_ascii(const uint8_t *text, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (text[i] < 0x20 || text[i] > 0x7e) return false;
    }
    return true;
}

static bool string_token(const zcl_rpc_token *token)
{
    return token->kind == ZJRP_KEY || token->kind == ZJRP_STR;
}

zcl_status zcl_rpc_ascii(const zcl_rpc_json *doc, const zcl_rpc_token *token,
                          uint8_t *output, size_t capacity, size_t *length)
{
    if (doc == NULL || token == NULL || output == NULL || length == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (!string_token(token))
        return ZCL_INVALID_ENCODING;
    const zjsonp_event event = {(zjsonp_event_kind)token->kind, token->offset, token->length};
    const size_t decoded = zjsonp_str_decode((const char *)doc->text, &event, (char *)output, capacity);
    if (decoded == SIZE_MAX) return ZCL_INVALID_ENCODING;
    if (decoded > capacity) return ZCL_BUFFER_TOO_SMALL;
    if (!printable_ascii(output, decoded)) return ZCL_INVALID_ENCODING;
    *length = decoded;
    return ZCL_OK;
}

static bool plain_string_is(const uint8_t *raw, size_t raw_len,
                            const uint8_t *text, size_t length)
{
    return raw_len == length && printable_ascii(raw, raw_len) &&
        memcmp(raw, text, length) == 0;
}

bool zcl_rpc_string_is(const zcl_rpc_json *doc, const zcl_rpc_token *token,
                        const uint8_t *text, size_t length)
{
    uint8_t decoded[ZCL_RPC_KEY_MAX];
    size_t count = 0;
    if (length > sizeof(decoded) || doc == NULL || token == NULL) return false;
    if (!string_token(token)) return false;
    /* A parsed token without escapes is already its decoded ASCII span.
     * Escaped text retains the original decoder and comparison below. */
    if (doc->text != NULL &&
        memchr(doc->text + token->offset, '\\', token->length) == NULL)
        return plain_string_is(doc->text + token->offset, token->length, text, length);
    return zcl_rpc_ascii(doc, token, decoded, sizeof(decoded), &count) == ZCL_OK &&
           count == length && memcmp(decoded, text, length) == 0;
}

const zcl_rpc_token *zcl_rpc_member(const zcl_rpc_json *doc, const zcl_rpc_token *object,
                                    const uint8_t *key, size_t key_len)
{
    if (object == NULL || object->kind != ZJRP_OBJ_OPEN) return NULL;
    /* Private API: object belongs to this successfully parsed document. */
    uint16_t index = (uint16_t)(object - doc->tokens + 1);
    while (index < object->next) {
        if (zcl_rpc_string_is(doc, &doc->tokens[index], key, key_len))
            return &doc->tokens[index + 1];
        index = doc->tokens[index + 1].next;
    }
    return NULL;
}

const zcl_rpc_token *zcl_rpc_child(const zcl_rpc_json *doc, const zcl_rpc_token *container,
                                   size_t position)
{
    if (container == NULL || container->kind != ZJRP_ARR_OPEN) return NULL;
    uint16_t index = (uint16_t)(container - doc->tokens + 1);
    while (index < container->next) {
        if (position == 0) return &doc->tokens[index];
        --position;
        index = doc->tokens[index].next;
    }
    return NULL;
}

static zcl_status magnitude(const uint8_t *digits, size_t length, uint64_t limit, uint64_t *output)
{
    if (length == 0 || length > 19) return ZCL_INVALID_ENCODING;
    if (length > 1 && digits[0] == '0') return ZCL_INVALID_ENCODING;
    uint64_t result = 0;
    for (size_t i = 0; i < length; ++i) {
        if (digits[i] < '0' || digits[i] > '9') return ZCL_INVALID_ENCODING;
        const uint64_t digit = (uint64_t)(digits[i] - '0');
        if (result > (limit - digit) / 10) return ZCL_OUT_OF_RANGE;
        result = result * 10 + digit;
    }
    *output = result;
    return ZCL_OK;
}

zcl_status zcl_rpc_integer(const zcl_rpc_json *doc, const zcl_rpc_token *token, int64_t *value)
{
    if (token == NULL || token->kind != ZJRP_NUM || token->length == 0)
        return ZCL_INVALID_ENCODING;
    const uint8_t *text = doc->text + token->offset;
    const bool negative = text[0] == '-';
    const size_t start = (size_t)negative;
    const uint64_t limit = (uint64_t)INT64_MAX + (uint64_t)negative;
    uint64_t absolute = 0;
    const zcl_status status = magnitude(text + start, (size_t)token->length - start, limit, &absolute);
    if (status != ZCL_OK) return status;
    if (negative && absolute == 0) return ZCL_INVALID_ENCODING;
    if (absolute == (uint64_t)INT64_MAX + UINT64_C(1)) *value = INT64_MIN;
    else *value = negative ? -(int64_t)absolute : (int64_t)absolute;
    return ZCL_OK;
}

zcl_status zcl_rpc_result(const zcl_rpc_json *doc, uint32_t id, const zcl_rpc_token **result)
{
    const zcl_rpc_token *root = &doc->tokens[0];
    int64_t received = 0;
    if (id == 0 || zcl_rpc_integer(doc, zcl_rpc_member(doc, root, (const uint8_t *)"id", 2), &received) != ZCL_OK)
        return ZCL_INVALID_ENCODING;
    if (received != (int64_t)id) return ZCL_INVALID_ENCODING;
    const zcl_rpc_token *version = zcl_rpc_member(doc, root, (const uint8_t *)"jsonrpc", 7);
    if (version != NULL && !zcl_rpc_string_is(doc, version, (const uint8_t *)"2.0", 3))
        return ZCL_UNSUPPORTED;
    if (zcl_rpc_member(doc, root, (const uint8_t *)"method", 6) != NULL)
        return ZCL_INVALID_ENCODING;
    const zcl_rpc_token *error = zcl_rpc_member(doc, root, (const uint8_t *)"error", 5);
    if (error != NULL && error->kind != ZJRP_NULL) return ZCL_IO_FAILURE;
    const zcl_rpc_token *value = zcl_rpc_member(doc, root, (const uint8_t *)"result", 6);
    if (value == NULL) return ZCL_INVALID_ENCODING;
    *result = value;
    return ZCL_OK;
}
