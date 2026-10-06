/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_transaction_check
#undef zcl_secure_zero
#include "transaction_fixture.h"
#include "../src/transaction_internal.h"
#include "zcl_keys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Codec retirement at %d: %s\n", __LINE__, #v); abort(); } } while (0)
typedef enum { NORMAL, CHECK_FAILURE, WRONG_SIZE } fault;
static fault failure;
static unsigned hash_clears, candidate_clears, wire_clears;
static bool base58_active;
static unsigned base58_clears[4];
/* Public fixture storage is bounded, single-threaded and absent from Android. */
static uint8_t fixture_wire[ZCL_TX_WIRE_MAX + 1];
static size_t fixture_length;
static zcl_transparent_tx fixture;

zcl_status zcl_codec_test_check(const zcl_transparent_tx *tx, size_t *length);
void zcl_codec_test_zero(void *buffer, size_t length);

static void filled(const void *buffer, size_t length, uint8_t value)
{
    CHECK(buffer != NULL);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

zcl_status zcl_codec_test_check(const zcl_transparent_tx *tx, size_t *length)
{
    const zcl_status status = zcl_transaction_check(tx, length);
    if (status != ZCL_OK) return status;
    if (failure == CHECK_FAILURE) return ZCL_IO_FAILURE;
    if (failure == WRONG_SIZE) { CHECK(*length < SIZE_MAX); ++*length; }
    return ZCL_OK;
}

void zcl_codec_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    if (base58_active) {
        const size_t lengths[] = {4, 32, 132, 184};
        size_t index = 0;
        while (index < 4 && length != lengths[index]) ++index;
        CHECK(index < 4);
        ++base58_clears[index];
    } else if (length == 32) ++hash_clears;
    else if (length == sizeof(zcl_transparent_tx)) ++candidate_clears;
    else { CHECK(length == ZCL_TX_WIRE_MAX); ++wire_clears; }
    zcl_secure_zero(buffer, length);
    filled(buffer, length, 0);
}

static void reset(fault mode)
{
    base58_active = false;
    failure = mode;
    hash_clears = candidate_clears = wire_clears = 0;
}

static void reset_base58(void)
{
    base58_active = true;
    memset(base58_clears, 0, sizeof(base58_clears));
}

static void base58_counts(unsigned four, unsigned thirty_two, unsigned one_thirty_two,
    unsigned one_eighty_four)
{
    CHECK(base58_clears[0] == four && base58_clears[1] == thirty_two);
    CHECK(base58_clears[2] == one_thirty_two && base58_clears[3] == one_eighty_four);
}

static void base58_retirement(void)
{
    uint8_t payload[128] = {0}, decoded[128] = {0}, text[184] = {0};
    for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = (uint8_t)(i + 1);
    const size_t lengths[] = {1, 78, 128};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        size_t text_len = 0;
        reset_base58();
        CHECK(zcl_base58check_encode(payload, lengths[i], text, sizeof(text), &text_len) == ZCL_OK);
        base58_counts(0, 2, 1, 2);
        size_t decoded_len = 0;
        reset_base58();
        CHECK(zcl_base58check_decode(text, text_len, decoded, sizeof(decoded), &decoded_len) == ZCL_OK);
        CHECK(decoded_len == lengths[i] && memcmp(decoded, payload, decoded_len) == 0);
        base58_counts(1, 2, 2, 0);
    }
    size_t length = 0;
    reset_base58();
    CHECK(zcl_base58check_decode((const uint8_t *)"0", 1, decoded, sizeof(decoded), &length) ==
        ZCL_INVALID_ENCODING);
    base58_counts(0, 0, 2, 0);
}

static void load_fixture(size_t index)
{
    static const char digits[] = "0123456789abcdef";
    const char *hex = transaction_vectors[index].hex;
    const size_t size = strlen(hex);
    CHECK(size % 2 == 0 && size / 2 <= ZCL_TX_WIRE_MAX);
    fixture_length = size / 2;
    for (size_t i = 0; i < fixture_length; ++i) {
        const char *a = strchr(digits, hex[2 * i]), *b = strchr(digits, hex[2 * i + 1]);
        CHECK(a != NULL && b != NULL);
        fixture_wire[i] = (uint8_t)((a - digits) * 16 + (b - digits));
    }
    reset(NORMAL);
    CHECK(zcl_transaction_parse(fixture_wire, fixture_length, &fixture) == ZCL_OK);
    CHECK(candidate_clears == 1 && wire_clears == 0 && hash_clears == fixture.input_count);
}

static void parse_refused(size_t length, unsigned hashes, unsigned candidates, fault mode)
{
    struct { uint64_t before; zcl_transparent_tx tx; uint64_t after; } box;
    memset(&box, 0xa5, sizeof(box));
    reset(mode);
    const zcl_status status = zcl_transaction_parse(fixture_wire, length, &box.tx);
    CHECK(status != ZCL_OK);
    if (mode == CHECK_FAILURE) CHECK(status == ZCL_IO_FAILURE);
    if (mode == WRONG_SIZE) CHECK(status == ZCL_INVALID_ENCODING);
    filled(&box, sizeof(box), 0xa5);
    CHECK(hash_clears == hashes && candidate_clears == candidates && wire_clears == 0);
}

static void serialization(size_t capacity, fault mode, zcl_status wanted)
{
    uint8_t wire[ZCL_TX_WIRE_MAX + 2];
    memset(wire, 0xa5, sizeof(wire));
    size_t length = SIZE_MAX;
    reset(mode);
    CHECK(zcl_transaction_serialize(&fixture, wire + 1, capacity, &length) == wanted);
    const bool staged = wanted == ZCL_OK || mode == WRONG_SIZE;
    CHECK(wire_clears == (staged ? 1U : 0U) && candidate_clears == 0);
    CHECK(hash_clears == (staged ? fixture.input_count : 0));
    if (wanted != ZCL_OK) {
        CHECK(length == SIZE_MAX);
        filled(wire, sizeof(wire), 0xa5);
        return;
    }
    CHECK(length == fixture_length && memcmp(wire + 1, fixture_wire, length) == 0);
    CHECK(wire[0] == 0xa5);
    filled(wire + 1 + length, sizeof(wire) - 1 - length, 0xa5);
}

static void fixture_cases(size_t index)
{
    load_fixture(index);
    CHECK(fixture.input_count <= UINT32_MAX);
    const unsigned inputs = (unsigned)fixture.input_count;
    for (size_t cut = 0; cut < fixture_length; ++cut)
        parse_refused(cut, cut >= 9 ? inputs : 0, cut >= 8 ? 1U : 0U, NORMAL);
    fixture_wire[fixture_length] = 0;
    parse_refused(fixture_length + 1, inputs, 1, NORMAL);
    parse_refused(fixture_length, inputs, 1, CHECK_FAILURE);
    parse_refused(fixture_length, inputs, 1, WRONG_SIZE);
    serialization(ZCL_TX_WIRE_MAX, NORMAL, ZCL_OK);
    serialization(fixture_length, NORMAL, ZCL_OK);
    serialization(fixture_length - 1, NORMAL, ZCL_BUFFER_TOO_SMALL);
    serialization(ZCL_TX_WIRE_MAX, CHECK_FAILURE, ZCL_IO_FAILURE);
    serialization(ZCL_TX_WIRE_MAX, WRONG_SIZE, ZCL_INVALID_ENCODING);
}

int main(void)
{
    for (size_t i = 0; i < sizeof(transaction_vectors) / sizeof(transaction_vectors[0]); ++i)
        fixture_cases(i);
    base58_retirement();
    CHECK(puts("Codec retirement: canonical fixtures, every truncation and staged failure passed") >= 0);
    return 0;
}
