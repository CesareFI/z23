/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_internal.h"
#include "zcl_keys.h"
#include <string.h>

typedef struct {
    uint8_t bytes[ZCL_TX_WIRE_MAX];
    size_t used;
    bool failed;
} tx_writer;

static void write_integer(tx_writer *writer, uint64_t value, size_t width)
{
    if (writer->failed) return;
    if (width > sizeof(writer->bytes) - writer->used) { writer->failed = true; return; }
    for (size_t i = 0; i < width; ++i)
        writer->bytes[writer->used + i] = (uint8_t)((value >> (8 * i)) & UINT64_C(255));
    writer->used += width;
}

static void write_bytes(tx_writer *writer, const uint8_t *bytes, size_t length)
{
    if (writer->failed) return;
    if (length > sizeof(writer->bytes) - writer->used) { writer->failed = true; return; }
    if (length != 0) memcpy(writer->bytes + writer->used, bytes, length);
    writer->used += length;
}

static void write_input(tx_writer *writer, const zcl_tx_input *input)
{
    uint8_t hash[32] = {0};
    for (size_t i = 0; i < sizeof(hash); ++i)
        hash[i] = input->previous_txid[sizeof(hash) - i - 1];
    write_bytes(writer, hash, sizeof(hash));
    zcl_secure_zero(hash, sizeof(hash));
    write_integer(writer, input->previous_index, 4);
    write_integer(writer, input->script_len, 1);
    write_bytes(writer, input->script, input->script_len);
    write_integer(writer, input->sequence, 4);
}

static void write_transaction(tx_writer *writer, const zcl_transparent_tx *tx)
{
    write_integer(writer, ZCL_TX_HEADER, 4);
    write_integer(writer, ZCL_TX_VERSION_GROUP, 4);
    write_integer(writer, tx->input_count, 1);
    for (size_t i = 0; i < tx->input_count; ++i) write_input(writer, &tx->inputs[i]);
    write_integer(writer, tx->output_count, 1);
    for (size_t i = 0; i < tx->output_count; ++i) {
        const zcl_tx_output *output = &tx->outputs[i];
        write_integer(writer, output->value, 8);
        write_integer(writer, output->script_len, 1);
        write_bytes(writer, output->script, output->script_len);
    }
    write_integer(writer, tx->lock_time, 4);
    write_integer(writer, tx->expiry_height, 4);
    write_integer(writer, 0, 8); /* valueBalance */
    write_integer(writer, 0, 3); /* Empty shielded spend/output and JoinSplit vectors. */
}

zcl_status zcl_transaction_serialize(const zcl_transparent_tx *transaction,
                                     uint8_t *wire, size_t capacity, size_t *length)
{
    if (wire == NULL || length == NULL) return ZCL_INVALID_ARGUMENT;
    size_t expected = 0;
    zcl_status status = zcl_transaction_check(transaction, &expected);
    if (status != ZCL_OK) return status;
    if (capacity < expected) return ZCL_BUFFER_TOO_SMALL;
    tx_writer writer = {{0}, 0, false};
    write_transaction(&writer, transaction);
    if (writer.failed || writer.used != expected) status = ZCL_INVALID_ENCODING;
    else {
        memcpy(wire, writer.bytes, writer.used);
        *length = writer.used;
    }
    zcl_secure_zero(writer.bytes, sizeof(writer.bytes));
    return status;
}
