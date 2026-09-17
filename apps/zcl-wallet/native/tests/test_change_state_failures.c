/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#undef zcl_hmac_sha512
#undef zcl_secure_zero
#include "zcl_change_state.h"
#include "secret_hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Change state failure check at %d\n", __LINE__); abort(); } } while (0)
typedef struct { void *pointer; bool cleared; } secret_span;
static secret_span spans[2];
static size_t span_count;
static unsigned hmac_calls, fail_call, anchor_clears, secret_clears, candidate_clears;

zcl_status zcl_change_state_test_hmac(const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len, uint8_t *output, size_t capacity)
{
    CHECK(output != NULL && capacity >= 64);
    ++hmac_calls;
    if (hmac_calls <= 2) {
        CHECK(span_count < 2);
        spans[span_count++] = (secret_span){output, false};
    }
    if (hmac_calls == fail_call) {
        /* Stronger than the real helper's unchanged-error output contract:
         * even partially written extract/expand scratch must still clear. */
        memset(output, 0x42, 16);
        return ZCL_CRYPTO_FAILURE;
    }
    return zcl_hmac_sha512(key, key_len, data, data_len, output, capacity);
}

void zcl_change_state_test_zero(void *pointer, size_t length)
{
    CHECK(pointer != NULL && (length == 35 || length == 64 || length == 80));
    if (length == 35) ++anchor_clears;
    if (length == 64) ++secret_clears;
    if (length == 80) ++candidate_clears;
    zcl_secure_zero(pointer, length);
    const uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) CHECK(bytes[i] == 0);
    for (size_t i = 0; i < span_count; ++i) {
        if (spans[i].pointer != pointer) continue;
        spans[i].pointer = NULL; /* Retire while its object is still live. */
        spans[i].cleared = true;
    }
}

static void reset(unsigned failure)
{
    for (size_t i = 0; i < span_count; ++i) CHECK(spans[i].cleared);
    memset(spans, 0, sizeof(spans));
    span_count = 0;
    hmac_calls = anchor_clears = secret_clears = candidate_clears = 0;
    fail_call = failure;
}

static void verify(unsigned failure, unsigned expected_secrets, unsigned expected_candidates)
{
    CHECK(hmac_calls == (failure == 0 ? 3 : failure));
    CHECK(span_count == (failure == 1 ? 1 : 2));
    CHECK(anchor_clears == 1);
    CHECK(secret_clears == expected_secrets);
    CHECK(candidate_clears == expected_candidates);
    for (size_t i = 0; i < span_count; ++i) CHECK(spans[i].cleared && spans[i].pointer == NULL);
}

int main(void)
{
    uint8_t entropy[16] = {0}, blinding[32] = {1}, header[80], reference[80];
    CHECK(zcl_wallet_header_create(entropy, sizeof(entropy), ZCL_MAINNET, blinding, sizeof(blinding),
        header, sizeof(header)) == ZCL_OK);
    reset(0);
    CHECK(zcl_change_state_encode(header, 80, entropy, 16, blinding, 32, 19, reference, 80) == ZCL_OK);
    verify(0, 2, 1);
    for (unsigned failure = 1; failure <= 3; ++failure) {
        uint8_t output[82], before[82];
        memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
        reset(failure);
        CHECK(zcl_change_state_encode(header, 80, entropy, 16, blinding, 32, 19, output + 1, 80) == ZCL_CRYPTO_FAILURE);
        verify(failure, 2, 1);
        CHECK(memcmp(output, before, sizeof(output)) == 0);
        uint32_t indexes[3] = {UINT32_C(0x12345678), UINT32_MAX, UINT32_C(0xabcdef01)};
        uint32_t saved[3]; memcpy(saved, indexes, sizeof(saved));
        reset(failure);
        CHECK(zcl_change_state_decode(header, 80, entropy, 16, blinding, 32, reference, 80, &indexes[1]) == ZCL_CRYPTO_FAILURE);
        verify(failure, 3, 0);
        CHECK(memcmp(indexes, saved, sizeof(indexes)) == 0);
    }
    reset(0);
    uint32_t index = UINT32_MAX;
    CHECK(zcl_change_state_decode(header, 80, entropy, 16, blinding, 32, reference, 80, &index) == ZCL_OK && index == 19);
    verify(0, 3, 0);
    reset(0);
    reference[79] ^= 1;
    index = UINT32_MAX;
    CHECK(zcl_change_state_decode(header, 80, entropy, 16, blinding, 32, reference, 80, &index) == ZCL_INVALID_ENCODING);
    CHECK(index == UINT32_MAX); verify(0, 3, 0);
    reset(0);
    reference[0] ^= 1;
    CHECK(zcl_change_state_decode(header, 80, entropy, 16, blinding, 32, reference, 80, &index) != ZCL_OK);
    CHECK(index == UINT32_MAX && hmac_calls == 0 && anchor_clears == 0 &&
        secret_clears == 0 && candidate_clears == 0);
    reference[0] ^= 1;
    uint8_t invalid_header[80];
    memcpy(invalid_header, header, sizeof(invalid_header));
    invalid_header[0] ^= 1;
    uint8_t output[80], before[80];
    memset(output, 0xa5, sizeof(output)); memcpy(before, output, sizeof(output));
    reset(0);
    CHECK(zcl_change_state_encode(invalid_header, sizeof(invalid_header), entropy, sizeof(entropy),
        blinding, sizeof(blinding), 19, output, sizeof(output)) == ZCL_UNSUPPORTED);
    CHECK(hmac_calls == 0 && anchor_clears == 1 && secret_clears == 1 && candidate_clears == 1);
    CHECK(memcmp(output, before, sizeof(output)) == 0);
    reset(0);
    index = UINT32_MAX;
    CHECK(zcl_change_state_decode(invalid_header, sizeof(invalid_header), entropy, sizeof(entropy),
        blinding, sizeof(blinding), reference, sizeof(reference), &index) == ZCL_UNSUPPORTED);
    CHECK(index == UINT32_MAX && hmac_calls == 0 && anchor_clears == 1 &&
        secret_clears == 2 && candidate_clears == 0);
    zcl_secure_zero(entropy, sizeof(entropy));
    puts("Change state HKDF/HMAC failure cleanup checks passed");
    return 0;
}
