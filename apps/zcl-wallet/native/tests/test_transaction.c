/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Transaction check failed at %d\n", __LINE__); abort(); } } while (0)

static size_t unhex(const char *hex, uint8_t *bytes, size_t capacity)
{
    static const char digits[] = "0123456789abcdef";
    const size_t size = strlen(hex);
    CHECK(size % 2 == 0 && size / 2 <= capacity);
    for (size_t i = 0; i < size / 2; ++i) {
        const char *a = strchr(digits, hex[2 * i]), *b = strchr(digits, hex[2 * i + 1]);
        CHECK(a != NULL && b != NULL);
        bytes[i] = (uint8_t)((a - digits) * 16 + (b - digits));
    }
    return size / 2;
}

static void parse_refused(const uint8_t *wire, size_t length, zcl_status expected)
{
    struct { uint64_t before; zcl_transparent_tx tx; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_transparent_tx previous;
    memcpy(&previous, &box.tx, sizeof(previous));
    const zcl_status status = zcl_transaction_parse(wire, length, &box.tx);
    CHECK(status != ZCL_OK && (expected == ZCL_OK || status == expected));
    CHECK(memcmp(&previous, &box.tx, sizeof(previous)) == 0);
    CHECK(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
}

static void serialize_refused(const zcl_transparent_tx *tx, size_t capacity, zcl_status expected)
{
    uint8_t wire[ZCL_TX_WIRE_MAX + 2], previous[sizeof(wire)];
    memset(wire, 0xa5, sizeof(wire));
    memcpy(previous, wire, sizeof(wire));
    size_t length = SIZE_MAX;
    CHECK(zcl_transaction_serialize(tx, wire + 1, capacity, &length) == expected);
    CHECK(length == SIZE_MAX && memcmp(wire, previous, sizeof(wire)) == 0);
}

static void pinned_prefixes(void)
{
    for (size_t i = 0; i < sizeof(transaction_vectors) / sizeof(transaction_vectors[0]); ++i) {
        uint8_t wire[ZCL_TX_WIRE_MAX + 1], encoded[ZCL_TX_WIRE_MAX], txid[34], expected[32];
        const size_t length = unhex(transaction_vectors[i].hex, wire, sizeof(wire));
        zcl_transparent_tx tx;
        CHECK(zcl_transaction_parse(wire, length, &tx) == ZCL_OK);
        CHECK(tx.input_count == transaction_vectors[i].inputs);
        CHECK(tx.output_count == transaction_vectors[i].outputs);
        CHECK(tx.lock_time == transaction_vectors[i].lock_time && tx.expiry_height == 0);
        CHECK(tx.inputs[0].previous_txid[0] == wire[40] && tx.inputs[0].previous_txid[31] == wire[9]);
        size_t encoded_size = 0;
        CHECK(zcl_transaction_serialize(&tx, encoded, sizeof(encoded), &encoded_size) == ZCL_OK);
        CHECK(encoded_size == length && memcmp(wire, encoded, length) == 0);
        CHECK(unhex(transaction_vectors[i].txid, expected, sizeof(expected)) == sizeof(expected));
        memset(txid, 0xa5, sizeof(txid));
        CHECK(zcl_transaction_id(&tx, txid + 1, 32) == ZCL_OK);
        CHECK(txid[0] == 0xa5 && txid[33] == 0xa5 && memcmp(txid + 1, expected, 32) == 0);
        for (size_t cut = 0; cut < length; ++cut) parse_refused(wire, cut, ZCL_OK);
        wire[length] = 0;
        parse_refused(wire, length + 1, ZCL_INVALID_ENCODING);
        for (size_t cap = 0; cap < length; ++cap) serialize_refused(&tx, cap, ZCL_BUFFER_TOO_SMALL);
        tx.inputs[0].script[0] ^= 1;
        CHECK(zcl_transaction_id(&tx, txid, sizeof(txid)) == ZCL_OK);
        CHECK(memcmp(txid, expected, 32) != 0);
    }
}

static void minimal(zcl_transparent_tx *tx)
{
    memset(tx, 0, sizeof(*tx));
    tx->input_count = tx->output_count = 1;
    for (size_t i = 0; i < 32; ++i) tx->inputs[0].previous_txid[i] = (uint8_t)(255 - i);
    tx->inputs[0].previous_index = UINT32_MAX;
    tx->inputs[0].sequence = UINT32_MAX;
    tx->outputs[0].value = ZCL_MAX_MONEY;
}

static void maximum(void)
{
    zcl_transparent_tx tx;
    minimal(&tx);
    tx.input_count = ZCL_TX_INPUT_MAX;
    tx.output_count = ZCL_TX_OUTPUT_MAX;
    tx.lock_time = UINT32_MAX;
    tx.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    for (size_t i = 0; i < tx.input_count; ++i) {
        tx.inputs[i] = tx.inputs[0];
        tx.inputs[i].previous_index = (uint32_t)i;
        tx.inputs[i].script_len = ZCL_TX_INPUT_SCRIPT_MAX;
        memset(tx.inputs[i].script, 0xff, sizeof(tx.inputs[i].script));
    }
    for (size_t i = 0; i < tx.output_count; ++i) {
        tx.outputs[i].script_len = ZCL_TX_OUTPUT_SCRIPT_MAX;
        memset(tx.outputs[i].script, (int)i, sizeof(tx.outputs[i].script));
    }
    uint8_t wire[ZCL_TX_WIRE_MAX + 2];
    memset(wire, 0xa5, sizeof(wire));
    size_t size = 0;
    CHECK(zcl_transaction_serialize(&tx, wire + 1, ZCL_TX_WIRE_MAX, &size) == ZCL_OK);
    CHECK(size == ZCL_TX_WIRE_MAX && wire[0] == 0xa5 && wire[sizeof(wire) - 1] == 0xa5);
    zcl_transparent_tx parsed;
    CHECK(zcl_transaction_parse(wire + 1, size, &parsed) == ZCL_OK);
    CHECK(parsed.input_count == tx.input_count && parsed.output_count == tx.output_count);
    CHECK(parsed.lock_time == UINT32_MAX && parsed.expiry_height == ZCL_TX_EXPIRY_LIMIT - 1);
    CHECK(parsed.inputs[7].script_len == 128 && parsed.inputs[7].script[127] == 255);
    CHECK(parsed.outputs[15].script_len == 25 && parsed.outputs[15].script[24] == 15);
    CHECK(parsed.outputs[0].value == ZCL_MAX_MONEY && parsed.outputs[15].value == 0);
    serialize_refused(&tx, size - 1, ZCL_BUFFER_TOO_SMALL);
    parse_refused(wire + 1, size + 1, ZCL_RESOURCE_EXHAUSTED);
}

static void invalid_objects(void)
{
    zcl_transparent_tx tx, bad;
    minimal(&tx);
    bad = tx; bad.input_count = 0; serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_INVALID_ENCODING);
    bad = tx; bad.output_count = 0; serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_INVALID_ENCODING);
    bad = tx; bad.input_count = SIZE_MAX; serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_RESOURCE_EXHAUSTED);
    bad = tx; bad.output_count = SIZE_MAX; serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_RESOURCE_EXHAUSTED);
    bad = tx; bad.inputs[0].script_len = SIZE_MAX; serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_RESOURCE_EXHAUSTED);
    bad = tx; bad.outputs[0].script_len = SIZE_MAX; serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_RESOURCE_EXHAUSTED);
    bad = tx; bad.expiry_height = ZCL_TX_EXPIRY_LIMIT; serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE);
    bad = tx; bad.outputs[0].value = UINT64_MAX; serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE);
    bad = tx; bad.output_count = 2; bad.outputs[1].value = 1;
    serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_OUT_OF_RANGE);
    bad = tx; bad.input_count = 2; bad.inputs[1] = bad.inputs[0];
    serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_INVALID_ENCODING);
    bad = tx; memset(bad.inputs[0].previous_txid, 0, 32);
    serialize_refused(&bad, ZCL_TX_WIRE_MAX, ZCL_INVALID_ENCODING);
    uint8_t id[32], previous[32];
    memset(id, 0xa5, sizeof(id)); memcpy(previous, id, sizeof(id));
    CHECK(zcl_transaction_id(&bad, id, sizeof(id)) == ZCL_INVALID_ENCODING);
    CHECK(memcmp(id, previous, sizeof(id)) == 0);
    CHECK(zcl_transaction_id(&tx, id, 31) == ZCL_BUFFER_TOO_SMALL);
    CHECK(memcmp(id, previous, sizeof(id)) == 0);
}

static void wire_boundaries(void)
{
    zcl_transparent_tx tx;
    minimal(&tx);
    uint8_t wire[128], bad[128];
    size_t length = 0;
    CHECK(zcl_transaction_serialize(&tx, wire, sizeof(wire), &length) == ZCL_OK && length == 79);
    /* Empty scripts and a zero-hash/non-null-index outpoint are codec-valid. */
    for (size_t i = 0; i < 32; ++i) tx.inputs[0].previous_txid[i] = 0;
    tx.inputs[0].previous_index = 0;
    CHECK(zcl_transaction_serialize(&tx, bad, sizeof(bad), &length) == ZCL_OK);
    CHECK(zcl_transaction_parse(bad, length, &tx) == ZCL_OK);
    static const size_t unsupported[] = {0, 3, 4, 7, 68, 75, 76, 77, 78};
    for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
        memcpy(bad, wire, length); bad[unsupported[i]] ^= 1;
        parse_refused(bad, length, ZCL_UNSUPPORTED);
    }
    static const size_t count_offsets[] = {8, 45, 50, 59};
    for (size_t i = 0; i < sizeof(count_offsets) / sizeof(count_offsets[0]); ++i) {
        const size_t offset = count_offsets[i];
        for (uint16_t prefix = 253; prefix <= 255; ++prefix) {
            const size_t width = (size_t)1 << (prefix - 252);
            memcpy(bad, wire, offset);
            bad[offset] = (uint8_t)prefix;
            memset(bad + offset + 1, 0, width);
            bad[offset + 1] = wire[offset];
            memcpy(bad + offset + 1 + width, wire + offset + 1, length - offset - 1);
            parse_refused(bad, length + width, ZCL_INVALID_ENCODING);
            memset(bad + offset + 1, 0xff, width);
            parse_refused(bad, length + width, ZCL_RESOURCE_EXHAUSTED);
        }
    }
    memcpy(bad, wire, length); bad[8] = 9; parse_refused(bad, length, ZCL_RESOURCE_EXHAUSTED);
    memcpy(bad, wire, length); bad[45] = 129; parse_refused(bad, length, ZCL_RESOURCE_EXHAUSTED);
    memcpy(bad, wire, length); bad[50] = 17; parse_refused(bad, length, ZCL_RESOURCE_EXHAUSTED);
    memcpy(bad, wire, length); bad[59] = 26; parse_refused(bad, length, ZCL_RESOURCE_EXHAUSTED);
    memcpy(bad, wire, length); memset(bad + 51, 0xff, 8); parse_refused(bad, length, ZCL_OUT_OF_RANGE);
    memcpy(bad, wire, length); memset(bad + 64, 0xff, 4); parse_refused(bad, length, ZCL_OUT_OF_RANGE);
    memcpy(bad, wire, length); memset(bad + 9, 0, 32); parse_refused(bad, length, ZCL_INVALID_ENCODING);
}

static void null_arguments(void)
{
    zcl_transparent_tx tx;
    minimal(&tx);
    uint8_t wire[ZCL_TX_WIRE_MAX];
    size_t length = 0;
    CHECK(zcl_transaction_parse(NULL, 0, &tx) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_parse(wire, 0, NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_serialize(NULL, wire, sizeof(wire), &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_serialize(&tx, NULL, 0, &length) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_serialize(&tx, wire, sizeof(wire), NULL) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_id(NULL, wire, sizeof(wire)) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_id(&tx, NULL, 0) == ZCL_INVALID_ARGUMENT);
}

int main(void)
{
    pinned_prefixes(); maximum(); invalid_objects(); wire_boundaries(); null_arguments();
    puts("Bounded transparent transaction checks passed");
    return 0;
}
