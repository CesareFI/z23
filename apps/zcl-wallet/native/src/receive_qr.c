/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_qr.h"
#include "qrcodegen.h"

#include <string.h>

#define RECEIVE_VERSION_MAX 4
#define RECEIVE_SYMBOL_MAX 33
#define RECEIVE_BORDER 4

static void expand_modules(const uint8_t *encoded, int symbol_side,
                           uint8_t *modules, size_t side)
{
    for (int y = 0; y < symbol_side; ++y) {
        for (int x = 0; x < symbol_side; ++x) {
            const size_t row = (size_t)(y + RECEIVE_BORDER);
            const size_t column = (size_t)(x + RECEIVE_BORDER);
            modules[row * side + column] = (uint8_t)qrcodegen_getModule(encoded, x, y);
        }
    }
}

zcl_status zcl_receive_qr(const uint8_t *text, size_t text_len, zcl_network network,
                          uint8_t *modules, size_t capacity, size_t *side)
{
    if (text == NULL || modules == NULL || side == NULL)
        return ZCL_INVALID_ARGUMENT;
    if (text_len != 35)
        return ZCL_INVALID_ENCODING;
    zcl_address address = {0};
    const zcl_status parsed = zcl_address_parse(text, text_len, network, &address);
    if (parsed != ZCL_OK)
        return parsed;
    uint8_t temporary[qrcodegen_BUFFER_LEN_FOR_VERSION(RECEIVE_VERSION_MAX)] = {0};
    uint8_t encoded[qrcodegen_BUFFER_LEN_FOR_VERSION(RECEIVE_VERSION_MAX)] = {0};
    memcpy(temporary, text, text_len);
    if (!qrcodegen_encodeBinary(temporary, text_len, encoded, qrcodegen_Ecc_QUARTILE,
                               1, RECEIVE_VERSION_MAX, qrcodegen_Mask_AUTO, true))
        return ZCL_INVALID_ENCODING;
    const int symbol_side = qrcodegen_getSize(encoded);
    if (symbol_side < 21 || symbol_side > RECEIVE_SYMBOL_MAX)
        return ZCL_INVALID_ENCODING;
    const size_t width = (size_t)(symbol_side + 2 * RECEIVE_BORDER);
    const size_t count = width * width; /* width <= 41; no caller-sized arithmetic. */
    if (capacity < count)
        return ZCL_BUFFER_TOO_SMALL;
    /* All fallible operations are complete; write only the validated span. */
    memset(modules, 0, count);
    expand_modules(encoded, symbol_side, modules, width);
    *side = width;
    return ZCL_OK;
}
