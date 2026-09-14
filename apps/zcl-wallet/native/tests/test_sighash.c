/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_sighash.h"
#include "transaction_fixture.h"
#include "sighash_vectors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ZCL_SIGHASH_ORACLE
#include "sighash_oracle.h"
#endif

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Public sighash check failed at %d\n", __LINE__); abort(); } } while (0)
/* Fixed serial public fixture objects; no app or wallet global state. */
static zcl_transparent_tx fixtures[3], working, saved;
static uint8_t saved_script[128];
typedef struct { uint8_t before[8], bytes[64], after[8]; } digest_box;

static void filled(const void *buffer, size_t length, uint8_t value)
{
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

static size_t unhex(const char *hex, uint8_t *bytes, size_t capacity)
{
    static const char digits[] = "0123456789abcdef";
    const size_t length = strlen(hex);
    CHECK(length % 2 == 0 && length / 2 <= capacity);
    for (size_t i = 0; i < length / 2; ++i) {
        const char *a = strchr(digits, hex[2 * i]), *b = strchr(digits, hex[2 * i + 1]);
        CHECK(a != NULL && b != NULL);
        bytes[i] = (uint8_t)((a - digits) * 16 + (b - digits));
    }
    return length / 2;
}

static void initialize(void)
{
    CHECK(sizeof(transaction_vectors) / sizeof(transaction_vectors[0]) == 3);
    CHECK(sizeof(sighash_vectors) / sizeof(sighash_vectors[0]) == 144);
    uint8_t bytes[ZCL_TX_WIRE_MAX];
    for (size_t i = 0; i < 3; ++i) {
        const size_t length = unhex(transaction_vectors[i].hex, bytes, sizeof(bytes));
        CHECK(zcl_transaction_parse(bytes, length, &fixtures[i]) == ZCL_OK);
    }
}

static void compare(const zcl_transparent_tx *tx, size_t input_index, const uint8_t *script,
    size_t script_length, uint64_t amount, uint32_t branch, const uint8_t expected[32])
{
    CHECK(script_length <= sizeof(saved_script));
    memcpy(&saved, tx, sizeof(saved));
    memcpy(saved_script, script, script_length);
#ifdef ZCL_SIGHASH_ORACLE
    uint8_t wire[ZCL_TX_WIRE_MAX], oracle[32];
    size_t length = 0;
    CHECK(zcl_transaction_serialize(tx, wire, sizeof(wire), &length) == ZCL_OK);
    zcl_test_sighash_all(wire, length, input_index, script, script_length, amount, branch, oracle, sizeof(oracle));
    CHECK(memcmp(oracle, expected, sizeof(oracle)) == 0);
#endif
    for (size_t capacity = 0; capacity <= 64; ++capacity) {
        digest_box result;
        memset(&result, 0xa5, sizeof(result));
        const zcl_status status = zcl_transaction_sighash_all(tx, input_index, script, script_length,
            amount, branch, result.bytes, capacity);
        CHECK(status == (capacity < 32 ? ZCL_BUFFER_TOO_SMALL : ZCL_OK));
        if (capacity < 32) filled(&result, sizeof(result), 0xa5);
        else {
            CHECK(memcmp(result.bytes, expected, 32) == 0);
            filled(result.bytes + 32, 32, 0xa5);
        }
        filled(result.before, sizeof(result.before), 0xa5);
        filled(result.after, sizeof(result.after), 0xa5);
    }
    CHECK(memcmp(tx, &saved, sizeof(saved)) == 0);
    CHECK(memcmp(script, saved_script, script_length) == 0);
}

static void known_answers(void)
{
    uint8_t p2pkh[25] = {0x76, 0xa9, 0x14};
    for (size_t i = 0; i < 20; ++i) p2pkh[i + 3] = (uint8_t)i;
    p2pkh[23] = 0x88; p2pkh[24] = 0xac;
    for (size_t i = 0; i < sizeof(sighash_vectors) / sizeof(sighash_vectors[0]); ++i) {
        const size_t projection = sighash_vectors[i].projection;
        CHECK(projection < 3 && sighash_vectors[i].script_profile <= 1);
        const uint8_t *script = sighash_vectors[i].script_profile == 0 ?
            sighash_scripts[projection].bytes : p2pkh;
        const size_t length = sighash_vectors[i].script_profile == 0 ?
            sighash_scripts[projection].length : sizeof(p2pkh);
        compare(&fixtures[projection], sighash_vectors[i].input_index, script, length,
            sighash_vectors[i].amount, sighash_vectors[i].branch, sighash_vectors[i].digest);
    }
}

static void refuse(const zcl_transparent_tx *tx, size_t input_index, const uint8_t *script,
    size_t length, uint64_t amount, zcl_status expected)
{
    digest_box result;
    memset(&result, 0xa5, sizeof(result));
    CHECK(zcl_transaction_sighash_all(tx, input_index, script, length, amount, 0,
        result.bytes, sizeof(result.bytes)) == expected);
    filled(&result, sizeof(result), 0xa5);
}

static void refusals(void)
{
    uint8_t byte = 0;
    refuse(NULL, 0, &byte, 0, 0, ZCL_INVALID_ARGUMENT);
    refuse(&fixtures[0], 0, NULL, 0, 0, ZCL_INVALID_ARGUMENT);
    CHECK(zcl_transaction_sighash_all(&fixtures[0], 0, &byte, 0, 0, 0, NULL, 32) == ZCL_INVALID_ARGUMENT);
    refuse(&fixtures[0], SIZE_MAX, &byte, 0, 0, ZCL_OUT_OF_RANGE);
    refuse(&fixtures[0], fixtures[0].input_count, &byte, 0, 0, ZCL_OUT_OF_RANGE);
    refuse(&fixtures[0], 0, &byte, 129, 0, ZCL_OUT_OF_RANGE);
    refuse(&fixtures[0], 0, &byte, SIZE_MAX, 0, ZCL_OUT_OF_RANGE);
    refuse(&fixtures[0], 0, &byte, 0, ZCL_MAX_MONEY + 1, ZCL_OUT_OF_RANGE);
    refuse(&fixtures[0], 0, &byte, 0, UINT64_MAX, ZCL_OUT_OF_RANGE);
    working = fixtures[0]; working.input_count = SIZE_MAX;
    refuse(&working, 0, &byte, 0, 0, ZCL_RESOURCE_EXHAUSTED);
    working = fixtures[0]; working.output_count = SIZE_MAX;
    refuse(&working, 0, &byte, 0, 0, ZCL_RESOURCE_EXHAUSTED);
    working = fixtures[0]; working.inputs[0].script_len = SIZE_MAX;
    refuse(&working, 0, &byte, 0, 0, ZCL_RESOURCE_EXHAUSTED);
    working = fixtures[0]; working.outputs[0].value = ZCL_MAX_MONEY;
    refuse(&working, 0, &byte, 0, 0, ZCL_OUT_OF_RANGE);
    working = fixtures[0]; working.expiry_height = ZCL_TX_EXPIRY_LIMIT;
    refuse(&working, 0, &byte, 0, 0, ZCL_OUT_OF_RANGE);
    CHECK(byte == 0);
}

static void maximum_profile(void)
{
    memset(&working, 0, sizeof(working));
    working.input_count = ZCL_TX_INPUT_MAX; working.output_count = ZCL_TX_OUTPUT_MAX;
    working.lock_time = UINT32_MAX; working.expiry_height = ZCL_TX_EXPIRY_LIMIT - 1;
    for (size_t i = 0; i < working.input_count; ++i) {
        memset(working.inputs[i].previous_txid, 0xff, 32);
        working.inputs[i].previous_index = UINT32_MAX - (uint32_t)i;
        working.inputs[i].sequence = UINT32_MAX;
        working.inputs[i].script_len = 128;
        memset(working.inputs[i].script, (int)i, 128);
    }
    working.outputs[0].value = ZCL_MAX_MONEY;
    for (size_t i = 0; i < working.output_count; ++i) {
        working.outputs[i].script_len = 25;
        memset(working.outputs[i].script, (int)i, 25);
    }
    uint8_t script[128], digest[32];
    memset(script, 0xff, sizeof(script));
    CHECK(zcl_transaction_sighash_all(&working, 7, script, sizeof(script), ZCL_MAX_MONEY,
        UINT32_MAX, digest, sizeof(digest)) == ZCL_OK);
    /* All capacity/canary/input-copy checks apply at the 544-byte component
     * and 397-byte final-preimage limits; the optional oracle checks its value. */
    compare(&working, 7, script, sizeof(script), ZCL_MAX_MONEY, UINT32_MAX, digest);
    for (size_t i = 0; i < working.input_count; ++i) {
        working.inputs[i].script_len = 0;
        memset(working.inputs[i].script, 0xa6, sizeof(working.inputs[i].script));
    }
    /* v4 excludes every scriptSig, including bytes outside its active span. */
    compare(&working, 7, script, sizeof(script), ZCL_MAX_MONEY, UINT32_MAX, digest);
}

int main(void)
{
    initialize();
    known_answers();
    refusals();
    maximum_profile();
    CHECK(puts("144 independent public SIGHASH_ALL vectors, maximum profiles, all 65 capacities and refusals passed") >= 0);
    return 0;
}
