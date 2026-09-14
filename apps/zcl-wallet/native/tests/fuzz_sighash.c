/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_sighash.h"
#ifdef ZCL_SIGHASH_ORACLE
#include "sighash_oracle.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Sighash fuzz check failed at %d\n", __LINE__); abort(); } } while (0)
static zcl_transparent_tx tx, saved;
static uint8_t script[129], saved_script[129], wire[ZCL_TX_WIRE_MAX];

static uint64_t integer(const uint8_t *data, size_t length, size_t offset, size_t width)
{
    CHECK(length >= 32 && length <= 4096 && width <= 8 && offset <= 4096);
    uint64_t result = 0;
    for (size_t i = 0; i < width; ++i) result |= (uint64_t)data[(offset + i) % length] << (8 * i);
    return result;
}

static void invalidate(uint8_t choice)
{
    switch (choice % 8) {
        case 0: tx.input_count = SIZE_MAX; break;
        case 1: tx.output_count = SIZE_MAX; break;
        case 2: tx.inputs[0].script_len = SIZE_MAX; break;
        case 3: tx.outputs[0].value = UINT64_MAX; break;
        case 4: tx.inputs[1] = tx.inputs[0]; break;
        case 5: tx.expiry_height = ZCL_TX_EXPIRY_LIMIT; break;
        case 6:
            memset(tx.inputs[0].previous_txid, 0, sizeof(tx.inputs[0].previous_txid));
            tx.inputs[0].previous_index = UINT32_MAX;
            break;
        case 7: tx.outputs[0].script_len = SIZE_MAX; break;
    }
}

static void fill_inputs(const uint8_t *data, size_t size, bool maximum)
{
    for (size_t i = 0; i < ZCL_TX_INPUT_MAX; ++i) {
        zcl_tx_input *input = &tx.inputs[i];
        for (size_t j = 0; j < 32; ++j) input->previous_txid[j] = data[(i * 32 + j) % size];
        input->previous_index = (data[18] & 1) != 0 ? UINT32_MAX - (uint32_t)i : (uint32_t)i;
        input->sequence = (uint32_t)integer(data, size, i * 9, 4);
        input->script_len = maximum ? 128 : (size_t)(data[(i + 21) % size] % 130);
        for (size_t j = 0; j < sizeof(input->script); ++j) input->script[j] = data[(i * 128 + j) % size];
    }
}

static void fill_outputs(const uint8_t *data, size_t size, bool maximum)
{
    uint64_t remaining = ZCL_MAX_MONEY;
    for (size_t i = 0; i < ZCL_TX_OUTPUT_MAX; ++i) {
        zcl_tx_output *output = &tx.outputs[i];
        output->value = integer(data, size, i * 8, 8) % (remaining + 1);
        remaining -= output->value;
        output->script_len = maximum ? 25 : (size_t)(data[(i + 12) % size] % 27);
        for (size_t j = 0; j < sizeof(output->script); ++j) output->script[j] = data[(i * 25 + j) % size];
    }
}

static void prepare(const uint8_t *data, size_t size, bool maximum)
{
    memset(&tx, 0, sizeof(tx));
    tx.input_count = maximum ? ZCL_TX_INPUT_MAX : (size_t)(data[0] % 10);
    tx.output_count = maximum ? ZCL_TX_OUTPUT_MAX : (size_t)(data[1] % 18);
    tx.lock_time = (uint32_t)integer(data, size, 2, 4);
    tx.expiry_height = (uint32_t)integer(data, size, 6, 4) % ZCL_TX_EXPIRY_LIMIT;
    fill_inputs(data, size, maximum);
    fill_outputs(data, size, maximum);
    if (data[19] == 255) invalidate(data[20]);
    for (size_t i = 0; i < sizeof(script); ++i) script[i] = data[(i + 21) % size];
    memcpy(&saved, &tx, sizeof(saved));
    memcpy(saved_script, script, sizeof(saved_script));
}

static zcl_status expected_status(size_t input_index, size_t script_length,
    uint64_t amount, size_t capacity, size_t *wire_length)
{
    if (script_length > 128 || amount > ZCL_MAX_MONEY) return ZCL_OUT_OF_RANGE;
    if (capacity < 32) return ZCL_BUFFER_TOO_SMALL;
    const zcl_status status = zcl_transaction_serialize(&tx, wire, sizeof(wire), wire_length);
    if (status != ZCL_OK) return status;
    return input_index < tx.input_count ? ZCL_OK : ZCL_OUT_OF_RANGE;
}

static void filled(const uint8_t *bytes, size_t length, uint8_t value)
{
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

static void check_hash(size_t input_index, size_t script_length, uint64_t amount,
                       uint32_t branch, size_t capacity)
{
    struct { uint8_t before[8], bytes[64], after[8]; } output;
    memset(&output, 0xa5, sizeof(output));
    size_t wire_length = 0;
    const zcl_status expected = expected_status(input_index, script_length, amount, capacity, &wire_length);
    const zcl_status actual = zcl_transaction_sighash_all(&tx, input_index, script, script_length,
        amount, branch, output.bytes, capacity);
    CHECK(actual == expected);
    CHECK(memcmp(&tx, &saved, sizeof(tx)) == 0 && memcmp(script, saved_script, sizeof(script)) == 0);
    filled(output.before, sizeof(output.before), 0xa5);
    filled(output.after, sizeof(output.after), 0xa5);
    filled(output.bytes + 32, 32, 0xa5);
    if (actual != ZCL_OK) filled(output.bytes, 32, 0xa5);
#ifdef ZCL_SIGHASH_ORACLE
    else {
        uint8_t oracle[32];
        zcl_test_sighash_all(wire, wire_length, input_index, script, script_length, amount, branch,
            oracle, sizeof(oracle));
        CHECK(memcmp(output.bytes, oracle, sizeof(oracle)) == 0);
    }
#endif
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 32 || size > 4096) return 0;
    const bool maximum = (data[17] & 1) != 0;
    prepare(data, size, maximum);
    const size_t input_index = maximum ? 7 : (size_t)(data[12] % 10);
    const size_t script_length = maximum ? 128 : (size_t)(data[13] % 130);
    uint64_t amount = integer(data, size, 21, 8) % (ZCL_MAX_MONEY + 1);
    if ((data[15] & 15) == 15) amount = UINT64_MAX;
    check_hash(input_index, script_length, amount, (uint32_t)integer(data, size, 28, 4),
        (size_t)(data[14] % 65));
    return 0;
}
