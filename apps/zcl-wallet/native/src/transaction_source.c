/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_source_internal.h"
#include "transaction_internal.h"
#include "zcl_keys.h"
#include <mbedtls/sha256.h>
#include <string.h>

typedef struct {
    const uint8_t *wire;
    size_t length, used;
    zcl_status status;
} source_reader;

static const uint8_t *take(source_reader *reader, size_t length)
{
    if (reader->status != ZCL_OK) return NULL;
    if (length > reader->length - reader->used) {
        reader->status = ZCL_INVALID_ENCODING;
        return NULL;
    }
    const uint8_t *result = reader->wire + reader->used;
    reader->used += length;
    return result;
}

static uint64_t integer(source_reader *reader, size_t width)
{
    const uint8_t *bytes = take(reader, width);
    if (bytes == NULL) return 0;
    uint64_t result = 0;
    for (size_t i = 0; i < width; ++i) result |= (uint64_t)bytes[i] << (8 * i);
    return result;
}

static size_t count(source_reader *reader, size_t minimum_bytes)
{
    const uint64_t prefix = integer(reader, 1);
    uint64_t result = prefix, minimum = 0;
    if (prefix == 253) { result = integer(reader, 2); minimum = 253; }
    if (prefix == 254) { result = integer(reader, 4); minimum = UINT64_C(65536); }
    if (prefix == 255) { result = integer(reader, 8); minimum = UINT64_C(4294967296); }
    if (reader->status != ZCL_OK) return 0;
    if (result < minimum || result > (reader->length - reader->used) / minimum_bytes) {
        reader->status = ZCL_INVALID_ENCODING;
        return 0;
    }
    return (size_t)result; /* Proven <=102000 before conversion/multiplication. */
}

static void inputs(source_reader *reader, zcl_v4_source *view)
{
    view->input_count = count(reader, 41);
    for (size_t i = 0; i < view->input_count && reader->status == ZCL_OK; ++i) {
        (void)take(reader, 36);
        const size_t length = count(reader, 1);
        (void)take(reader, length);
        (void)integer(reader, 4);
    }
}

static void outputs(source_reader *reader, uint32_t selected, zcl_v4_source *view)
{
    view->output_count = count(reader, 9);
    uint64_t total = 0;
    for (size_t i = 0; i < view->output_count && reader->status == ZCL_OK; ++i) {
        const uint64_t value = integer(reader, 8);
        const size_t length = count(reader, 1);
        const uint8_t *script = take(reader, length);
        if (reader->status != ZCL_OK) return;
        if (value > ZCL_MAX_MONEY - total) { reader->status = ZCL_OUT_OF_RANGE; return; }
        total += value;
        if (i != selected) continue;
        if (length > sizeof(view->output.script)) { reader->status = ZCL_UNSUPPORTED; return; }
        view->output.value = value;
        view->output.script_len = length;
        memcpy(view->output.script, script, length);
    }
}

static size_t descriptions(source_reader *reader, size_t width)
{
    const size_t amount = count(reader, width);
    (void)take(reader, amount * width); /* Bounded by remaining input above. */
    return amount;
}

static void shielded(source_reader *reader, zcl_v4_source *view)
{
    view->lock_time = (uint32_t)integer(reader, 4);
    view->expiry_height = (uint32_t)integer(reader, 4);
    const uint64_t balance = integer(reader, 8);
    view->value_balance = balance <= INT64_MAX ? (int64_t)balance : -1 - (int64_t)(~balance);
    view->spend_count = descriptions(reader, 384);
    view->shielded_count = descriptions(reader, 948);
    view->joinsplit_count = descriptions(reader, 1698);
    if (view->joinsplit_count != 0) (void)take(reader, 96);
    if (view->spend_count != 0 || view->shielded_count != 0) (void)take(reader, 64);
}

static zcl_status source_layout(const uint8_t *wire, size_t length, uint32_t selected,
    zcl_v4_source *view)
{
    source_reader reader = {wire, length, 0, ZCL_OK};
    const uint64_t header = integer(&reader, 4), group = integer(&reader, 4);
    if (reader.status != ZCL_OK) return reader.status;
    if (header != ZCL_TX_HEADER || group != ZCL_TX_VERSION_GROUP) return ZCL_UNSUPPORTED;
    inputs(&reader, view);
    outputs(&reader, selected, view);
    shielded(&reader, view);
    if (reader.status != ZCL_OK) return reader.status;
    if (reader.used != length) return ZCL_INVALID_ENCODING;
    return selected < view->output_count ? ZCL_OK : ZCL_OUT_OF_RANGE;
}

static zcl_status source_id(const uint8_t *wire, size_t length, uint8_t output[32])
{
    uint8_t first[32] = {0}, second[32] = {0};
    zcl_status status = ZCL_CRYPTO_FAILURE;
    if (mbedtls_sha256(wire, length, first, 0) != 0) goto cleanup;
    if (mbedtls_sha256(first, sizeof(first), second, 0) != 0) goto cleanup;
    for (size_t i = 0; i < 32; ++i) output[i] = second[31 - i];
    status = ZCL_OK;
cleanup:
    zcl_secure_zero(first, sizeof(first));
    zcl_secure_zero(second, sizeof(second));
    return status;
}

zcl_status zcl_v4_source_inspect(const uint8_t *wire, size_t length,
    uint32_t output_index, zcl_v4_source *output)
{
    if (wire == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    if (length > ZCL_V4_SOURCE_MAX) return ZCL_RESOURCE_EXHAUSTED;
    zcl_v4_source candidate = {0};
    zcl_status status = source_layout(wire, length, output_index, &candidate);
    if (status == ZCL_OK) status = source_id(wire, length, candidate.transaction_id);
    if (status == ZCL_OK) *output = candidate;
    zcl_secure_zero(&candidate, sizeof(candidate));
    return status;
}

zcl_status zcl_v4_source_prevout(const zcl_tx_input *input,
    const uint8_t *wire, size_t length, zcl_tx_output *output)
{
    if (input == NULL || output == NULL) return ZCL_INVALID_ARGUMENT;
    zcl_v4_source candidate = {0};
    zcl_status status = zcl_v4_source_inspect(wire, length, input->previous_index, &candidate);
    if (status == ZCL_OK && memcmp(candidate.transaction_id, input->previous_txid, 32) != 0)
        status = ZCL_INVALID_ENCODING;
    if (status == ZCL_OK) *output = candidate.output;
    zcl_secure_zero(&candidate, sizeof(candidate));
    return status;
}
