/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
 * Purpose: prove private-key ownership verification fails closed when entropy
 * or signing refuses and retires its random challenge on every path. */
#include "crypto/random_secret.h"
#include "keys/key.h"
#include "support/cleanse.h"

#include <secp256k1.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool fixture_sign_succeeds;
static bool fixture_random_succeeds;
static unsigned fixture_public_verify_calls;
static unsigned fixture_challenge_wipes;
static bool fixture_challenge_erased;
static bool fixture_challenge_retired_before_verify;

static bool fixture_random(unsigned char *bytes, size_t length,
                           const char *label)
{
    (void)label;
    if (length != 8) return false;
    memset(bytes, 0xa5, length);
    return fixture_random_succeeds;
}

static void fixture_cleanse(void *bytes, size_t length)
{
    bool populated = false;
    const unsigned char *span = bytes;
    for (size_t i = 0; i < length; i++)
        populated = populated || span[i] != 0;
    memory_cleanse(bytes, length);
    if (length != 8 || !populated) return;
    fixture_challenge_wipes++;
    fixture_challenge_erased = true;
    for (size_t i = 0; i < length; i++)
        fixture_challenge_erased = fixture_challenge_erased && span[i] == 0;
}

static int fixture_sign(const secp256k1_context *context,
                        secp256k1_ecdsa_signature *signature,
                        const unsigned char *hash,
                        const unsigned char *secret,
                        secp256k1_nonce_function nonce,
                        const void *nonce_data)
{
    (void)context;
    (void)hash;
    (void)secret;
    (void)nonce;
    (void)nonce_data;
    memset(signature, 0x5a, sizeof(*signature));
    return fixture_sign_succeeds ? 1 : 0;
}

static int fixture_serialize(const secp256k1_context *context,
                             unsigned char *output, size_t *length,
                             const secp256k1_ecdsa_signature *signature)
{
    (void)context;
    (void)signature;
    if (!output || !length || *length == 0) return 0;
    output[0] = 0x30;
    *length = 1;
    return 1;
}

static bool fixture_pubkey_verify(const struct pubkey *key,
                                  const struct uint256 *hash,
                                  const unsigned char *signature,
                                  size_t length)
{
    (void)key;
    (void)hash;
    (void)signature;
    (void)length;
    fixture_public_verify_calls++;
    fixture_challenge_retired_before_verify =
        fixture_challenge_wipes == 1 && fixture_challenge_erased;
    return true;
}

#define memory_cleanse fixture_cleanse
#define zcl_random_secret_bytes fixture_random
#define secp256k1_ecdsa_sign fixture_sign
#define secp256k1_ecdsa_signature_serialize_der fixture_serialize
#define pubkey_verify fixture_pubkey_verify
#define privkey_make_new verify_fixture_privkey_make_new
#define privkey_range_check verify_fixture_privkey_range_check
#define privkey_get_pubkey verify_fixture_privkey_get_pubkey
#define privkey_sign verify_fixture_privkey_sign
#define privkey_sign_compact verify_fixture_privkey_sign_compact
#define privkey_verify_pubkey verify_fixture_privkey_verify_pubkey
#define privkey_derive verify_fixture_privkey_derive
#define ext_key_encode verify_fixture_ext_key_encode
#define ext_key_decode verify_fixture_ext_key_decode
#define ext_key_derive verify_fixture_ext_key_derive
#define ext_key_set_master verify_fixture_ext_key_set_master
#define ext_key_neuter verify_fixture_ext_key_neuter
#define ecc_init_sanity_check verify_fixture_ecc_init_sanity_check
#define ecc_start verify_fixture_ecc_start
#define ecc_start_once verify_fixture_ecc_start_once
#define ecc_stop verify_fixture_ecc_stop
#include "../../../contexts/wallet/modules/keys/src/key.c"
#undef ecc_stop
#undef ecc_start_once
#undef ecc_start
#undef ecc_init_sanity_check
#undef ext_key_neuter
#undef ext_key_set_master
#undef ext_key_derive
#undef ext_key_decode
#undef ext_key_encode
#undef privkey_derive
#undef privkey_verify_pubkey
#undef privkey_sign_compact
#undef privkey_sign
#undef privkey_get_pubkey
#undef privkey_range_check
#undef privkey_make_new
#undef pubkey_verify
#undef secp256k1_ecdsa_signature_serialize_der
#undef secp256k1_ecdsa_sign
#undef zcl_random_secret_bytes
#undef memory_cleanse

static void fixture_reset(bool random_succeeds, bool sign_succeeds)
{
    fixture_random_succeeds = random_succeeds;
    fixture_sign_succeeds = sign_succeeds;
    fixture_public_verify_calls = 0;
    fixture_challenge_wipes = 0;
    fixture_challenge_erased = false;
    fixture_challenge_retired_before_verify = false;
}

static bool verification_case(bool sign_succeeds)
{
    struct privkey secret = {0};
    secret.fValid = true;
    secret.fCompressed = true;
    memset(secret.vch, 0x42, sizeof(secret.vch));
    struct pubkey public_key = {0};
    public_key.size = COMPRESSED_PUBLIC_KEY_SIZE;
    public_key.vch[0] = 0x02;

    fixture_reset(true, sign_succeeds);
    bool verified = verify_fixture_privkey_verify_pubkey(&secret, &public_key);
    memory_cleanse(&secret, sizeof(secret));
    if (!sign_succeeds)
        return !verified && fixture_public_verify_calls == 0 &&
               fixture_challenge_wipes == 1 && fixture_challenge_erased;
    return verified && fixture_public_verify_calls == 1 &&
           fixture_challenge_retired_before_verify;
}

static bool random_failure_case(void)
{
    struct privkey secret = {0};
    secret.fValid = true;
    secret.fCompressed = true;
    memset(secret.vch, 0x42, sizeof(secret.vch));
    struct pubkey public_key = {0};
    public_key.size = COMPRESSED_PUBLIC_KEY_SIZE;
    public_key.vch[0] = 0x02;

    fixture_reset(false, true);
    bool verified = verify_fixture_privkey_verify_pubkey(&secret, &public_key);
    memory_cleanse(&secret, sizeof(secret));
    return !verified && fixture_public_verify_calls == 0 &&
           fixture_challenge_wipes == 1 && fixture_challenge_erased;
}

int wallet_pubkey_verify_failure_cases(void);
int wallet_pubkey_verify_failure_cases(void)
{
    bool okay = random_failure_case() && verification_case(false) &&
                verification_case(true);
    printf("domain_wallet_key_derivation: signing refusal and challenge retirement... %s\n",
           okay ? "OK" : "FAIL");
    return okay ? 0 : 1;
}
