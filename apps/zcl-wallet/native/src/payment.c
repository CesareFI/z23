/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_wallet.h"
#include "uri_text.h"

#include <string.h>

static bool zclassic_scheme(const uint8_t *text, size_t length)
{
    static const uint8_t expected[] = "zclassic:";
    if (length < sizeof(expected) - 1)
        return false;
    for (size_t index = 0; index < sizeof(expected) - 1; ++index) {
        uint8_t byte = text[index];
        if (byte >= (uint8_t)'A' && byte <= (uint8_t)'Z')
            byte = (uint8_t)(byte + ((uint8_t)'a' - (uint8_t)'A'));
        if (byte != expected[index])
            return false;
    }
    return true;
}

static size_t delimiter(const uint8_t *text, size_t length, uint8_t separator)
{
    size_t position = 0;
    while (position < length && text[position] != separator)
        ++position;
    return position;
}

static unsigned field_kind(const uint8_t *key, size_t size)
{
    if (size == 6 && memcmp(key, "amount", 6) == 0)
        return 1;
    if (size == 5 && memcmp(key, "label", 5) == 0)
        return 2;
    if (size == 7 && memcmp(key, "message", 7) == 0)
        return 4;
    return 0;
}

static zcl_status store_field(unsigned kind, const uint8_t *value, size_t length,
                               zcl_payment_request *request)
{
    if (kind == 1) {
        zcl_status status = zcl_amount_parse(value, length, &request->amount);
        if (status != ZCL_OK)
            return status;
        if (request->amount == 0)
            return ZCL_OUT_OF_RANGE;
        request->has_amount = true;
    } else if (kind == 2) {
        memcpy(request->label, value, length);
        request->label_len = length;
        request->has_label = true;
    } else {
        memcpy(request->message, value, length);
        request->message_len = length;
        request->has_message = true;
    }
    return ZCL_OK;
}

static zcl_status parse_field(const uint8_t *field, size_t size, unsigned *seen,
                               zcl_payment_request *request)
{
    size_t equals = delimiter(field, size, (uint8_t)'=');
    if (equals == size)
        return ZCL_INVALID_ENCODING;
    uint8_t key[7] = {0}, value[200] = {0};
    size_t key_len = 0, value_len = 0;
    zcl_status status = zcl_uri_decode_field(field, equals, key, sizeof(key), &key_len);
    if (status != ZCL_OK)
        return status;
    unsigned kind = field_kind(key, key_len);
    if (kind == 0 || (*seen & kind) != 0)
        return ZCL_UNSUPPORTED;
    size_t start = equals + 1;
    status = zcl_uri_decode_field(field + start, size - start, value, sizeof(value), &value_len);
    if (status != ZCL_OK)
        return status;
    status = store_field(kind, value, value_len, request);
    if (status != ZCL_OK)
        return status;
    *seen |= kind;
    return ZCL_OK;
}

static zcl_status parse_query(const uint8_t *query, size_t size, zcl_payment_request *request)
{
    size_t position = 0;
    unsigned seen = 0;
    for (unsigned count = 0; count < 3; ++count) {
        if (position == size)
            return ZCL_INVALID_ENCODING;
        size_t field_len = delimiter(query + position, size - position, (uint8_t)'&');
        zcl_status status = parse_field(query + position, field_len, &seen, request);
        if (status != ZCL_OK)
            return status;
        position += field_len;
        if (position == size)
            return ZCL_OK;
        ++position;
    }
    return ZCL_OUT_OF_RANGE;
}

static zcl_status input_bounds(const uint8_t *text, size_t length,
                                const zcl_payment_request *request)
{
    if (text == NULL || request == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (length == 0 || length > ZCL_PAYMENT_TEXT_MAX)
        return ZCL_OUT_OF_RANGE;
    return ZCL_OK;
}

static zcl_status parse_metadata(const uint8_t *text, size_t length, size_t start,
                                  zcl_payment_request *request)
{
    if (length == start + 35)
        return ZCL_OK;
    if (start == 0 || text[start + 35] != (uint8_t)'?')
        return ZCL_INVALID_ENCODING;
    zcl_status status = zcl_uri_validate_text(text, length);
    if (status != ZCL_OK)
        return status;
    size_t query_start = start + 36;
    return parse_query(text + query_start, length - query_start, request);
}

zcl_status zcl_payment_parse(const uint8_t *text, size_t text_len,
                             zcl_network network, zcl_payment_request *request)
{
    zcl_status status = input_bounds(text, text_len, request);
    if (status != ZCL_OK)
        return status;
    size_t start = zclassic_scheme(text, text_len) ? 9 : 0;
    if (text_len - start < 35)
        return ZCL_INVALID_ENCODING;
    zcl_payment_request temporary = {0};
    status = zcl_address_parse(text + start, 35, network, &temporary.address);
    if (status != ZCL_OK)
        return status;
    memcpy(temporary.address_text, text + start, sizeof(temporary.address_text));
    status = parse_metadata(text, text_len, start, &temporary);
    if (status != ZCL_OK)
        return status;
    *request = temporary;
    return ZCL_OK;
}
