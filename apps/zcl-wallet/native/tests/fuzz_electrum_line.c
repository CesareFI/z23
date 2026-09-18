/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/* One serialized host fixture owns both large buffers; no Android stack use.
 * Compare complete bytes, including unused capacity, against a bytewise model. */
static struct {
    uint64_t before;
    zcl_electrum_line line;
    uint64_t after;
} actual;
static zcl_electrum_line model;

static void require(bool condition)
{
    if (!condition) {
        fputs("Electrum line model mismatch\n", stderr);
        abort();
    }
}

static void compare(void)
{
    require(actual.before == UINT64_C(0xa5a5a5a5a5a5a5a5));
    require(actual.after == actual.before);
    require(actual.line.used == model.used);
    require(actual.line.ready == model.ready);
    require(actual.line.failed == model.failed);
    require(memcmp(actual.line.bytes, model.bytes, sizeof(model.bytes)) == 0);
}

static void reset(void)
{
    memset(&actual, 0xa5, sizeof(actual));
    zcl_electrum_line_reset(&actual.line);
    memset(&model, 0, sizeof(model));
    compare();
}

static zcl_status model_feed(const uint8_t *data, size_t size, size_t *consumed)
{
    if (model.failed) return ZCL_INVALID_ENCODING;
    if (model.ready) return ZCL_BUSY;
    if (size > sizeof(model.bytes) + 1) return ZCL_OUT_OF_RANGE;
    for (size_t i = 0; i < size; ++i) {
        if (data[i] == '\n') {
            model.ready = true;
            *consumed = i + 1;
            return ZCL_OK;
        }
        if (model.used == sizeof(model.bytes)) {
            model.failed = true;
            return ZCL_OUT_OF_RANGE;
        }
        model.bytes[model.used++] = data[i];
    }
    *consumed = size;
    return ZCL_OK;
}

static size_t feed(const uint8_t *data, size_t size)
{
    size_t expected = SIZE_MAX, consumed = SIZE_MAX;
    const zcl_status status = model_feed(data, size, &expected);
    require(zcl_electrum_line_feed(&actual.line, data, size, &consumed) == status);
    require(consumed == expected);
    compare();
    return consumed;
}

static void fragmented(const uint8_t *data, size_t size, size_t chunk)
{
    reset();
    for (size_t at = 0; at < size;) {
        const size_t left = size - at;
        const size_t count = left < chunk ? left : chunk;
        const size_t consumed = feed(data + at, count);
        if (consumed == SIZE_MAX) return;
        require(consumed != 0 && consumed <= count);
        at += consumed;
        if (model.ready) {
            (void)feed(data + at, size - at); /* Ready remains immutable. */
            reset();
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > ZCL_ELECTRUM_FRAME_MAX + 2) return 0;
    reset();
    (void)feed(data, size);
    (void)feed((const uint8_t *)"\n", 1);
    (void)feed((const uint8_t *)"x", 1);
    if (size == 0) return 0;
    fragmented(data, size, (size_t)data[0] + 1);
    /* A valid prefix near the ceiling exercises partial-copy refusal, a LF
     * exactly at capacity, and reset after sticky failure. No private state
     * fields are forged to reach those boundaries. */
    reset();
    static uint8_t prefix[ZCL_ELECTRUM_FRAME_MAX];
    memset(prefix, 'p', sizeof(prefix));
    (void)feed(prefix, sizeof(prefix) - data[0]);
    (void)feed(data, size);
    (void)feed((const uint8_t *)"\n", 1);
    reset();
    return 0;
}
