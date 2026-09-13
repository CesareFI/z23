/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_internal.h"
#include <string.h>

typedef struct {
    const uint8_t *bytes;
    size_t length;
    size_t used;
    zcl_status status;
} tx_reader;

/* All callers use widths 1,2,4,8. Failure is sticky and never advances. */
static uint64_t read_integer(tx_reader *reader, size_t width)
{
    if (reader->status != ZCL_OK) return 0;
    if (width > reader->length - reader->used) {
        reader->status = ZCL_INVALID_ENCODING;
        return 0;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < width; ++i)
        value |= (uint64_t)reader->bytes[reader->used + i] << (8 * i);
    reader->used += width;
    return value;
}

static size_t read_count(tx_reader *reader, size_t limit)
{
    const uint64_t prefix = read_integer(reader, 1);
    uint64_t value = prefix;
    uint64_t minimum = 0;
    if (prefix == 253) { value = read_integer(reader, 2); minimum = 253; }
    if (prefix == 254) { value = read_integer(reader, 4); minimum = UINT64_C(65536); }
    if (prefix == 255) { value = read_integer(reader, 8); minimum = UINT64_C(4294967296); }
    if (reader->status != ZCL_OK) return 0;
    if (value < minimum) reader->status = ZCL_INVALID_ENCODING;
    else if (value > limit) reader->status = ZCL_RESOURCE_EXHAUSTED;
    else return (size_t)value; /* <= fixed capacity before conversion. */
    return 0;
}

static void read_bytes(tx_reader *reader, uint8_t *out, size_t length)
{
    if (reader->status != ZCL_OK) return;
    if (length > reader->length - reader->used) {
        reader->status = ZCL_INVALID_ENCODING;
        return;
    }
    if (length != 0) memcpy(out, reader->bytes + reader->used, length);
    reader->used += length;
}

static void read_input(tx_reader *reader, zcl_tx_input *input)
{
    uint8_t hash[32] = {0};
    read_bytes(reader, hash, sizeof(hash));
    for (size_t i = 0; i < sizeof(hash); ++i)
        input->previous_txid[i] = hash[sizeof(hash) - i - 1];
    input->previous_index = (uint32_t)read_integer(reader, 4);
    input->script_len = read_count(reader, sizeof(input->script));
    read_bytes(reader, input->script, input->script_len);
    input->sequence = (uint32_t)read_integer(reader, 4);
}

static void read_vectors(tx_reader *reader, zcl_transparent_tx *tx)
{
    tx->input_count = read_count(reader, ZCL_TX_INPUT_MAX);
    for (size_t i = 0; i < tx->input_count; ++i) read_input(reader, &tx->inputs[i]);
    tx->output_count = read_count(reader, ZCL_TX_OUTPUT_MAX);
    for (size_t i = 0; i < tx->output_count; ++i) {
        zcl_tx_output *output = &tx->outputs[i];
        output->value = read_integer(reader, 8);
        output->script_len = read_count(reader, sizeof(output->script));
        read_bytes(reader, output->script, output->script_len);
    }
}

static zcl_status read_tail(tx_reader *reader, zcl_transparent_tx *tx)
{
    tx->lock_time = (uint32_t)read_integer(reader, 4);
    tx->expiry_height = (uint32_t)read_integer(reader, 4);
    const uint64_t value_balance = read_integer(reader, 8);
    if (reader->status != ZCL_OK) return reader->status;
    if (value_balance != 0) return ZCL_UNSUPPORTED;
    /* Three empty vectors; no JoinSplit or binding signature follows. */
    for (size_t i = 0; i < 3; ++i) {
        const uint64_t count = read_integer(reader, 1);
        if (reader->status != ZCL_OK) return reader->status;
        if (count != 0) return ZCL_UNSUPPORTED;
    }
    return reader->used == reader->length ? ZCL_OK : ZCL_INVALID_ENCODING;
}

zcl_status zcl_transaction_parse(const uint8_t *wire, size_t length,
                                 zcl_transparent_tx *transaction)
{
    if (wire == NULL || transaction == NULL) return ZCL_INVALID_ARGUMENT;
    if (length > ZCL_TX_WIRE_MAX) return ZCL_RESOURCE_EXHAUSTED;
    tx_reader reader = {wire, length, 0, ZCL_OK};
    const uint64_t header = read_integer(&reader, 4);
    const uint64_t group = read_integer(&reader, 4);
    if (reader.status != ZCL_OK) return reader.status;
    if (header != ZCL_TX_HEADER || group != ZCL_TX_VERSION_GROUP) return ZCL_UNSUPPORTED;
    zcl_transparent_tx candidate = {0};
    read_vectors(&reader, &candidate);
    zcl_status status = read_tail(&reader, &candidate);
    if (status != ZCL_OK) return status;
    size_t expected = 0;
    status = zcl_transaction_check(&candidate, &expected);
    if (status != ZCL_OK) return status;
    if (expected != length) return ZCL_INVALID_ENCODING;
    *transaction = candidate;
    return ZCL_OK;
}
