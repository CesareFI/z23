/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "zcl_keys.h"

void zcl_electrum_line_reset(zcl_electrum_line *line)
{
    if (line != NULL) zcl_secure_zero(line, sizeof(*line));
}

static zcl_status append_byte(zcl_electrum_line *line, uint8_t byte)
{
    if (byte == '\n') {
        line->ready = true;
        return ZCL_OK;
    }
    if (line->used >= sizeof(line->bytes)) {
        line->failed = true;
        return ZCL_OUT_OF_RANGE;
    }
    line->bytes[line->used++] = byte;
    return ZCL_OK;
}

static zcl_status line_status(const zcl_electrum_line *line, size_t length)
{
    if (line->failed || line->used > sizeof(line->bytes)) return ZCL_INVALID_ENCODING;
    if (line->ready) return ZCL_BUSY;
    return length > ZCL_ELECTRUM_FRAME_MAX + 1 ? ZCL_OUT_OF_RANGE : ZCL_OK;
}

zcl_status zcl_electrum_line_feed(zcl_electrum_line *line, const uint8_t *input,
                                  size_t length, size_t *consumed)
{
    if (line == NULL || input == NULL || consumed == NULL) return ZCL_INVALID_ARGUMENT;
    const zcl_status initial = line_status(line, length);
    if (initial != ZCL_OK) return initial;
    for (size_t i = 0; i < length; ++i) {
        const zcl_status status = append_byte(line, input[i]);
        if (status != ZCL_OK) return status;
        if (line->ready) { *consumed = i + 1; return ZCL_OK; }
    }
    *consumed = length;
    return ZCL_OK;
}
