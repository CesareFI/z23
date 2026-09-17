/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_transaction_parse
#undef zcl_transaction_serialize
#undef mbedtls_sha256
#undef zcl_secure_zero
#include "transaction_fixture.h"
#include "zcl_keys.h"
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Transaction retirement at %d: %s\n", __LINE__, #v); abort(); } } while (0)
typedef enum { NORMAL, PARSE_FAILURE, SERIALIZE_FAILURE, FIRST_HASH_FAILURE, SECOND_HASH_FAILURE } fault;
static fault failure;
static uintptr_t parsed_id, wire_id, hash_ids[2];
static unsigned parses, parsed_clears, serializes, wire_clears, hashes, hash_clears, extra_digest_clears;
/* Public fixture storage is single-threaded and absent from production. */
static zcl_transparent_tx fixture;
static uint8_t fixture_wire[ZCL_TX_WIRE_MAX], fixture_id[32];
static size_t fixture_length;

zcl_status zcl_tx_test_parse(const uint8_t *wire, size_t length, zcl_transparent_tx *output);
zcl_status zcl_tx_test_serialize(const zcl_transparent_tx *tx, uint8_t *wire, size_t capacity, size_t *length);
int zcl_tx_test_sha256(const unsigned char *input, size_t length, unsigned char *output, int is224);
void zcl_tx_test_zero(void *buffer, size_t length);

static void filled(const void *buffer, size_t length, uint8_t value)
{
    CHECK(buffer != NULL);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

zcl_status zcl_tx_test_parse(const uint8_t *wire, size_t length, zcl_transparent_tx *output)
{
    CHECK(output != NULL && parsed_id == 0 && parses == parsed_clears);
    parsed_id = (uintptr_t)output;
    ++parses;
    if (failure == PARSE_FAILURE) { memset(output, 0x5a, sizeof(*output)); return ZCL_INVALID_ENCODING; }
    return zcl_transaction_parse(wire, length, output);
}

zcl_status zcl_tx_test_serialize(const zcl_transparent_tx *tx, uint8_t *wire, size_t capacity, size_t *length)
{
    CHECK(wire != NULL && capacity == ZCL_TX_WIRE_MAX && wire_id == 0 && serializes == wire_clears);
    wire_id = (uintptr_t)wire;
    ++serializes;
    if (failure == SERIALIZE_FAILURE) { memset(wire, 0x5a, capacity); return ZCL_INVALID_ENCODING; }
    return zcl_transaction_serialize(tx, wire, capacity, length);
}

int zcl_tx_test_sha256(const unsigned char *input, size_t length, unsigned char *output, int is224)
{
    CHECK(hashes < 2 && input != NULL && output != NULL && is224 == 0);
    hash_ids[hashes++] = (uintptr_t)output;
    if ((failure == FIRST_HASH_FAILURE && hashes == 1) || (failure == SECOND_HASH_FAILURE && hashes == 2)) {
        memset(output, 0x5a, 32);
        return -1;
    }
    return mbedtls_sha256(input, length, output, is224);
}

static bool clear_hash(uintptr_t identity, size_t length)
{
    for (size_t i = 0; i < 2; ++i) {
        if (identity != hash_ids[i]) continue;
        CHECK(length == 32);
        hash_ids[i] = 0;
        ++hash_clears;
        return true;
    }
    return false;
}

void zcl_tx_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    filled(buffer, length, 0);
    const uintptr_t identity = (uintptr_t)buffer;
    if (identity == parsed_id) {
        CHECK(length == sizeof(zcl_transparent_tx));
        parsed_id = 0; ++parsed_clears;
    } else if (identity == wire_id) {
        CHECK(length == ZCL_TX_WIRE_MAX);
        wire_id = 0; ++wire_clears;
    } else if (!clear_hash(identity, length)) {
        CHECK(length == 32);
        ++extra_digest_clears;
    }
}

static void retired(void)
{
    CHECK(parsed_id == 0 && wire_id == 0 && hash_ids[0] == 0 && hash_ids[1] == 0);
    CHECK(parses == parsed_clears && serializes == wire_clears && hashes == hash_clears);
}

static void reset(fault mode)
{
    retired();
    parses = parsed_clears = serializes = wire_clears = hashes = hash_clears = extra_digest_clears = 0;
    failure = mode;
}

static size_t decode(const char *hex, uint8_t *bytes, size_t capacity)
{
    static const char digits[] = "0123456789abcdef";
    const size_t size = strlen(hex);
    CHECK(size % 2 == 0 && size / 2 <= capacity);
    for (size_t i = 0; i < size / 2; ++i) {
        const char *a = strchr(digits, hex[2 * i]), *b = strchr(digits, hex[2 * i + 1]);
        CHECK(a != NULL && b != NULL);
        bytes[i] = (uint8_t)((a - digits) * 16 + (b - digits));
    }
    return size / 2;
}

static void load_fixture(size_t index)
{
    CHECK(index < sizeof(transaction_vectors) / sizeof(transaction_vectors[0]));
    fixture_length = decode(transaction_vectors[index].hex, fixture_wire, sizeof(fixture_wire));
    CHECK(decode(transaction_vectors[index].txid, fixture_id, sizeof(fixture_id)) == 32);
    CHECK(zcl_transaction_parse(fixture_wire, fixture_length, &fixture) == ZCL_OK);
}

static void id_case(fault mode)
{
    uint8_t output[34];
    memset(output, 0xa5, sizeof(output));
    reset(mode);
    const zcl_status wanted = mode == NORMAL ? ZCL_OK :
        mode == SERIALIZE_FAILURE ? ZCL_INVALID_ENCODING : ZCL_CRYPTO_FAILURE;
    CHECK(zcl_transaction_id(&fixture, output + 1, 32) == wanted);
    retired();
    CHECK(serializes == 1 && parses == 0 && hashes + extra_digest_clears == 2);
    CHECK(output[0] == 0xa5 && output[33] == 0xa5);
    if (wanted == ZCL_OK) CHECK(memcmp(output + 1, fixture_id, 32) == 0);
    else filled(output, sizeof(output), 0xa5);
}

static void prevout_case(fault mode, size_t index)
{
    zcl_tx_input input = {0};
    memcpy(input.previous_txid, fixture_id, sizeof(fixture_id));
    CHECK(index <= UINT32_MAX);
    input.previous_index = (uint32_t)index;
    struct { uint64_t before; zcl_tx_output output; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    reset(mode);
    const zcl_status wanted = mode == NORMAL ? ZCL_OK :
        mode <= SERIALIZE_FAILURE ? ZCL_INVALID_ENCODING : ZCL_CRYPTO_FAILURE;
    CHECK(zcl_transaction_prevout(&input, fixture_wire, fixture_length, &box.output) == wanted);
    retired();
    CHECK(parses == 1 && extra_digest_clears + hashes == (mode == PARSE_FAILURE ? 1U : 3U));
    CHECK(box.before == UINT64_C(0xa5a5a5a5a5a5a5a5) && box.after == box.before);
    if (wanted == ZCL_OK) CHECK(memcmp(&box.output, &fixture.outputs[index], sizeof(box.output)) == 0);
    else filled(&box, sizeof(box), 0xa5);
}

static void refused_binding(bool wrong_hash)
{
    zcl_tx_input input = {0};
    memcpy(input.previous_txid, fixture_id, sizeof(fixture_id));
    if (wrong_hash) input.previous_txid[0] ^= 1;
    else input.previous_index = UINT32_MAX;
    zcl_tx_output output;
    memset(&output, 0xa5, sizeof(output));
    reset(NORMAL);
    CHECK(zcl_transaction_prevout(&input, fixture_wire, fixture_length, &output) ==
        (wrong_hash ? ZCL_INVALID_ENCODING : ZCL_OUT_OF_RANGE));
    retired();
    CHECK(parses == 1 && hashes == 2 && extra_digest_clears == 1);
    filled(&output, sizeof(output), 0xa5);
}

int main(void)
{
    static const fault id_modes[] = {NORMAL, SERIALIZE_FAILURE, FIRST_HASH_FAILURE, SECOND_HASH_FAILURE};
    for (size_t i = 0; i < sizeof(transaction_vectors) / sizeof(transaction_vectors[0]); ++i) {
        load_fixture(i);
        for (size_t mode = 0; mode < sizeof(id_modes) / sizeof(id_modes[0]); ++mode) id_case(id_modes[mode]);
        for (size_t index = 0; index < fixture.output_count; ++index) prevout_case(NORMAL, index);
        for (fault mode = PARSE_FAILURE; mode <= SECOND_HASH_FAILURE; mode = (fault)((unsigned)mode + 1U))
            prevout_case(mode, 0);
        refused_binding(false); refused_binding(true);
    }
    retired();
    CHECK(puts("Transaction ID/prevout retirement: exact public vectors, dirty providers and atomic hash-bound output passed") >= 0);
    return 0;
}
