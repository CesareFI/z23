/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_sighash.h"
#include "blake2_hash.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Sighash failure check failed at %d\n", __LINE__); abort(); } } while (0)
/* Fake provider/zero names are remapped only in this separately built fixture. */
static unsigned failure, calls, wipes;
static zcl_transparent_tx transaction, saved;
static struct { const uint8_t *bytes; size_t length; bool cleared; } spans[9];
static size_t span_count;

static void filled(const uint8_t *bytes, size_t length, uint8_t value)
{
    CHECK(bytes != NULL);
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

static void observe(const uint8_t *bytes, size_t length)
{
    CHECK(bytes != NULL && length <= 544 && span_count < 9);
    spans[span_count].bytes = bytes;
    spans[span_count].length = length;
    spans[span_count].cleared = false;
    ++span_count;
}

zcl_status zcl_blake2b256(const uint8_t *input, size_t length, const uint8_t *personal,
    size_t personal_length, uint8_t *output, size_t capacity)
{
    static const uint8_t domains[4][16] = {"ZcashPrevoutHash", "ZcashSequencHash",
        "ZcashOutputsHash", {'Z','c','a','s','h','S','i','g','H','a','s','h',0x04,0x03,0x02,0x01}};
    const size_t lengths[4] = {36, 4, 9, 274}; /* Final script has five bytes. */
    CHECK(calls < 4 && wipes == 0 && personal != NULL);
    CHECK(length == lengths[calls] && personal_length == 16 && capacity == 32);
    CHECK(memcmp(personal, domains[calls], 16) == 0);
    filled(output, capacity, 0);
    observe(input, length);
    observe(output, capacity);
    if (calls == 3) observe(personal, personal_length); /* Invocation-owned final domain. */
    memset(output, 0x6a, capacity); /* Partial output is possible even on failure. */
    ++calls;
    return calls == failure ? ZCL_CRYPTO_FAILURE : ZCL_OK;
}

void zcl_secure_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL && length <= 4096 && wipes++ == 0);
    memset(buffer, 0, length);
    /* Captured work spans are inspected only while this cleanup callback has
     * the live caller object. Post-return assertions use flags, never pointers. */
    for (size_t i = 0; i < span_count; ++i) {
        filled(spans[i].bytes, spans[i].length, 0);
        spans[i].bytes = NULL;
        spans[i].cleared = true;
    }
}

static void run(unsigned ordinal)
{
    const uint8_t script[5] = {1, 2, 3, 4, 5};
    struct { uint8_t before[8], bytes[32], after[8]; } output;
    memset(&output, 0xa5, sizeof(output));
    memset(spans, 0, sizeof(spans));
    span_count = 0; calls = 0; wipes = 0; failure = ordinal;
    memcpy(&saved, &transaction, sizeof(saved));
    CHECK(zcl_transaction_sighash_all(&transaction, 0, script, sizeof(script), 1,
        UINT32_C(0x01020304), output.bytes, sizeof(output.bytes)) ==
        (ordinal == 0 ? ZCL_OK : ZCL_CRYPTO_FAILURE));
    CHECK(calls == (ordinal == 0 ? 4 : ordinal) && wipes == 1);
    for (size_t i = 0; i < span_count; ++i) CHECK(spans[i].cleared && spans[i].bytes == NULL);
    filled(output.before, sizeof(output.before), 0xa5);
    filled(output.after, sizeof(output.after), 0xa5);
    filled(output.bytes, sizeof(output.bytes), ordinal == 0 ? 0x6a : 0xa5);
    CHECK(memcmp(&transaction, &saved, sizeof(saved)) == 0);
    for (size_t i = 0; i < sizeof(script); ++i) CHECK(script[i] == i + 1);
}

int main(void)
{
    transaction.input_count = transaction.output_count = 1;
    transaction.inputs[0].previous_txid[0] = 1;
    transaction.inputs[0].sequence = UINT32_MAX;
    transaction.outputs[0].value = 1;
    for (unsigned ordinal = 0; ordinal <= 4; ++ordinal) run(ordinal);
    calls = 0; wipes = 0;
    uint8_t byte = 0, output[32];
    memset(output, 0xa5, sizeof(output));
    CHECK(zcl_transaction_sighash_all(&transaction, 0, &byte, SIZE_MAX, 0, 0,
        output, sizeof(output)) == ZCL_OUT_OF_RANGE);
    CHECK(calls == 0 && wipes == 0);
    filled(output, sizeof(output), 0xa5);
    CHECK(puts("All four hash failures stop publication and clear live work spans") >= 0);
    return 0;
}
