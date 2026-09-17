/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_secure_zero
#undef zcl_network_genesis
#undef zcl_address_parse
#undef zcl_receive_from_entropy
#include "zcl_wallet_record.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Header retirement check at %d: %s\n", __LINE__, #v); abort(); } } while (0)

typedef enum { NORMAL, DIRTY_GENESIS, DIRTY_PARSE, DIRTY_DERIVE, SHORT_DERIVE, LONG_DERIVE } fault;
typedef struct { uintptr_t identity; size_t length; unsigned calls, clears; } observed_span;
static observed_span observed[3];
static unsigned info_clears, header_clears, derived_clears;
static fault failure;

void zcl_header_test_zero(void *buffer, size_t length);
zcl_status zcl_header_test_genesis(zcl_network network, uint8_t *output, size_t capacity);
zcl_status zcl_header_test_parse(const uint8_t *text, size_t length, zcl_network network, zcl_address *output);
zcl_status zcl_header_test_receive(const uint8_t *entropy, size_t entropy_len, zcl_network network,
    uint32_t index, const uint8_t *blinding, size_t blinding_len, uint8_t *output, size_t capacity, size_t *length);

static void filled(const void *buffer, size_t length, uint8_t value)
{
    CHECK(buffer != NULL);
    const uint8_t *bytes = buffer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == value);
}

static void capture(size_t slot, const void *buffer, size_t length)
{
    CHECK(slot < 3 && buffer != NULL && length > 0);
    observed_span *span = &observed[slot];
    CHECK(span->identity == 0 && span->calls == span->clears);
    span->identity = (uintptr_t)buffer;
    span->length = length;
    ++span->calls;
}

void zcl_header_test_zero(void *buffer, size_t length)
{
    CHECK(buffer != NULL);
    zcl_secure_zero(buffer, length);
    filled(buffer, length, 0);
    const uintptr_t start = (uintptr_t)buffer;
    for (size_t i = 0; i < 3; ++i) {
        observed_span *span = &observed[i];
        if (span->identity == 0 || span->identity < start || span->identity - start >= length) continue;
        CHECK(length - (size_t)(span->identity - start) >= span->length);
        span->identity = 0;
        ++span->clears;
    }
    if (length == sizeof(zcl_wallet_info)) ++info_clears;
    if (length == 80) ++header_clears;
    if (length == 35) ++derived_clears;
}

zcl_status zcl_header_test_genesis(zcl_network network, uint8_t *output, size_t capacity)
{
    CHECK(capacity == 32);
    capture(0, output, capacity);
    if (failure == DIRTY_GENESIS) { memset(output, 0x5a, capacity); return ZCL_CRYPTO_FAILURE; }
    return zcl_network_genesis(network, output, capacity);
}

zcl_status zcl_header_test_parse(const uint8_t *text, size_t length, zcl_network network, zcl_address *output)
{
    capture(1, output, sizeof(*output));
    if (failure == DIRTY_PARSE) { memset(output, 0x5a, sizeof(*output)); return ZCL_INVALID_ENCODING; }
    return zcl_address_parse(text, length, network, output);
}

zcl_status zcl_header_test_receive(const uint8_t *entropy, size_t entropy_len, zcl_network network,
    uint32_t index, const uint8_t *blinding, size_t blinding_len, uint8_t *output, size_t capacity, size_t *length)
{
    CHECK(capacity == 35 && length != NULL && index == 0);
    capture(2, output, capacity);
    if (failure == DIRTY_DERIVE) { memset(output, 0x5a, capacity); return ZCL_CRYPTO_FAILURE; }
    const zcl_status status = zcl_receive_from_entropy(entropy, entropy_len, network, index,
        blinding, blinding_len, output, capacity, length);
    if (status == ZCL_OK && failure == SHORT_DERIVE) *length = 34;
    if (status == ZCL_OK && failure == LONG_DERIVE) *length = SIZE_MAX;
    return status;
}

static void retired(void)
{
    for (size_t i = 0; i < 3; ++i)
        CHECK(observed[i].identity == 0 && observed[i].calls == observed[i].clears);
}

static void reset(fault mode)
{
    retired();
    memset(observed, 0, sizeof(observed));
    info_clears = header_clears = derived_clears = 0;
    failure = mode;
}

static void header(uint8_t output[80], zcl_network network)
{
    const uint8_t entropy[16] = {0}, blinding[32] = {1};
    reset(NORMAL);
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), network, blinding, sizeof(blinding),
        output, 80) == ZCL_OK);
    retired();
    CHECK(header_clears == 1);
}

static void creation(void)
{
    const uint8_t entropy[16] = {0}, blinding[32] = {1};
    uint8_t expected[80], output[82];
    header(expected, ZCL_TESTNET);
    static const fault modes[] = {NORMAL, DIRTY_GENESIS, DIRTY_DERIVE, SHORT_DERIVE, LONG_DERIVE};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        reset(modes[i]);
        memset(output, 0xa5, sizeof(output));
        const zcl_status wanted = modes[i] == NORMAL ? ZCL_OK : ZCL_CRYPTO_FAILURE;
        CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_TESTNET, blinding, sizeof(blinding),
            output + 1, 80) == wanted);
        retired();
        CHECK(header_clears == 1 && output[0] == 0xa5 && output[81] == 0xa5);
        if (wanted == ZCL_OK) CHECK(memcmp(output + 1, expected, 80) == 0);
        else filled(output, sizeof(output), 0xa5);
    }
}

static void parsing(void)
{
    uint8_t input[80];
    header(input, ZCL_MAINNET);
    static const fault modes[] = {NORMAL, DIRTY_GENESIS, DIRTY_PARSE};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        zcl_wallet_info output, before;
        memset(&output, 0xa5, sizeof(output)); memcpy(&before, &output, sizeof(before));
        reset(modes[i]);
        const zcl_status wanted = modes[i] == NORMAL ? ZCL_OK :
            modes[i] == DIRTY_GENESIS ? ZCL_CRYPTO_FAILURE : ZCL_INVALID_ENCODING;
        CHECK(zcl_wallet_header_parse(input, sizeof(input), &output) == wanted);
        retired();
        CHECK(info_clears == 1);
        if (wanted == ZCL_OK) {
            CHECK(output.network == ZCL_MAINNET && output.entropy_len == 16);
            CHECK(memcmp(output.address, input + 44, 35) == 0);
        } else CHECK(memcmp(&output, &before, sizeof(output)) == 0);
    }
}

static void recovery(void)
{
    uint8_t input[80], output[37];
    const uint8_t entropy[16] = {0}, blinding[32] = {1};
    header(input, ZCL_MAINNET);
    static const zcl_status wanted[] = {ZCL_OK, ZCL_CRYPTO_FAILURE, ZCL_INVALID_ENCODING,
        ZCL_CRYPTO_FAILURE, ZCL_INVALID_ENCODING, ZCL_INVALID_ENCODING};
    for (unsigned i = NORMAL; i <= LONG_DERIVE; ++i) {
        reset((fault)i);
        memset(output, 0xa5, sizeof(output));
        CHECK(zcl_wallet_recovered_address(input, sizeof(input), entropy, sizeof(entropy),
            blinding, sizeof(blinding), output + 1, 35) == wanted[i]);
        retired();
        CHECK(info_clears == 2 && derived_clears == 1);
        CHECK(output[0] == 0xa5 && output[36] == 0xa5);
        if (wanted[i] == ZCL_OK) CHECK(memcmp(output + 1, input + 44, 35) == 0);
        else filled(output, sizeof(output), 0xa5);
    }
}

static void mismatches(void)
{
    uint8_t input[80], changed[80], entropy[16] = {0}, output[35];
    const uint8_t blinding[32] = {1};
    header(input, ZCL_MAINNET);
    for (unsigned mismatch = 0; mismatch < 4; ++mismatch) {
        reset(NORMAL);
        memcpy(changed, input, sizeof(changed));
        if (mismatch == 0) changed[12] ^= 1; /* Genesis mismatch before address parsing. */
        if (mismatch == 1) changed[7] = 20; /* Structurally valid entropy-length mismatch. */
        if (mismatch == 2) changed[4] = 2; /* Unsupported fixed header, before scratch parsing. */
        entropy[0] = mismatch == 3 ? 1 : 0;
        memset(output, 0xa5, sizeof(output));
        const zcl_status wanted = mismatch == 2 ? ZCL_UNSUPPORTED : ZCL_INVALID_ENCODING;
        CHECK(zcl_wallet_recovered_address(changed, sizeof(changed), entropy, sizeof(entropy),
            blinding, sizeof(blinding), output, sizeof(output)) == wanted);
        retired();
        CHECK(info_clears == (mismatch == 2 ? 1U : 2U) && derived_clears == 1);
        filled(output, sizeof(output), 0xa5);
    }
}

static void unsupported_address(void)
{
    uint8_t input[80], output[35];
    const uint8_t entropy[16] = {0}, blinding[32] = {1};
    header(input, ZCL_MAINNET);
    const zcl_address script_address = {.network = ZCL_MAINNET, .kind = ZCL_P2SH};
    size_t length = 0;
    CHECK(zcl_address_encode(&script_address, input + 44, 35, &length) == ZCL_OK && length == 35);
    reset(NORMAL);
    memset(output, 0xa5, sizeof(output));
    CHECK(zcl_wallet_recovered_address(input, sizeof(input), entropy, sizeof(entropy),
        blinding, sizeof(blinding), output, sizeof(output)) == ZCL_UNSUPPORTED);
    retired();
    CHECK(info_clears == 2 && derived_clears == 1 && observed[2].calls == 0);
    filled(output, sizeof(output), 0xa5);
}

static void admission(void)
{
    uint8_t input[80], output[80];
    const uint8_t entropy[16] = {0}, blinding[32] = {1};
    header(input, ZCL_MAINNET);
    reset(NORMAL);
    memset(output, 0xa5, sizeof(output));
    for (size_t capacity = 0; capacity < 80; ++capacity)
        CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_MAINNET, blinding, sizeof(blinding),
            output, capacity) == ZCL_BUFFER_TOO_SMALL);
    for (size_t capacity = 0; capacity < 35; ++capacity)
        CHECK(zcl_wallet_recovered_address(input, sizeof(input), entropy, sizeof(entropy),
            blinding, sizeof(blinding), output, capacity) == ZCL_BUFFER_TOO_SMALL);
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_MAINNET, blinding, sizeof(blinding),
        NULL, 80) == ZCL_INVALID_ARGUMENT);
    CHECK(zcl_wallet_recovered_address(input, sizeof(input), entropy, sizeof(entropy),
        blinding, sizeof(blinding), NULL, 35) == ZCL_INVALID_ARGUMENT);
    filled(output, sizeof(output), 0xa5);
    CHECK(info_clears == 0 && header_clears == 0 && derived_clears == 0);
    for (size_t i = 0; i < 3; ++i) CHECK(observed[i].calls == 0);
}

int main(void)
{
    creation(); parsing(); recovery(); mismatches(); unsupported_address(); admission();
    retired();
    CHECK(puts("Header creation/parsing/recovery: live scratch retirement, dirty providers and exact output binding passed") >= 0);
    return 0;
}
