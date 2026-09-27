/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_review.h"
#include "zcl_zip243_host.h"
#include "zcl_zip243.h"

#include "crypto/blake2b.h"
#include <openssl/evp.h>

#undef NDEBUG
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t bytes[256];
    size_t length, output_end[2], second_amount;
} fixture;

static void append_u32(fixture *item, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        item->bytes[item->length++] = (uint8_t)(value >> (i * 8));
}

static void append_u64(fixture *item, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        item->bytes[item->length++] = (uint8_t)(value >> (i * 8));
}

static fixture make_fixture(void) {
    fixture item = {0};
    append_u32(&item, 0x80000004);
    append_u32(&item, 0x892f2085);
    item.bytes[item.length++] = 1;
    memset(item.bytes + item.length, 0xaa, 32);
    item.length += 32;
    append_u32(&item, 0);
    item.bytes[item.length++] = 0;
    append_u32(&item, UINT32_MAX - 1);
    item.bytes[item.length++] = 2;
    append_u64(&item, 100000000);
    item.bytes[item.length++] = 25;
    const uint8_t p2pkh[3] = {0x76, 0xa9, 0x14};
    memcpy(item.bytes + item.length, p2pkh, sizeof p2pkh);
    item.length += sizeof p2pkh;
    memset(item.bytes + item.length, 0x11, 20);
    item.length += 20;
    item.bytes[item.length++] = 0x88;
    item.bytes[item.length++] = 0xac;
    item.output_end[0] = item.length;
    item.second_amount = item.length;
    append_u64(&item, 200000000);
    item.bytes[item.length++] = 23;
    item.bytes[item.length++] = 0xa9;
    item.bytes[item.length++] = 0x14;
    memset(item.bytes + item.length, 0x22, 20);
    item.length += 20;
    item.bytes[item.length++] = 0x87;
    item.output_end[1] = item.length;
    append_u32(&item, 100);
    append_u32(&item, 200);
    append_u64(&item, 0);
    item.bytes[item.length++] = 0;
    item.bytes[item.length++] = 0;
    item.bytes[item.length++] = 0;
    return item;
}

static bool sha_init(void *context) {
    return EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;
}

static bool sha_update(void *context, const uint8_t *bytes, size_t length) {
    return EVP_DigestUpdate(context, bytes, length) == 1;
}

static bool sha_final(void *context, uint8_t digest[32]) {
    unsigned length = 0;
    return EVP_DigestFinal_ex(context, digest, &length) == 1 && length == 32;
}

static void begin(blue_payment_review *review, const fixture *item,
    struct blake2b_ctx *blake_context, EVP_MD_CTX *sha_context) {
    zcl_zip243_hasher blake = zcl_zip243_host_hasher(blake_context);
    zcl_tx_replay_sha256 sha = {.context = sha_context, .init = sha_init,
        .update = sha_update, .final = sha_final};
    assert(blue_payment_review_begin(review, (uint32_t)item->length, 0,
        0x76b809bb, &blake, &sha));
}

static void first_two_passes(blue_payment_review *review,
    const fixture *item) {
    for (unsigned pass = 0; pass < 2; ++pass) {
        assert(blue_payment_review_feed(review, item->bytes, item->length));
        assert(blue_payment_review_next_pass(review));
    }
    assert(review->total_outputs == 2);
}

static void test_success(const fixture *item) {
    struct blake2b_ctx blake_context, reference_context;
    EVP_MD_CTX *sha_context = EVP_MD_CTX_new();
    assert(sha_context);
    blue_payment_review review;
    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    uint8_t script_code[25] = {0x76, 0xa9, 0x14};
    memset(script_code + 3, 0x33, 20);
    script_code[23] = 0x88;
    script_code[24] = 0xac;
    assert(blue_payment_review_feed(&review, item->bytes,
        item->output_end[0]));
    const blue_payment_output *output = blue_payment_review_pending(&review);
    assert(output && output->index == 0 && output->amount_zat == 100000000);
    assert(output->type == ZCL_TX_STREAM_P2PKH);
    for (size_t i = 0; i < 20; ++i) assert(output->hash160[i] == 0x11);
    assert(blue_payment_review_acknowledge(&review));
    assert(blue_payment_review_feed(&review,
        item->bytes + item->output_end[0],
        item->output_end[1] - item->output_end[0]));
    output = blue_payment_review_pending(&review);
    assert(output && output->index == 1 && output->amount_zat == 200000000);
    assert(output->type == ZCL_TX_STREAM_P2SH);
    for (size_t i = 0; i < 20; ++i) assert(output->hash160[i] == 0x22);
    assert(blue_payment_review_acknowledge(&review));
    assert(blue_payment_review_feed(&review,
        item->bytes + item->output_end[1],
        item->length - item->output_end[1]));
    zcl_tx_stream_facts facts;
    uint8_t actual[32], expected[32];
    assert(blue_payment_review_finish(&review, script_code,
        sizeof script_code, 300000001, &facts, actual));
    assert(facts.inputs == 1 && facts.outputs == 2);
    assert(facts.output_zat == 300000000);
    zcl_zip243_hasher reference = zcl_zip243_host_hasher(&reference_context);
    assert(zcl_zip243_transparent_digest(item->bytes, item->length, 0,
        script_code, sizeof script_code, 300000001, 0x76b809bb,
        &reference, expected) == 0);
    assert(memcmp(actual, expected, sizeof actual) == 0);
    assert(!blue_payment_review_acknowledge(&review));
    EVP_MD_CTX_free(sha_context);
}

static void test_failures(const fixture *item) {
    struct blake2b_ctx blake_context;
    EVP_MD_CTX *sha_context = EVP_MD_CTX_new();
    assert(sha_context);
    blue_payment_review review;
    zcl_tx_stream_facts facts;
    uint8_t digest[32];
    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    assert(blue_payment_review_feed(&review, item->bytes,
        item->output_end[0]));
    assert(!blue_payment_review_feed(&review,
        item->bytes + item->output_end[0], 1));
    assert(!blue_payment_review_pending(&review));
    assert(!blue_payment_review_acknowledge(&review));

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    assert(!blue_payment_review_feed(&review, item->bytes,
        item->output_end[1]));
    assert(!blue_payment_review_pending(&review));

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    assert(blue_payment_review_feed(&review, item->bytes,
        item->output_end[0]));
    assert(!blue_payment_review_finish(&review, NULL, 0, 0,
        &facts, digest));
    for (size_t i = 0; i < sizeof digest; ++i) assert(!digest[i]);

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    uint8_t changed[sizeof item->bytes];
    memcpy(changed, item->bytes, item->length);
    changed[item->second_amount] ^= 1;
    assert(blue_payment_review_feed(&review, changed,
        item->output_end[0]));
    assert(blue_payment_review_acknowledge(&review));
    assert(blue_payment_review_feed(&review,
        changed + item->output_end[0],
        item->output_end[1] - item->output_end[0]));
    assert(blue_payment_review_acknowledge(&review));
    assert(blue_payment_review_feed(&review,
        changed + item->output_end[1],
        item->length - item->output_end[1]));
    assert(!blue_payment_review_finish(&review, NULL, 0, 0,
        &facts, digest));
    for (size_t i = 0; i < sizeof digest; ++i) assert(!digest[i]);

    begin(&review, item, &blake_context, sha_context);
    first_two_passes(&review, item);
    blue_payment_review_abort(&review);
    assert(!blue_payment_review_feed(&review, item->bytes, 1));
    EVP_MD_CTX_free(sha_context);
}

int main(void) {
    fixture item = make_fixture();
    test_success(&item);
    test_failures(&item);
    return 0;
}
