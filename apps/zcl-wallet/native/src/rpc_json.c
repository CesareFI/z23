/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "rpc_json.h"
#include <string.h>

static bool is_close(zjsonp_event_kind kind)
{
    return kind == ZJRP_OBJ_CLOSE || kind == ZJRP_ARR_CLOSE;
}

static bool is_open(zjsonp_event_kind kind)
{
    return kind == ZJRP_OBJ_OPEN || kind == ZJRP_ARR_OPEN;
}

static zcl_status add_token(zcl_rpc_json *doc, const zjsonp_event *event)
{
    if (doc->count >= ZCL_RPC_TOKENS_MAX)
        return ZCL_RESOURCE_EXHAUSTED;
    if (event->off > doc->length || event->len > doc->length - event->off)
        return ZCL_INVALID_ENCODING;
    if (event->kind == ZJRP_KEY || event->kind == ZJRP_STR) {
        /* Validate escape/surrogate syntax even in ignored extension fields.
         * No Unicode text is returned by this measurement-only call. */
        if (zjsonp_str_decode((const char *)doc->text, event, NULL, 0) == SIZE_MAX)
            return ZCL_INVALID_ENCODING;
    }
    zcl_rpc_token *token = &doc->tokens[doc->count];
    token->offset = (uint16_t)event->off;
    token->length = (uint16_t)event->len;
    token->kind = (uint8_t)event->kind;
    ++doc->count;
    token->next = doc->count;
    return ZCL_OK;
}

static zcl_status parse_event(zcl_rpc_json *doc, const zjsonp_event *event,
                               uint16_t stack[8], size_t *depth)
{
    if (is_close(event->kind)) {
        if (*depth == 0) return ZCL_INVALID_ENCODING;
        --*depth;
        doc->tokens[stack[*depth]].next = doc->count;
        return ZCL_OK;
    }
    const zcl_status added = add_token(doc, event);
    if (added != ZCL_OK) return added;
    if (is_open(event->kind)) {
        if (*depth == 8) return ZCL_RESOURCE_EXHAUSTED;
        stack[*depth] = (uint16_t)(doc->count - 1);
        ++*depth;
    }
    return ZCL_OK;
}

static zcl_status parse_tokens(zcl_rpc_json *doc)
{
    zjsonp parser;
    zjsonp_init(&parser, (const char *)doc->text, doc->length);
    uint16_t stack[8] = {0};
    size_t depth = 0;
    for (size_t events = 0; events <= ZCL_RPC_TOKENS_MAX * 2; ++events) {
        zjsonp_event event = {0};
        const zjsonp_status parsed = zjsonp_next(&parser, &event);
        if (parsed == ZJRP_DONE)
            return depth == 0 && doc->count != 0 ? ZCL_OK : ZCL_INVALID_ENCODING;
        if (parsed != ZJRP_OK)
            return ZCL_INVALID_ENCODING;
        const zcl_status status = parse_event(doc, &event, stack, &depth);
        if (status != ZCL_OK) return status;
    }
    return ZCL_RESOURCE_EXHAUSTED;
}

static zcl_status duplicate_key(const zcl_rpc_json *doc, uint16_t start, uint16_t stop,
                                 const uint8_t *key, size_t length)
{
    uint16_t index = start;
    while (index < stop) {
        if (zcl_rpc_string_is(doc, &doc->tokens[index], key, length))
            return ZCL_INVALID_ENCODING;
        if ((size_t)index + 1 >= doc->count) return ZCL_INVALID_ENCODING;
        const uint16_t next = doc->tokens[index + 1].next;
        if (next <= index || next > stop) return ZCL_INVALID_ENCODING;
        index = next;
    }
    return ZCL_OK;
}

static zcl_status validate_object(const zcl_rpc_json *doc, uint16_t index)
{
    const uint16_t start = (uint16_t)(index + 1);
    const uint16_t end = doc->tokens[index].next;
    index = start;
    while (index < end) {
        uint8_t key[ZCL_RPC_KEY_MAX];
        size_t length = 0;
        if (doc->tokens[index].kind != ZJRP_KEY || (size_t)index + 1 >= end)
            return ZCL_INVALID_ENCODING;
        if (zcl_rpc_ascii(doc, &doc->tokens[index], key, sizeof(key), &length) != ZCL_OK)
            return ZCL_INVALID_ENCODING;
        if (duplicate_key(doc, start, index, key, length) != ZCL_OK)
            return ZCL_INVALID_ENCODING;
        const uint16_t next = doc->tokens[index + 1].next;
        if (next <= index || next > end) return ZCL_INVALID_ENCODING;
        index = next;
    }
    return ZCL_OK;
}

zcl_status zcl_rpc_json_parse(const uint8_t *text, size_t length, zcl_rpc_json *document)
{
    if (text == NULL || document == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (length == 0 || length > ZCL_RPC_JSON_MAX)
        return ZCL_OUT_OF_RANGE;
    memset(document, 0, sizeof(*document));
    document->text = text;
    document->length = length;
    const zcl_status parsed = parse_tokens(document);
    if (parsed != ZCL_OK) return parsed;
    if (document->tokens[0].kind != ZJRP_OBJ_OPEN)
        return ZCL_INVALID_ENCODING;
    for (uint16_t index = 0; index < document->count; ++index) {
        if (document->tokens[index].kind == ZJRP_OBJ_OPEN) {
            const zcl_status valid = validate_object(document, index);
            if (valid != ZCL_OK) return valid;
        }
    }
    return ZCL_OK;
}
