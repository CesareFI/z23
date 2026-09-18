/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_source_internal.h"
#include <stdlib.h>
#include <string.h>
#include "source_vectors.h"
#ifdef ZCL_MIXED_SOURCE_TEST
#include "legacy_source_vectors.h"
#define zcl_v4_source_inspect zcl_source_inspect
#define zcl_v4_source_assess zcl_source_assess
#endif
#ifdef ZCL_SOURCE_ORACLE
#include <openssl/evp.h>
#endif
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
#define CHECK(v) do { if (!(v)) abort(); } while (0)
static uint8_t changed[11001];
static zcl_transparent_tx spending;

static void identity(const uint8_t *wire, size_t size, const zcl_source_view *view)
{
#ifdef ZCL_SOURCE_ORACLE
    uint8_t first[32], second[32]; unsigned length = 0;
    CHECK(EVP_Digest(wire, size, first, &length, EVP_sha256(), NULL) == 1 && length == 32);
    CHECK(EVP_Digest(first, sizeof(first), second, &length, EVP_sha256(), NULL) == 1 && length == 32);
    for (size_t i = 0; i < 32; ++i) CHECK(view->transaction_id[i] == second[31 - i]);
#else
    (void)wire; (void)size; (void)view;
#endif
}

static void assess(const uint8_t *wire, size_t size, uint32_t index, const zcl_v4_source *source)
{
    memset(&spending, 0, sizeof(spending));
    spending.input_count = spending.output_count = 1;
    memcpy(spending.inputs[0].previous_txid, source->transaction_id, 32);
    spending.inputs[0].previous_index = index;
    spending.outputs[0] = source->output;
    const zcl_previous_transaction previous = {wire, size};
    zcl_address destination;
    const zcl_status expected = zcl_address_from_script(source->output.script,
        source->output.script_len, ZCL_MAINNET, &destination);
    struct { uint64_t before; zcl_transaction_assessment value; uint64_t after; } box, original;
    memset(&box, 0xa5, sizeof(box)); memcpy(&original, &box, sizeof(box));
    CHECK(zcl_v4_source_assess(&spending, ZCL_MAINNET, &previous, 1, 0, &box.value) == expected);
    CHECK(box.before == original.before && box.after == original.after);
    if (expected != ZCL_OK) { CHECK(memcmp(&box, &original, sizeof(box)) == 0); return; }
    CHECK(box.value.fee == 0 && box.value.input_total == source->output.value);
    CHECK(box.value.output_total == source->output.value && box.value.input_count == 1 && box.value.output_count == 1);
    CHECK(memcmp(&box.value.inputs[0].destination, &destination, sizeof(destination)) == 0);
    spending.inputs[0].previous_txid[0] ^= 1;
    memcpy(&original, &box, sizeof(box));
    CHECK(zcl_v4_source_assess(&spending, ZCL_MAINNET, &previous, 1, 0, &box.value) == ZCL_INVALID_ENCODING);
    CHECK(memcmp(&box, &original, sizeof(box)) == 0);
}

static void inspect(const uint8_t *data, size_t size, uint32_t index)
{
    struct { uint64_t before; zcl_v4_source view; uint64_t after; } output, original;
    memset(&output, 0xa5, sizeof(output)); memcpy(&original, &output, sizeof(output));
    const zcl_status status = zcl_v4_source_inspect(data, size, index, &output.view);
    CHECK(output.before == original.before && output.after == original.after);
    if (status != ZCL_OK) { CHECK(memcmp(&output, &original, sizeof(output)) == 0); return; }
    CHECK(output.view.output_count > index && output.view.output_count <= size / 9);
    CHECK(output.view.input_count <= size / 41 && output.view.spend_count <= size / 384);
    CHECK(output.view.shielded_count <= size / 948 && output.view.joinsplit_count <= size / 1698);
    CHECK(output.view.output.value <= ZCL_MAX_MONEY && output.view.output.script_len <= 25);
    identity(data, size, &output.view);
    assess(data, size, index, &output.view);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4 || size > ZCL_V4_SOURCE_MAX + 4) return 0;
    const uint32_t index = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    inspect(data + 4, size - 4, index);
    const size_t vector = data[0] % 4;
    memcpy(changed, source_vectors[vector].wire, source_vectors[vector].length);
    for (size_t i = 4; i < size; ++i) changed[(i - 4) % source_vectors[vector].length] ^= data[i];
    inspect(changed, source_vectors[vector].length, data[1] % 5);
#ifdef ZCL_MIXED_SOURCE_TEST
    const size_t row = data[2] % (sizeof(legacy_vectors) / sizeof(legacy_vectors[0]));
    memcpy(changed, legacy_vectors[row].wire, legacy_vectors[row].length);
    for (size_t i = 4; i < size; ++i) changed[(i - 4) % legacy_vectors[row].length] ^= data[i];
    inspect(changed, legacy_vectors[row].length, data[3] % 5);
#endif
    return 0;
}
