/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_source_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "source_vectors.h"

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "V4 source check failed at %d\n", __LINE__); abort(); } } while (0)
static uint8_t wire[ZCL_V4_SOURCE_MAX + 1];
static size_t used;
static zcl_transparent_tx projected;

static void refused(const uint8_t *bytes, size_t length, uint32_t index, zcl_status expected)
{
    struct { uint8_t before[8]; zcl_v4_source value; uint8_t after[8]; } output, original;
    memset(&output, 0xa5, sizeof(output)); memcpy(&original, &output, sizeof(output));
    CHECK(zcl_v4_source_inspect(bytes, length, index, &output.value) == expected);
    CHECK(memcmp(&output, &original, sizeof(output)) == 0);
}

static void vectors(void)
{
    for (size_t n = 0; n < sizeof(source_vectors) / sizeof(source_vectors[0]); ++n) {
        memcpy(wire, source_vectors[n].wire, source_vectors[n].prefix);
        /* Upstream hash vectors need not be consensus-valid. The independent
         * narrow output decoder needs a zero-expiry projection; original raw
         * expiry bytes are checked separately below and never changed there. */
        memset(wire + source_vectors[n].prefix - 4, 0, 4);
        memset(wire + source_vectors[n].prefix, 0, 11);
        const zcl_status parsed = zcl_transaction_parse(wire, source_vectors[n].prefix + 11, &projected);
        if (parsed != ZCL_OK) fprintf(stderr, "Projection %zu status %d\n", n, (int)parsed);
        CHECK(parsed == ZCL_OK);
        for (uint32_t index = 0; index < source_vectors[n].outputs; ++index) {
            zcl_v4_source view = {0};
            CHECK(zcl_v4_source_inspect(source_vectors[n].wire, source_vectors[n].length, index, &view) == ZCL_OK);
            CHECK(memcmp(view.transaction_id, source_vectors[n].id, 32) == 0);
            CHECK(view.input_count == source_vectors[n].inputs && view.output_count == source_vectors[n].outputs);
            CHECK(view.spend_count == source_vectors[n].spends && view.shielded_count == source_vectors[n].shielded);
            CHECK(view.joinsplit_count == source_vectors[n].joins);
            CHECK(view.lock_time == projected.lock_time);
            for (size_t i = 0; i < 4; ++i)
                CHECK((uint8_t)(view.expiry_height >> (8 * i)) == source_vectors[n].wire[source_vectors[n].prefix - 4 + i]);
            CHECK(memcmp(&view.output, &projected.outputs[index], sizeof(view.output)) == 0);
            const uint64_t raw = (uint64_t)view.value_balance;
            for (size_t i = 0; i < 8; ++i)
                CHECK((uint8_t)(raw >> (8 * i)) == source_vectors[n].wire[source_vectors[n].prefix + i]);
        }
    }
}

static void truncated_and_changed(void)
{
    for (size_t n = 0; n < sizeof(source_vectors) / sizeof(source_vectors[0]); ++n) {
        for (size_t length = 0; length < source_vectors[n].length; ++length)
            refused(source_vectors[n].wire, length, 0, ZCL_INVALID_ENCODING);
        memcpy(wire, source_vectors[n].wire, source_vectors[n].length);
        for (size_t offset = 0; offset < source_vectors[n].length; ++offset) {
            wire[offset] ^= 1;
            zcl_v4_source view;
            const zcl_status status = zcl_v4_source_inspect(wire, source_vectors[n].length, 0, &view);
            if (status == ZCL_OK) CHECK(memcmp(view.transaction_id, source_vectors[n].id, 32) != 0);
            wire[offset] ^= 1;
        }
        wire[source_vectors[n].length] = 0;
        refused(wire, source_vectors[n].length + 1, 0, ZCL_INVALID_ENCODING);
        refused(wire, source_vectors[n].length, UINT32_MAX, ZCL_OUT_OF_RANGE);
    }
}

static void bytes(uint64_t value, size_t length)
{
    CHECK(length <= 8 && length <= sizeof(wire) - used);
    for (size_t i = 0; i < length; ++i) wire[used++] = (uint8_t)(value >> (8 * i));
}

static void zeros(size_t length)
{
    CHECK(length <= sizeof(wire) - used);
    memset(wire + used, 0, length); used += length;
}

static void compact(size_t amount)
{
    if (amount < 253) bytes(amount, 1);
    else if (amount <= UINT16_MAX) { bytes(253, 1); bytes(amount, 2); }
    else { CHECK(amount <= UINT32_MAX); bytes(254, 1); bytes(amount, 4); }
}

static void build(size_t ins, size_t outs, size_t script, unsigned tail)
{
    used = 0;
    bytes(UINT32_C(0x80000004), 4); bytes(UINT32_C(0x892f2085), 4);
    compact(ins);
    for (size_t i = 0; i < ins; ++i) { zeros(36); compact(script); zeros(script); zeros(4); }
    compact(outs);
    for (size_t i = 0; i < outs; ++i) { bytes(7, 8); compact(25); zeros(25); }
    bytes(UINT32_MAX, 4); bytes(UINT32_MAX, 4); /* Raw fields, not finality/expiry approval. */
    bytes(UINT64_C(0x8000000000000000), 8);
    compact(tail & 1U); if ((tail & 1U) != 0) zeros(384);
    compact((tail >> 1) & 1U); if ((tail & 2U) != 0) zeros(948);
    compact((tail >> 2) & 1U); if ((tail & 4U) != 0) zeros(1698 + 96);
    if ((tail & 3U) != 0) zeros(64);
}

static void shapes(void)
{
    for (unsigned tail = 0; tail < 8; ++tail) {
        build(0, 1, 0, tail);
        zcl_v4_source view;
        CHECK(zcl_v4_source_inspect(wire, used, 0, &view) == ZCL_OK);
        CHECK(view.input_count == 0 && view.output_count == 1 && view.output.value == 7);
        CHECK(view.spend_count == (tail & 1U) && view.shielded_count == ((tail >> 1) & 1U));
        CHECK(view.joinsplit_count == ((tail >> 2) & 1U) && view.value_balance == INT64_MIN);
        CHECK(view.lock_time == UINT32_MAX && view.expiry_height == UINT32_MAX);
    }
    build(253, 253, 0, 7);
    zcl_v4_source view;
    CHECK(zcl_v4_source_inspect(wire, used, 252, &view) == ZCL_OK);
    CHECK(view.input_count == 253 && view.output_count == 253);
    build(1, 1, ZCL_V4_SOURCE_MAX - 108, 0);
    CHECK(used == ZCL_V4_SOURCE_MAX);
    CHECK(zcl_v4_source_inspect(wire, used, 0, &view) == ZCL_OK);
    refused(wire, used + 1, 0, ZCL_RESOURCE_EXHAUSTED);
    refused(wire, SIZE_MAX, 0, ZCL_RESOURCE_EXHAUSTED);
}

static void malformed(void)
{
    build(1, 1, 0, 0);
    zcl_v4_source output;
    CHECK(zcl_v4_source_inspect(wire, used, 0, NULL) == ZCL_INVALID_ARGUMENT);
    refused(NULL, used, 0, ZCL_INVALID_ARGUMENT);
    wire[8] = 255; refused(wire, used, 0, ZCL_INVALID_ENCODING);
    build(0, 1, 0, 0); wire[8] = 253;
    refused(wire, used, 0, ZCL_INVALID_ENCODING);
    build(0, 1, 0, 0); memset(wire + 10, 0xff, 8);
    refused(wire, used, 0, ZCL_OUT_OF_RANGE);
    build(0, 1, 0, 0); wire[18] = 26;
    refused(wire, used, 0, ZCL_UNSUPPORTED);
    build(0, 1, 0, 0); wire[0] = 5;
    refused(wire, used, 0, ZCL_UNSUPPORTED);
    build(0, 0, 0, 7);
    CHECK(zcl_v4_source_inspect(wire, used, 0, &output) == ZCL_OUT_OF_RANGE);
}

static void noncanonical(void)
{
    const size_t widths[] = {2,4,8};
    for (size_t i = 0; i < 3; ++i) {
        build(0, 1, 0, 0);
        /* Encode the original zero input count with an unnecessary extension;
         * keep every later field and the complete tail in its correct place. */
        memmove(wire + 9 + widths[i], wire + 9, used - 9);
        wire[8] = (uint8_t)(253 + i); memset(wire + 9, 0, widths[i]); used += widths[i];
        refused(wire, used, 0, ZCL_INVALID_ENCODING);
    }
}

int main(void)
{
    vectors(); truncated_and_changed(); shapes(); malformed(); noncanonical();
    puts("Full v4 source layout, independent wire identities and bounded selected outputs passed");
    return 0;
}
