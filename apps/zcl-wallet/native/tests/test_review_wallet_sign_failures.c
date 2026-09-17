/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "review_wallet_fixture.h"
#include "bip32_internal.h"
#include <stdlib.h>
#include <string.h>

#undef CHECK
#define CHECK(v) do { if (!(v)) { fprintf(stderr, "Wallet signing fault at %d\n", __LINE__); abort(); } } while (0)
#undef zcl_entropy_seed
#undef zcl_seed_private
#undef zcl_signature_create
#undef zcl_signature_p2pkh
#undef zcl_secure_zero
#undef zcl_random_bytes
#undef zcl_ec_begin
#undef zcl_ec_end
zcl_status zcl_entropy_seed(const uint8_t *, size_t, uint8_t *, size_t);
zcl_status zcl_seed_private(const uint8_t *, size_t, zcl_network, uint32_t, uint32_t,
    const secp256k1_context *, zcl_extended_private *);
zcl_status zcl_signature_create(const uint8_t *, size_t, const uint8_t *, size_t, zcl_signature *);
zcl_status zcl_signature_p2pkh(const zcl_signature *, const uint8_t *, size_t, const uint8_t *, size_t,
    uint8_t *, size_t, size_t *);
void zcl_secure_zero(void *, size_t);
zcl_status zcl_random_bytes(uint8_t *, size_t);
zcl_status zcl_ec_begin(zcl_ec_context *, const uint8_t *, size_t);
void zcl_ec_end(zcl_ec_context *);

static review_wallet_fixture fixture;
static const void *owned_entropy, *owned_seed, *owned_key;
static unsigned fault, samples, seeds, keys, signatures, verifications, entropy_wipes, key_wipes, work_wipes;
static unsigned context_ends;

zcl_status zcl_sign_test_random(uint8_t *output, size_t length)
{
    if (fault == 12 && seeds != 0) { memset(output, 0x81, length); return ZCL_IO_FAILURE; }
    return zcl_random_bytes(output, length);
}

zcl_status zcl_sign_test_begin(zcl_ec_context *context, const uint8_t *blind, size_t length)
{
    CHECK(context != NULL && context->handle == NULL && context->storage == NULL && length == 32);
    return fault == 11 ? ZCL_RESOURCE_EXHAUSTED : zcl_ec_begin(context, blind, length);
}

void zcl_sign_test_end(zcl_ec_context *context)
{
    CHECK(context_ends++ == 0 && owned_seed == NULL);
    zcl_ec_end(context);
    CHECK(context->storage == NULL && context->handle == NULL && context->storage_len == 0);
}

static void zeroed(const void *pointer, size_t size)
{
    const uint8_t *bytes = pointer;
    CHECK(bytes != NULL);
    for (size_t i = 0; i < size; ++i) CHECK(bytes[i] == 0);
}

zcl_status zcl_sign_test_seed(const uint8_t *entropy, size_t length, uint8_t *seed, size_t capacity)
{
    CHECK(seeds++ == 0 && samples == 1 && entropy != fixture.entropy && length == 16 && capacity == 64);
    owned_entropy = entropy; owned_seed = seed;
    zeroed(seed, capacity);
    if (fault == 1) { memset(seed, 0x51, capacity); return ZCL_CRYPTO_FAILURE; }
    return zcl_entropy_seed(entropy, length, seed, capacity);
}

zcl_status zcl_sign_test_private(const uint8_t *seed, size_t length, zcl_network network,
    uint32_t chain, uint32_t index, const secp256k1_context *context, zcl_extended_private *output)
{
    CHECK(keys++ == 0 && seed == owned_seed && length == 64 && chain == 0 && index == 0);
    CHECK(context != NULL && output != NULL && samples == 1);
    owned_key = output;
    if (fault == 2) { memset(output, 0x61, sizeof(*output)); return ZCL_CRYPTO_FAILURE; }
    return zcl_seed_private(seed, length, network, chain, fault == 8 ? 1 : index, context, output);
}

zcl_status zcl_sign_test_create(const uint8_t *secret, size_t secret_len,
    const uint8_t *digest, size_t digest_len, zcl_signature *output)
{
    CHECK(signatures++ == 0 && samples == 2 && entropy_wipes == 1 && context_ends == 1);
    CHECK(owned_entropy == NULL && secret == owned_key && secret_len == 32 && digest_len == 32);
    const zcl_extended_private *key = owned_key;
    zeroed(key->chain_code, sizeof(key->chain_code));
    if (fault == 3) { memset(output, 0x71, sizeof(*output)); return ZCL_CRYPTO_FAILURE; }
    uint8_t different[32] = {0};
    different[31] = 1;
    if (fault == 6) return zcl_signature_create(different, 32, digest, digest_len, output);
    memcpy(different, digest, 32); different[0] ^= 1;
    return zcl_signature_create(secret, secret_len, fault == 7 ? different : digest, digest_len, output);
}

zcl_status zcl_sign_test_verify(const zcl_signature *signature, const uint8_t *digest, size_t digest_len,
    const uint8_t *hash, size_t hash_len, uint8_t *script, size_t capacity, size_t *length)
{
    CHECK(verifications++ == 0 && samples == 2 && owned_key == NULL && key_wipes == 1);
    CHECK(owned_entropy == NULL && owned_seed == NULL);
    zeroed(script, capacity);
    if (fault == 4) { memset(script, 0x31, capacity); return ZCL_CRYPTO_FAILURE; }
    if (fault == 5) { *length = 0; return ZCL_OK; }
    return zcl_signature_p2pkh(signature, digest, digest_len, hash, hash_len, script, capacity, length);
}

void zcl_sign_test_zero(void *pointer, size_t length)
{
    CHECK(pointer != NULL);
    zcl_secure_zero(pointer, length);
    zeroed(pointer, length);
    if (pointer == owned_entropy && length == 32) { owned_entropy = NULL; ++entropy_wipes; }
    if (pointer == owned_seed) { CHECK(length == 64); owned_seed = NULL; }
    if (pointer == owned_key) { CHECK(length == sizeof(zcl_extended_private)); owned_key = NULL; ++key_wipes; }
    if (length > ZCL_STORAGE_PATH_MAX) {
        CHECK(owned_entropy == NULL && owned_seed == NULL && owned_key == NULL);
        CHECK(length <= 4096); ++work_wipes;
    }
}

static zcl_status sample(void *context, uint64_t *now)
{
    CHECK(context == NULL && now != NULL && samples < 3 && work_wipes == 0);
    *now = 100; ++samples;
    if (samples >= 2) CHECK(owned_entropy == NULL && owned_seed == NULL && context_ends == 1);
    if (samples == 3) CHECK(owned_key == NULL && key_wipes == 1);
    if (fault == 9 && samples == 2) *now = 90100;
    if (fault == 10 && samples == 3) return ZCL_IO_FAILURE;
    return ZCL_OK;
}

static void call_counts(unsigned mode)
{
    static const unsigned expected_signatures[] = {1,0,0,1,1,1,1,1,1,0,1,0,0};
    static const unsigned expected_verifications[] = {1,0,0,0,1,1,1,1,1,0,1,0,0};
    static const unsigned expected_samples[] = {3,1,1,2,2,2,2,2,2,2,3,1,1};
    CHECK(mode < sizeof(expected_samples) / sizeof(expected_samples[0]));
    CHECK(signatures == expected_signatures[mode]);
    CHECK(verifications == expected_verifications[mode]);
    CHECK(samples == expected_samples[mode]);
}

static void run(unsigned mode)
{
    CHECK(review_wallet_fixture_open(&fixture, ZCL_MAINNET, 16, false) == 0);
    fault = mode;
    samples = seeds = keys = signatures = verifications = entropy_wipes = key_wipes = work_wipes = 0;
    context_ends = 0;
    owned_entropy = owned_seed = owned_key = NULL;
    const zcl_review_wallet_input claim = review_wallet_fixture_claim(&fixture, 0, 0);
    const zcl_review_clock clock = {sample, NULL};
    const zcl_review_block block = {ZCL_MAINNET, 1000000, 0};
    zcl_signature output, before;
    memset(&output, 0xa5, sizeof(output)); before = output;
    const zcl_status status = zcl_review_input_wallet_sign(&fixture.review, fixture.id, &clock,
        0, &block, &claim, &output);
    if (mode == 0) CHECK(status == ZCL_OK && memcmp(&output, &before, sizeof(output)) != 0);
    else CHECK(status != ZCL_OK && memcmp(&output, &before, sizeof(output)) == 0);
    CHECK(work_wipes == 1 && seeds == 1 && entropy_wipes == 1 && context_ends == 1);
    CHECK(owned_entropy == NULL && owned_seed == NULL && owned_key == NULL);
    call_counts(mode);
    CHECK(review_wallet_fixture_close(&fixture) == 0);
}

int main(void)
{
    for (unsigned mode = 0; mode <= 12; ++mode) run(mode);
    puts("Wallet signing faults: private retirement, mismatched signatures and atomic refusal passed");
    return 0;
}
