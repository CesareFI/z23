/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include "zcl_keys.h"
#include <string.h>

void zcl_electrum_line_reset(zcl_electrum_line *line)
{
    if (line != NULL) zcl_secure_zero(line, sizeof(*line));
}

/* Preserve the inexpensive byte path for maximally fragmented input. */
static zcl_status append_single(zcl_electrum_line *line, uint8_t byte, size_t *consumed)
{
    if (byte == '\n') line->ready = true;
    else {
        if (line->used >= sizeof(line->bytes)) {
            line->failed = true;
            return ZCL_OUT_OF_RANGE;
        }
        line->bytes[line->used++] = byte;
    }
    *consumed = 1;
    return ZCL_OK;
}

static zcl_status append_span(zcl_electrum_line *line, const uint8_t *input,
    size_t length, size_t *consumed)
{
    /* line_status proved used<=capacity. Inspect at most one byte beyond the
     * remaining room: a LF there terminates an exactly full frame. All counts
     * are <=FRAME_MAX+1, and caller spans are stable and non-overlapping. */
    const size_t available = sizeof(line->bytes) - line->used;
    const size_t scan = length <= available ? length : available + 1;
    const uint8_t *newline = memchr(input, '\n', scan);
    const size_t count = newline != NULL ? (size_t)(newline - input) : scan;
    const size_t copied = count <= available ? count : available;
    memcpy(line->bytes + line->used, input, copied);
    line->used += copied;
    if (count > available) {
        line->failed = true;
        return ZCL_OUT_OF_RANGE;
    }
    line->ready = newline != NULL;
    *consumed = count + (size_t)line->ready;
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
    if (length == 1) return append_single(line, input[0], consumed);
    return append_span(line, input, length, consumed);
}
