/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
#include "blue_payment_sign.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/sha.h>

#undef NDEBUG
#include <assert.h>
#include <string.h>

typedef struct {
    EVP_PKEY *key;
    uint8_t public_key[33];
    unsigned calls;
    bool refuse, malformed;
} signer_fixture;

static void initialize_signer(signer_fixture *fixture) {
    static char group[] = "secp256k1";
    OSSL_PARAM params[] = {
        OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, group,
                               sizeof group - 1),
        OSSL_PARAM_END
    };
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    assert(context && EVP_PKEY_keygen_init(context) > 0);
    assert(EVP_PKEY_CTX_set_params(context, params) > 0);
    assert(EVP_PKEY_generate(context, &fixture->key) > 0);
    EVP_PKEY_CTX_free(context);
    uint8_t uncompressed[65];
    size_t length = 0;
    assert(EVP_PKEY_get_octet_string_param(fixture->key,
        OSSL_PKEY_PARAM_PUB_KEY, uncompressed, sizeof uncompressed,
        &length) > 0);
    assert(length == 65 && uncompressed[0] == 4);
    fixture->public_key[0] = (uint8_t)(2u | (uncompressed[64] & 1u));
    memcpy(fixture->public_key + 1, uncompressed + 1, 32);
}

static bool hash_public_key(const uint8_t public_key[33],
    uint8_t hash160[20]) {
    uint8_t first[32];
    unsigned length = 0;
    return SHA256(public_key, 33, first) != NULL &&
        EVP_Digest(first, sizeof first, hash160, &length,
                   EVP_ripemd160(), NULL) == 1 && length == 20;
}

static bool sign_digest(void *context, uint8_t path,
    const uint8_t digest[32], uint8_t public_key[33],
    uint8_t signature[BLUE_ECDSA_DER_MAX], size_t *signature_length) {
    signer_fixture *fixture = context;
    ++fixture->calls;
    if (fixture->refuse || path != BLUE_PAYMENT_INPUT_EXTERNAL) return false;
    EVP_PKEY_CTX *signer = EVP_PKEY_CTX_new(fixture->key, NULL);
    *signature_length = BLUE_ECDSA_DER_MAX;
    bool valid = signer && EVP_PKEY_sign_init(signer) > 0 &&
        EVP_PKEY_CTX_set_signature_md(signer, EVP_sha256()) > 0 &&
        EVP_PKEY_sign(signer, signature, signature_length,
                      digest, 32) > 0;
    EVP_PKEY_CTX_free(signer);
    if (!valid) return false;
    memcpy(public_key, fixture->public_key, 33);
    if (fixture->malformed) signature[0] = 0x31;
    return true;
}

static void initialize_review(blue_payment_apdu *state,
    uint8_t digest[32]) {
    memset(state, 0, sizeof *state);
    static const uint8_t message[] = "reviewed transparent input 0";
    assert(SHA256(message, sizeof message - 1, digest));
    state->review.verified = true;
    state->fee_ready = true;
    state->input_count = state->bound_inputs = 1;
    state->input_zat = 400000000;
    state->output_zat = 300000000;
    state->fee_zat = 100000000;
    state->input_paths = BLUE_PAYMENT_INPUT_EXTERNAL;
    memcpy(state->input_record[0], digest, 32);
    state->input_record[0][32] = BLUE_PAYMENT_INPUT_EXTERNAL;
}

static void test_signed_reply(signer_fixture *signer,
    const blue_payment_owned_hashes *owned) {
    blue_payment_apdu state;
    uint8_t digest[32], reply[BLUE_PAYMENT_SIGN_REPLY_MAX];
    initialize_review(&state, digest);
    assert(blue_payment_apdu_touch_approve(&state));
    size_t length = 0;
    assert(blue_payment_sign_next(&state, 0, owned, sign_digest, signer,
        hash_public_key, reply, sizeof reply, &length));
    assert(length >= 44 && length <= sizeof reply);
    assert(reply[0] == 0 && reply[1] == BLUE_PAYMENT_INPUT_EXTERNAL);
    assert(memcmp(reply + 2, signer->public_key, 33) == 0);
    assert(reply[35] == length - 36);
    uint8_t normalized[BLUE_ECDSA_DER_MAX];
    size_t normalized_length = 0;
    assert(blue_ecdsa_der_low_s(reply + 36, reply[35],
        normalized, &normalized_length));
    assert(normalized_length == reply[35]);
    assert(memcmp(normalized, reply + 36, normalized_length) == 0);
    EVP_PKEY_CTX *verify = EVP_PKEY_CTX_new(signer->key, NULL);
    assert(verify && EVP_PKEY_verify_init(verify) > 0);
    assert(EVP_PKEY_CTX_set_signature_md(verify, EVP_sha256()) > 0);
    assert(EVP_PKEY_verify(verify, reply + 36, reply[35],
                           digest, sizeof digest) == 1);
    EVP_PKEY_CTX_free(verify);
    assert(!state.approved && state.next_sign_index == 1);
    uint8_t zero[36] = {0};
    assert(memcmp(state.input_record[0], zero, sizeof zero) == 0);
    assert(!blue_payment_sign_next(&state, 0, owned, sign_digest, signer,
        hash_public_key, reply, sizeof reply, &length));
    assert(length == 0 && !state.fee_ready);
    assert(signer->calls == 1);
}

static void expect_failed_reply(const blue_payment_apdu *state,
    const uint8_t reply[BLUE_PAYMENT_SIGN_REPLY_MAX], size_t length) {
    assert(length == 0 && !state->fee_ready);
    for (size_t i = 0; i < BLUE_PAYMENT_SIGN_REPLY_MAX; ++i)
        assert(reply[i] == 0);
}

static void test_fail_closed(signer_fixture *signer,
    const blue_payment_owned_hashes *owned) {
    blue_payment_apdu state;
    uint8_t digest[32], reply[BLUE_PAYMENT_SIGN_REPLY_MAX];
    size_t length = 99;
    unsigned prior_calls = signer->calls;
    initialize_review(&state, digest);
    assert(!blue_payment_sign_next(&state, 0, owned, sign_digest, signer,
        hash_public_key, reply, sizeof reply, &length));
    expect_failed_reply(&state, reply, length);
    assert(signer->calls == prior_calls);
    initialize_review(&state, digest);
    assert(blue_payment_apdu_touch_approve(&state));
    assert(!blue_payment_sign_next(&state, 1, owned, sign_digest, signer,
        hash_public_key, reply, sizeof reply, &length));
    expect_failed_reply(&state, reply, length);
    assert(signer->calls == prior_calls);
    initialize_review(&state, digest);
    assert(blue_payment_apdu_touch_approve(&state));
    signer->refuse = true;
    assert(!blue_payment_sign_next(&state, 0, owned, sign_digest, signer,
        hash_public_key, reply, sizeof reply, &length));
    expect_failed_reply(&state, reply, length);
    signer->refuse = false;
    initialize_review(&state, digest);
    assert(blue_payment_apdu_touch_approve(&state));
    signer->malformed = true;
    assert(!blue_payment_sign_next(&state, 0, owned, sign_digest, signer,
        hash_public_key, reply, sizeof reply, &length));
    expect_failed_reply(&state, reply, length);
    signer->malformed = false;
    blue_payment_owned_hashes wrong = *owned;
    wrong.external[0] ^= 1;
    initialize_review(&state, digest);
    assert(blue_payment_apdu_touch_approve(&state));
    assert(!blue_payment_sign_next(&state, 0, &wrong, sign_digest, signer,
        hash_public_key, reply, sizeof reply, &length));
    expect_failed_reply(&state, reply, length);
}

static void test_sign_command(signer_fixture *signer,
    const blue_payment_owned_hashes *owned) {
    blue_payment_apdu state;
    uint8_t digest[32], frame[BLUE_PAYMENT_SIGN_REPLY_MAX] =
        {0xa5, 0x29, 0, 0, 1, 0};
    initialize_review(&state, digest);
    assert(blue_payment_apdu_touch_approve(&state));
    size_t length = 0;
    unsigned calls = signer->calls;
    assert(blue_payment_sign_command(&state, frame, 6, owned,
        sign_digest, signer, hash_public_key, frame, sizeof frame,
        &length) == 0x9000);
    assert(signer->calls == calls + 1 && length >= 44);
    assert(frame[0] == 0 && frame[1] == BLUE_PAYMENT_INPUT_EXTERNAL);
    EVP_PKEY_CTX *verify = EVP_PKEY_CTX_new(signer->key, NULL);
    assert(verify && EVP_PKEY_verify_init(verify) > 0);
    assert(EVP_PKEY_CTX_set_signature_md(verify, EVP_sha256()) > 0);
    assert(EVP_PKEY_verify(verify, frame + 36, frame[35],
                           digest, sizeof digest) == 1);
    EVP_PKEY_CTX_free(verify);
    static const uint8_t request[6] = {0xa5, 0x29, 0, 0, 1, 0};
    memcpy(frame, request, sizeof request);
    assert(blue_payment_sign_command(&state, frame, sizeof request, owned,
        sign_digest, signer, hash_public_key, frame, sizeof frame,
        &length) == 0x6985);
    assert(length == 0 && signer->calls == calls + 1);
    for (size_t i = 0; i < sizeof frame; ++i) assert(frame[i] == 0);
}

static void reject_sign_command(signer_fixture *signer,
    const blue_payment_owned_hashes *owned, const uint8_t request[6],
    size_t request_length, size_t capacity, uint16_t expected,
    bool approve) {
    blue_payment_apdu state;
    uint8_t digest[32], frame[BLUE_PAYMENT_SIGN_REPLY_MAX];
    initialize_review(&state, digest);
    if (approve) assert(blue_payment_apdu_touch_approve(&state));
    memset(frame, 0xcc, sizeof frame);
    memcpy(frame, request, 6);
    unsigned calls = signer->calls;
    size_t length = 99;
    assert(blue_payment_sign_command(&state, frame, request_length, owned,
        sign_digest, signer, hash_public_key, frame, capacity,
        &length) == expected);
    assert(calls == signer->calls && length == 0 && !state.fee_ready);
    for (size_t i = 0; i < capacity; ++i) assert(frame[i] == 0);
}

static void test_sign_command_rejections(signer_fixture *signer,
    const blue_payment_owned_hashes *owned) {
    const uint8_t valid[6] = {0xa5, 0x29, 0, 0, 1, 0};
    reject_sign_command(signer, owned, valid, 6, BLUE_PAYMENT_SIGN_REPLY_MAX,
        0x6985, false);
    reject_sign_command(signer, owned, valid, 5, BLUE_PAYMENT_SIGN_REPLY_MAX,
        0x6700, true);
    reject_sign_command(signer, owned, valid, 7, BLUE_PAYMENT_SIGN_REPLY_MAX,
        0x6700, true);
    reject_sign_command(signer, owned, valid, 6,
        BLUE_PAYMENT_SIGN_REPLY_MAX - 1, 0x6700, true);
    uint8_t malformed[6];
    memcpy(malformed, valid, sizeof malformed);
    malformed[4] = 0;
    reject_sign_command(signer, owned, malformed, 6,
        BLUE_PAYMENT_SIGN_REPLY_MAX, 0x6700, true);
    memcpy(malformed, valid, sizeof malformed);
    malformed[0] = 0xa4;
    reject_sign_command(signer, owned, malformed, 6,
        BLUE_PAYMENT_SIGN_REPLY_MAX, 0x6e00, true);
    memcpy(malformed, valid, sizeof malformed);
    malformed[1] = 0x28;
    reject_sign_command(signer, owned, malformed, 6,
        BLUE_PAYMENT_SIGN_REPLY_MAX, 0x6d00, true);
    memcpy(malformed, valid, sizeof malformed);
    malformed[2] = 1;
    reject_sign_command(signer, owned, malformed, 6,
        BLUE_PAYMENT_SIGN_REPLY_MAX, 0x6b00, true);
    memcpy(malformed, valid, sizeof malformed);
    malformed[3] = 1;
    reject_sign_command(signer, owned, malformed, 6,
        BLUE_PAYMENT_SIGN_REPLY_MAX, 0x6b00, true);
    memcpy(malformed, valid, sizeof malformed);
    malformed[5] = 1;
    reject_sign_command(signer, owned, malformed, 6,
        BLUE_PAYMENT_SIGN_REPLY_MAX, 0x6985, true);
}

static void test_wrong_indices(signer_fixture *signer,
    const blue_payment_owned_hashes *owned) {
    uint8_t request[6] = {0xa5, 0x29, 0, 0, 1, 0};
    for (unsigned index = 1; index <= UINT8_MAX; ++index) {
        request[5] = (uint8_t)index;
        reject_sign_command(signer, owned, request, sizeof request,
            BLUE_PAYMENT_SIGN_REPLY_MAX, 0x6985, true);
    }
}

static void test_malformed_command_fuzz(signer_fixture *signer,
    const blue_payment_owned_hashes *owned) {
    uint32_t random = 0x75c10d23;
    const uint8_t valid[6] = {0xa5, 0x29, 0, 0, 1, 0};
    for (unsigned trial = 0; trial < 10000; ++trial) {
        uint8_t request[8];
        for (size_t i = 0; i < sizeof request; ++i) {
            random = random * 1664525u + 1013904223u;
            request[i] = (uint8_t)(random >> 24);
        }
        size_t length = random % (sizeof request + 1);
        if (length == 6 && memcmp(request, valid, 6) == 0)
            request[5] = 1;
        blue_payment_apdu state;
        uint8_t digest[32], frame[BLUE_PAYMENT_SIGN_REPLY_MAX];
        initialize_review(&state, digest);
        assert(blue_payment_apdu_touch_approve(&state));
        memset(frame, 0xcc, sizeof frame);
        memcpy(frame, request, sizeof request);
        size_t reply_length = 99;
        unsigned calls = signer->calls;
        assert(blue_payment_sign_command(&state, frame, length, owned,
            sign_digest, signer, hash_public_key, frame, sizeof frame,
            &reply_length) != 0x9000);
        assert(reply_length == 0 && signer->calls == calls);
        assert(!state.fee_ready);
        for (size_t i = 0; i < sizeof frame; ++i)
            assert(frame[i] == 0);
    }
}

int main(void) {
    signer_fixture signer = {0};
    initialize_signer(&signer);
    blue_payment_owned_hashes owned = {0};
    assert(hash_public_key(signer.public_key, owned.external));
    test_signed_reply(&signer, &owned);
    test_fail_closed(&signer, &owned);
    test_sign_command(&signer, &owned);
    test_sign_command_rejections(&signer, &owned);
    test_wrong_indices(&signer, &owned);
    test_malformed_command_fuzz(&signer, &owned);
    EVP_PKEY_free(signer.key);
    return 0;
}
