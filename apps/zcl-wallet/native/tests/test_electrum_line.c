/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_electrum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void check(const uint8_t *data, size_t size)
{
    if (LLVMFuzzerTestOneInput(data, size) != 0) abort();
}

static void short_lines(void)
{
    uint8_t data[66];
    for (size_t size = 0; size <= sizeof(data); ++size) {
        for (size_t lf = 0; lf <= size; ++lf) {
            /* Embedded NUL and CR are bytes, not framing terminators. */
            for (size_t i = 0; i < size; ++i) data[i] = (uint8_t)(i % 14);
            if (lf < size) data[lf] = '\n';
            check(data, size);
        }
    }
}

static void capacity_lines(void)
{
    static uint8_t data[ZCL_ELECTRUM_FRAME_MAX + 2];
    const size_t lengths[] = {1, 64, 255, 256, 4095, 4096, 16383, 16384, 16385, sizeof(data)};
    const size_t endings[] = {0, 1, 63, 254, 4095, 16382, 16383, 16384, 16385};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        memset(data, 'x', sizeof(data));
        check(data, lengths[i]);
        for (size_t j = 0; j < sizeof(endings) / sizeof(endings[0]); ++j) {
            memset(data, 'x', sizeof(data));
            data[endings[j]] = '\n';
            check(data, lengths[i]);
        }
    }
}

static void refusal(zcl_electrum_line *line, const uint8_t *input, size_t size,
    size_t *consumed, zcl_status expected)
{
    static zcl_electrum_line before;
    if (line != NULL) memcpy(&before, line, sizeof(before));
    const size_t old_count = consumed != NULL ? *consumed : 0;
    if (zcl_electrum_line_feed(line, input, size, consumed) != expected) abort();
    if (consumed != NULL && *consumed != old_count) abort();
    if (line != NULL && memcmp(&before, line, sizeof(before)) != 0) abort();
}

static void argument_errors(void)
{
    static zcl_electrum_line line;
    const uint8_t input[] = {'x'};
    size_t consumed = SIZE_MAX;
    zcl_electrum_line_reset(&line);
    refusal(NULL, input, 1, &consumed, ZCL_INVALID_ARGUMENT);
    refusal(&line, NULL, 0, &consumed, ZCL_INVALID_ARGUMENT);
    refusal(&line, input, 1, NULL, ZCL_INVALID_ARGUMENT);
    refusal(&line, input, SIZE_MAX, &consumed, ZCL_OUT_OF_RANGE);
    /* Defensive corruption checks must precede subtraction/indexing. */
    line.used = sizeof(line.bytes) + 1;
    refusal(&line, input, 1, &consumed, ZCL_INVALID_ENCODING);
    line.used = SIZE_MAX;
    refusal(&line, input, 1, &consumed, ZCL_INVALID_ENCODING);
    zcl_electrum_line_reset(&line);
}

int main(void)
{
    short_lines();
    capacity_lines();
    argument_errors();
    puts("Electrum framing matches bytewise model across fragmentation and capacity boundaries");
    return 0;
}
