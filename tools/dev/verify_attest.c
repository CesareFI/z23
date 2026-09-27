/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: RED placeholder for separate-account compile attestations. */

#include "verify_attest.h"

#include <string.h>

bool zcl_verify_attest_body_encode(const struct zcl_verify_attest_record *record,
                                   uint8_t **out, size_t *out_len,
                                   const char **why)
{
    (void)record;
    if (out) *out = NULL;
    if (out_len) *out_len = 0;
    if (why) *why = ZCL_VERIFY_ATTEST_WHY_ARGUMENTS;
    return false;
}

bool zcl_verify_attest_seal(const struct zcl_verify_attest_record *record,
                            const uint8_t seed[ZCL_VERIFY_ATTEST_SEED_BYTES],
                            uint8_t **out, size_t *out_len, const char **why)
{
    (void)seed;
    return zcl_verify_attest_body_encode(record, out, out_len, why);
}

bool zcl_verify_attest_parse(const uint8_t *bytes, size_t len,
                             struct zcl_verify_attest_signed *out,
                             const char **why)
{
    (void)bytes;
    (void)len;
    (void)out;
    if (why) *why = ZCL_VERIFY_ATTEST_WHY_ARGUMENTS;
    return false;
}

void zcl_verify_attest_store_key(
    const struct zcl_verify_attest_text *toolchain_id,
    const struct zcl_verify_attest_text *argv_norm,
    const uint8_t pp_sha3[ZCL_VERIFY_ATTEST_HASH_BYTES],
    uint8_t out[ZCL_VERIFY_ATTEST_HASH_BYTES])
{
    (void)toolchain_id;
    (void)argv_norm;
    (void)pp_sha3;
    memset(out, 0, ZCL_VERIFY_ATTEST_HASH_BYTES);
}

void zcl_verify_attest_store_key_hex(
    const struct zcl_verify_attest_text *toolchain_id,
    const struct zcl_verify_attest_text *argv_norm,
    const uint8_t pp_sha3[ZCL_VERIFY_ATTEST_HASH_BYTES],
    char out[ZCL_VERIFY_ATTEST_STORE_KEY_HEX])
{
    (void)toolchain_id;
    (void)argv_norm;
    (void)pp_sha3;
    out[0] = 0;
}

const char *zcl_verify_attest_path_check(
    const struct zcl_verify_attest_path_entry *entries, size_t count,
    const struct zcl_verify_attest_path_policy *policy)
{
    (void)entries;
    (void)count;
    (void)policy;
    return ZCL_VERIFY_ATTEST_WHY_ARGUMENTS;
}

bool zcl_verify_attest_pubkey_parse(
    const uint8_t *bytes, size_t len,
    uint8_t out[ZCL_VERIFY_ATTEST_PUBKEY_BYTES])
{
    (void)bytes;
    (void)len;
    memset(out, 0, ZCL_VERIFY_ATTEST_PUBKEY_BYTES);
    return false;
}

bool zcl_verify_attest_trust_root_load(
    const char *configured_path, const struct zcl_verify_attest_box_key *box,
    struct zcl_verify_attest_trust_root *out, const char **why)
{
    (void)configured_path;
    (void)box;
    if (out) memset(out, 0, sizeof(*out));
    if (why) *why = ZCL_VERIFY_ATTEST_WHY_ARGUMENTS;
    return false;
}

bool zcl_verify_attest_test_override_compiled(void)
{
    return false;
}

struct zcl_verify_attest_decision zcl_verify_attest_admit(
    const uint8_t *record_bytes, size_t record_len,
    const uint8_t *obj_bytes, size_t obj_len,
    const struct zcl_verify_attest_expected *expected,
    const struct zcl_verify_attest_trust_root *trust_root)
{
    (void)record_bytes;
    (void)record_len;
    (void)obj_bytes;
    (void)obj_len;
    (void)expected;
    (void)trust_root;
    struct zcl_verify_attest_decision d = {ZCL_VERIFY_ATTEST_REFUSE,
                                           ZCL_VERIFY_ATTEST_WHY_ARGUMENTS};
    return d;
}
