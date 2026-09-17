/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "transaction_source_internal.h"
#include <stdlib.h>
#include <string.h>
#include "legacy_source_vectors.h"
#ifdef ZCL_LEGACY_ORACLE
#include <openssl/evp.h>
#endif
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
static uint8_t changed[4000];
#define CHECK(v) do { if (!(v)) abort(); } while (0)

static void identity(const uint8_t *wire, size_t size, const zcl_source_view *view)
{
#ifdef ZCL_LEGACY_ORACLE
    uint8_t first[32], second[32]; unsigned length = 0;
    CHECK(EVP_Digest(wire, size, first, &length, EVP_sha256(), NULL) == 1 && length == 32);
    CHECK(EVP_Digest(first, sizeof(first), second, &length, EVP_sha256(), NULL) == 1 && length == 32);
    for (size_t i = 0; i < 32; ++i) CHECK(view->transaction_id[i] == second[31 - i]);
#else
    (void)wire; (void)size; (void)view;
#endif
}

static void inspect(const uint8_t *wire, size_t size, uint32_t index)
{
    struct { uint64_t before; zcl_source_view view; uint64_t after; } output, unchanged;
    memset(&output, 0xa5, sizeof(output)); memcpy(&unchanged, &output, sizeof(output));
    const zcl_status status = zcl_legacy_source_inspect(wire, size, index, &output.view);
    CHECK(output.before == unchanged.before && output.after == unchanged.after);
    if (status != ZCL_OK) { CHECK(memcmp(&output, &unchanged, sizeof(output)) == 0); return; }
    CHECK(output.view.output_count > index && output.view.output_count <= size / 9);
    CHECK(output.view.input_count <= size / 41 && output.view.joinsplit_count <= size / 1802);
    CHECK(output.view.spend_count == 0 && output.view.shielded_count == 0 && output.view.value_balance == 0);
    CHECK(output.view.output.value <= ZCL_MAX_MONEY && output.view.output.script_len <= 25);
    identity(wire, size, &output.view);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 4 || size > ZCL_LEGACY_SOURCE_MAX + 4) return 0;
    const uint32_t index = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    inspect(data + 4, size - 4, index);
    const size_t row = data[0] % (sizeof(legacy_vectors) / sizeof(legacy_vectors[0]));
    const size_t length = legacy_vectors[row].length;
    memcpy(changed, legacy_vectors[row].wire, length);
    for (size_t i = 4; i < size; ++i) changed[(i - 4) % length] ^= data[i];
    inspect(changed, length, data[1] % 5);
    return 0;
}
