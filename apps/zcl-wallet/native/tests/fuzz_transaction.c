/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "zcl_transaction.h"
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void script_bytes(const uint8_t *data, size_t size)
{
    for (int network = 0; network < 3; ++network) {
        zcl_address address, old;
        memset(&address, 0xa5, sizeof(address));
        memcpy(&old, &address, sizeof(old));
        const zcl_status status = zcl_address_from_script(data, size, (zcl_network)network, &address);
        if (status != ZCL_OK) {
            if (memcmp(&old, &address, sizeof(old)) != 0) abort();
            continue;
        }
        if (address.network != (zcl_network)network || network == 2) abort();
        uint8_t encoded[25];
        size_t length = SIZE_MAX;
        if (zcl_address_script(&address, encoded, sizeof(encoded), &length) != ZCL_OK) abort();
        if (length != size || memcmp(data, encoded, length) != 0) abort();
    }
}

static void matching_prevout(const zcl_transparent_tx *tx, const uint8_t *wire, size_t length,
                             const uint8_t id[32])
{
    zcl_tx_input input = {0};
    memcpy(input.previous_txid, id, sizeof(input.previous_txid));
    input.previous_index = (uint32_t)(id[0] % tx->output_count);
    zcl_tx_output output;
    if (zcl_transaction_prevout(&input, wire, length, &output) != ZCL_OK) abort();
    const zcl_tx_output *expected = &tx->outputs[input.previous_index];
    if (output.value != expected->value || output.script_len != expected->script_len) abort();
    if (memcmp(output.script, expected->script, expected->script_len) != 0) abort();
    zcl_tx_output old;
    memcpy(&old, &output, sizeof(old));
    input.previous_txid[0] ^= 1;
    if (zcl_transaction_prevout(&input, wire, length, &output) != ZCL_INVALID_ENCODING) abort();
    if (memcmp(&old, &output, sizeof(old)) != 0) abort();
    input.previous_txid[0] ^= 1;
    input.previous_index = UINT32_MAX;
    if (zcl_transaction_prevout(&input, wire, length, &output) != ZCL_OUT_OF_RANGE) abort();
    if (memcmp(&old, &output, sizeof(old)) != 0) abort();
}

static void parse_wire(const uint8_t *data, size_t size)
{
    struct { uint64_t before; zcl_transparent_tx tx; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    zcl_transparent_tx previous;
    memcpy(&previous, &box.tx, sizeof(previous));
    const zcl_status status = zcl_transaction_parse(data, size, &box.tx);
    if (box.before != UINT64_C(0xa5a5a5a5a5a5a5a5) || box.after != box.before) abort();
    if (status != ZCL_OK) {
        if (memcmp(&previous, &box.tx, sizeof(previous)) != 0) abort();
        return;
    }
    uint8_t encoded[ZCL_TX_WIRE_MAX], id[32];
    size_t length = SIZE_MAX;
    if (zcl_transaction_serialize(&box.tx, encoded, sizeof(encoded), &length) != ZCL_OK) abort();
    if (length != size || memcmp(encoded, data, size) != 0) abort();
    if (zcl_transaction_id(&box.tx, id, sizeof(id)) != ZCL_OK) abort();
    matching_prevout(&box.tx, data, size, id);
}

static void serialize_object(const zcl_transparent_tx *tx, size_t capacity)
{
    uint8_t wire[ZCL_TX_WIRE_MAX + 2], previous[sizeof(wire)];
    memset(wire, 0xa5, sizeof(wire));
    memcpy(previous, wire, sizeof(wire));
    zcl_transparent_tx old;
    memcpy(&old, tx, sizeof(old));
    size_t length = SIZE_MAX;
    const zcl_status status = zcl_transaction_serialize(tx, wire + 1, capacity, &length);
    if (memcmp(&old, tx, sizeof(old)) != 0) abort();
    if (wire[0] != 0xa5 || wire[sizeof(wire) - 1] != 0xa5) abort();
    if (status != ZCL_OK) {
        if (length != SIZE_MAX || memcmp(previous, wire, sizeof(wire)) != 0) abort();
        return;
    }
    if (length > capacity || length > ZCL_TX_WIRE_MAX) abort();
    if (memcmp(wire + length + 1, previous + length + 1, sizeof(wire) - length - 1) != 0) abort();
    parse_wire(wire + 1, length);
}

static void object_from_bytes(const uint8_t *data, size_t size)
{
    if (size < 2) return;
    /* Unsigned integer and byte fields only; no bool, enum or pointer trap
     * representations. Initialize all storage before a bounded mutation copy. */
    zcl_transparent_tx tx;
    memset(&tx, 0, sizeof(tx));
    memcpy(&tx, data, size < sizeof(tx) ? size : sizeof(tx));
    serialize_object(&tx, ZCL_TX_WIRE_MAX);
    tx.input_count = data[0] % 10;
    tx.output_count = data[1] % 18;
    tx.expiry_height %= ZCL_TX_EXPIRY_LIMIT + 2;
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i)
        tx.inputs[i].script_len %= ZCL_TX_INPUT_SCRIPT_MAX + 3;
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        tx.outputs[i].script_len %= ZCL_TX_OUTPUT_SCRIPT_MAX + 3;
        tx.outputs[i].value %= ZCL_MAX_MONEY + 2;
    }
    serialize_object(&tx, ZCL_TX_WIRE_MAX);
    serialize_object(&tx, size % (ZCL_TX_WIRE_MAX + 1));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    script_bytes(data, size);
    parse_wire(data, size);
    object_from_bytes(data, size);
    return 0;
}
