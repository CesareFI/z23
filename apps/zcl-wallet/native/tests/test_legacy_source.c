/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_source_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "legacy_source_vectors.h"
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Legacy source at %d\n", __LINE__); abort(); } } while (0)
static uint8_t wire[4001], before[4001];
static struct { uint64_t before; zcl_source_view view; uint64_t after; } output, unchanged;

static zcl_status inspect(size_t length, uint32_t index)
{
    memset(&output, 0xa5, sizeof(output)); memcpy(&unchanged, &output, sizeof(output));
    memcpy(before, wire, sizeof(wire));
    const zcl_status status = zcl_legacy_source_inspect(wire, length, index, &output.view);
    CHECK(memcmp(wire, before, sizeof(wire)) == 0);
    CHECK(output.before == unchanged.before && output.after == unchanged.after);
    if (status != ZCL_OK) CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0);
    return status;
}

static uint32_t scalar(const uint8_t bytes[4])
{
    uint32_t value = 0;
    for (size_t i = 4; i > 0; --i) value = (value << 8) | bytes[i - 1];
    return value;
}

static void vectors(void)
{
    for (size_t row = 0; row < sizeof(legacy_vectors) / sizeof(legacy_vectors[0]); ++row) {
        const size_t length = legacy_vectors[row].length;
        memcpy(wire, legacy_vectors[row].wire, length);
        for (uint32_t index = 0; index < legacy_vectors[row].outputs; ++index) {
            CHECK(inspect(length, index) == ZCL_OK);
            CHECK(memcmp(output.view.transaction_id, legacy_vectors[row].id, 32) == 0);
            CHECK(output.view.input_count == legacy_vectors[row].inputs && output.view.output_count == legacy_vectors[row].outputs);
            CHECK(output.view.joinsplit_count == legacy_vectors[row].joins);
            CHECK(output.view.spend_count == 0 && output.view.shielded_count == 0 && output.view.value_balance == 0);
            CHECK(output.view.lock_time == scalar(wire + legacy_vectors[row].lock_offset));
            const uint32_t expiry = legacy_vectors[row].version == 3 ? scalar(wire + legacy_vectors[row].lock_offset + 4) : 0;
            CHECK(output.view.expiry_height == expiry);
        }
        CHECK(inspect(length, UINT32_MAX) == ZCL_OUT_OF_RANGE);
        for (size_t cut = 0; cut < length; ++cut) CHECK(inspect(cut, 0) != ZCL_OK);
        CHECK(inspect(length + 1, 0) == ZCL_INVALID_ENCODING);
        CHECK(zcl_v4_source_inspect(wire, length, 0, &output.view) == ZCL_UNSUPPORTED);
    }
}

static void prefix_at(size_t length, size_t offset, unsigned mask)
{
    const uint8_t original = wire[offset];
    for (unsigned byte = 0; byte <= 255; ++byte) {
        wire[offset] = (uint8_t)byte;
        const zcl_status expected = byte == mask || byte == mask + 1 ? ZCL_OK : ZCL_INVALID_ENCODING;
        CHECK(inspect(length, 0) == expected);
    }
    wire[offset] = original;
}

static void proof_prefixes(void)
{
    static const size_t offsets[] = {304,337,370,435,468,501,534,567};
    for (size_t row = 1; row <= 4; ++row) {
        const size_t length = legacy_vectors[row].length;
        memcpy(wire, legacy_vectors[row].wire, length);
        for (size_t join = 0; join < legacy_vectors[row].joins; ++join) {
            for (size_t point = 0; point < 8; ++point) {
                const size_t offset = legacy_vectors[row].join_offset + join * 1802 + offsets[point];
                prefix_at(length, offset, point == 2 ? 10 : 2);
            }
        }
    }
}

static void bounds(void)
{
    memcpy(wire, legacy_vectors[1].wire, legacy_vectors[1].length);
    CHECK(inspect(ZCL_LEGACY_SOURCE_MAX + 1, 0) == ZCL_RESOURCE_EXHAUSTED);
    CHECK(inspect(SIZE_MAX, 0) == ZCL_RESOURCE_EXHAUSTED);
    CHECK(zcl_legacy_source_inspect(NULL, 0, 0, &output.view) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_legacy_source_inspect(wire, 0, 0, NULL) == ZCL_INVALID_ARGUMENT);
    wire[4] ^= 1; CHECK(inspect(legacy_vectors[1].length, 0) == ZCL_UNSUPPORTED); wire[4] ^= 1;
    wire[0] = 4; CHECK(inspect(legacy_vectors[1].length, 0) == ZCL_UNSUPPORTED);
    wire[0] = 3; wire[3] = 0; CHECK(inspect(legacy_vectors[1].length, 0) == ZCL_UNSUPPORTED);
    wire[0] = 0; CHECK(inspect(legacy_vectors[1].length, 0) == ZCL_UNSUPPORTED);
}

int main(void)
{
    vectors(); proof_prefixes(); bounds();
    puts("Legacy v1/v2/v3 source identities, bounds and PHGR prefixes passed"); return 0;
}
