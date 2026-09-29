/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
 * Bounded receiver for the fixed result.c verifier observation store.
 * This is a dev proof input reader, not an eligible expected-key producer. */
#ifndef ZCL_TOOLS_DEV_VERIFY_STORE_H
#define ZCL_TOOLS_DEV_VERIFY_STORE_H

#include "verify_attest.h"
#include "verify/fixed_result_key_v2.h"

#include <stddef.h>
#include <stdint.h>

enum zcl_verify_store_verdict {
    ZCL_VERIFY_STORE_COLD = 0,
    ZCL_VERIFY_STORE_HIT = 1,
    ZCL_VERIFY_STORE_BLOCK = 2,
};

struct zcl_verify_store_result {
    enum zcl_verify_store_verdict verdict;
    const char *reason;
    uint8_t *object;
    size_t object_len;
    uint8_t *depfile;
    size_t depfile_len;
    uint8_t *stderr_bytes;
    size_t stderr_len;
    char store_key[ZCL_VERIFY_ATTEST_STORE_KEY_HEX];
    char record_sha3[ZCL_VERIFY_ATTEST_STORE_KEY_HEX];
    uint8_t verifier_pubkey[ZCL_VERIFY_ATTEST_PUBKEY_BYTES];
    /* On HIT, the shared publication lock remains held until release. A
     * caller must materialize the verified object before releasing it. */
    int lock_fd;
};

/* Production paths, signer UID, and root publisher custody come only from
 * root-owned policy. This function reloads that policy and the root-pinned
 * public key on every call, and refuses store_owner_same_uid when the
 * receiver runs as the signer or the publisher.
 * The caller must independently construct `expected` from an eligible input
 * closure and pass the root-owned pins v2 it loaded itself; every
 * observation's launch.bin receipt is bound against them
 * (zcl_fr_receipt_bind) before its record can admit. This reader cannot
 * make a probe-only closure eligible. */
void zcl_verify_store_lookup(const struct zcl_verify_attest_expected *expected,
                             const struct zcl_fixed_result_v2_roots *pins,
                             const struct zcl_verify_attest_box_key *box,
                             struct zcl_verify_store_result *out);

void zcl_verify_store_result_release(struct zcl_verify_store_result *result);

#ifdef ZCL_TESTING
/* Isolated fixture only. Production does not compile a path/UID override.
 * Without allow_same_uid, a signer or publisher UID equal to the caller's
 * refuses store_owner_same_uid before any file is opened. */
void zcl_verify_store_lookup_fixture(
    const char *root, unsigned signer_uid, unsigned publisher_uid,
    bool allow_same_uid,
    const struct zcl_verify_attest_expected *expected,
    const struct zcl_fixed_result_v2_roots *pins,
    const struct zcl_verify_attest_box_key *box,
    struct zcl_verify_store_result *out);
#endif

#endif
