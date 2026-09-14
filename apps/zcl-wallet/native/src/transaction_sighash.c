/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_sighash.h"
#include "transaction_internal.h"
#include "blake2_hash.h"
#include "zcl_keys.h"
#include <string.h>

/* Largest component: 16 outputs * (8 value + 1 length + 25 script) bytes.
 * The final transparent preimage is at most 220+36+1+128+8+4 = 397 bytes. */
#define HASH_BYTES ((size_t)544)
_Static_assert(ZCL_TX_OUTPUT_MAX * (9 + ZCL_TX_OUTPUT_SCRIPT_MAX) <= HASH_BYTES,
               "Output hash buffer must cover the complete local transaction profile");
_Static_assert(ZCL_TX_INPUT_MAX * 36 <= HASH_BYTES &&
               269 + ZCL_TX_INPUT_SCRIPT_MAX <= HASH_BYTES && HASH_BYTES <= ZCL_BLAKE2_INPUT_MAX,
               "Every preimage must fit both the writer and the hash provider");

typedef struct { uint8_t bytes[HASH_BYTES]; size_t used; bool failed; } hash_writer;
typedef struct {
    hash_writer writer;
    uint8_t components[6][32], personal[16], result[32];
} hash_work;

static void put_bytes(hash_writer *writer, const uint8_t *bytes, size_t length)
{
    if (writer->failed) return;
    if (length > sizeof(writer->bytes) - writer->used) { writer->failed = true; return; }
    if (length != 0) memcpy(writer->bytes + writer->used, bytes, length);
    writer->used += length;
}

static void put_integer(hash_writer *writer, uint64_t value, size_t width)
{
    uint8_t bytes[8] = {0};
    if (width != 1 && width != 4 && width != 8) { writer->failed = true; return; }
    for (size_t i = 0; i < width; ++i) bytes[i] = (uint8_t)(value >> (8 * i));
    put_bytes(writer, bytes, width);
}

static void put_outpoint(hash_writer *writer, const zcl_tx_input *input)
{
    uint8_t hash[32] = {0};
    for (size_t i = 0; i < sizeof(hash); ++i) hash[i] = input->previous_txid[31 - i];
    put_bytes(writer, hash, sizeof(hash));
    put_integer(writer, input->previous_index, 4);
}

static zcl_status hash_current(hash_writer *writer, const uint8_t *personal, uint8_t *digest)
{
    if (writer->failed) return ZCL_RESOURCE_EXHAUSTED;
    return zcl_blake2b256(writer->bytes, writer->used, personal, 16, digest, 32);
}

static zcl_status input_components(const zcl_transparent_tx *tx, hash_work *work)
{
    static const uint8_t prevouts[16] = "ZcashPrevoutHash", sequences[16] = "ZcashSequencHash";
    for (size_t i = 0; i < tx->input_count; ++i) put_outpoint(&work->writer, &tx->inputs[i]);
    zcl_status status = hash_current(&work->writer, prevouts, work->components[0]);
    if (status != ZCL_OK) return status;
    memset(&work->writer, 0, sizeof(work->writer));
    for (size_t i = 0; i < tx->input_count; ++i) put_integer(&work->writer, tx->inputs[i].sequence, 4);
    return hash_current(&work->writer, sequences, work->components[1]);
}

static zcl_status output_component(const zcl_transparent_tx *tx, hash_work *work)
{
    static const uint8_t outputs[16] = "ZcashOutputsHash";
    memset(&work->writer, 0, sizeof(work->writer));
    for (size_t i = 0; i < tx->output_count; ++i) {
        const zcl_tx_output *output = &tx->outputs[i];
        put_integer(&work->writer, output->value, 8);
        put_integer(&work->writer, output->script_len, 1);
        put_bytes(&work->writer, output->script, output->script_len);
    }
    return hash_current(&work->writer, outputs, work->components[2]);
}

static zcl_status final_hash(const zcl_transparent_tx *tx, size_t input_index,
    const uint8_t *script, size_t script_length, uint64_t amount, uint32_t branch, hash_work *work)
{
    memcpy(work->personal, "ZcashSigHash", 12);
    for (size_t i = 0; i < 4; ++i) work->personal[12 + i] = (uint8_t)(branch >> (8 * i));
    memset(&work->writer, 0, sizeof(work->writer));
    put_integer(&work->writer, ZCL_TX_HEADER, 4);
    put_integer(&work->writer, ZCL_TX_VERSION_GROUP, 4);
    put_bytes(&work->writer, (const uint8_t *)work->components, sizeof(work->components));
    put_integer(&work->writer, tx->lock_time, 4);
    put_integer(&work->writer, tx->expiry_height, 4);
    put_integer(&work->writer, 0, 8); /* Transparent-only valueBalance. */
    put_integer(&work->writer, 1, 4); /* SIGHASH_ALL, without ANYONECANPAY. */
    put_outpoint(&work->writer, &tx->inputs[input_index]);
    put_integer(&work->writer, script_length, 1);
    put_bytes(&work->writer, script, script_length);
    put_integer(&work->writer, amount, 8);
    put_integer(&work->writer, tx->inputs[input_index].sequence, 4);
    return hash_current(&work->writer, work->personal, work->result);
}

static zcl_status validate_inputs(const zcl_transparent_tx *tx, size_t input_index,
    const uint8_t *script, size_t script_length, uint64_t amount, const uint8_t *digest, size_t capacity)
{
    if (tx == NULL || script == NULL || digest == NULL) return ZCL_INVALID_ARGUMENT;
    if (script_length > ZCL_TX_INPUT_SCRIPT_MAX || amount > ZCL_MAX_MONEY) return ZCL_OUT_OF_RANGE;
    if (capacity < 32) return ZCL_BUFFER_TOO_SMALL;
    size_t wire_length = 0;
    const zcl_status status = zcl_transaction_check(tx, &wire_length);
    if (status != ZCL_OK) return status;
    return input_index < tx->input_count ? ZCL_OK : ZCL_OUT_OF_RANGE;
}

zcl_status zcl_transaction_sighash_all(const zcl_transparent_tx *transaction,
    size_t input_index, const uint8_t *script_code, size_t script_length,
    uint64_t amount, uint32_t branch, uint8_t *digest, size_t capacity)
{
    zcl_status status = validate_inputs(transaction, input_index, script_code, script_length,
                                        amount, digest, capacity);
    if (status != ZCL_OK) return status;
    hash_work work;
    memset(&work, 0, sizeof(work));
    status = input_components(transaction, &work);
    if (status == ZCL_OK) status = output_component(transaction, &work);
    if (status == ZCL_OK)
        status = final_hash(transaction, input_index, script_code, script_length, amount, branch, &work);
    if (status == ZCL_OK) memcpy(digest, work.result, sizeof(work.result));
    zcl_secure_zero(&work, sizeof(work));
    return status;
}
