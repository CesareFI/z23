/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * Hermetic proof that wallet metadata is AEAD-encrypted under a wrapped DEK. */

#include "test/test_core.h"

#include "models/database.h"
#include "models/wallet_metadata_crypto.h"
#include "wallet/wallet_lock.h"

#include <sqlite3.h>
#include <string.h>

static bool wme_contains(const uint8_t *hay, size_t hlen,
                         const uint8_t *needle, size_t nlen)
{
    if (!hay || !needle || nlen == 0 || hlen < nlen)
        return false;
    for (size_t i = 0; i + nlen <= hlen; i++)
        if (memcmp(hay + i, needle, nlen) == 0)
            return true;
    return false;
}

int test_wallet_metadata_encryption(void)
{
    int failures = 0;
    struct node_db ndb;
    memset(&ndb, 0, sizeof(ndb));

    TEST("wallet metadata encrypts per row and rejects lock/tamper/wrong pass") {
        ASSERT(node_db_open(&ndb, ":memory:"));
        ASSERT(ndb.open);
        wallet_lock_reset_for_test();
        wallet_lock_note_encrypted_at_rest();
        ASSERT(wallet_lock_unlock(NULL, NULL, "metadata-test-pass").ok);

        const uint8_t aad[WALLET_METADATA_AAD_LEN] = {0x42};
        const uint8_t plaintext[] =
            "zs1-secret-recipient|invoice-body|private-memo";
        uint8_t first[256], second[256], opened[256];
        size_t first_len = 0, second_len = 0, opened_len = 0;
        ASSERT(wallet_metadata_encrypt(&ndb, aad, sizeof(aad), plaintext,
            sizeof(plaintext) - 1, first, sizeof(first), &first_len));
        ASSERT(wallet_metadata_encrypt(&ndb, aad, sizeof(aad), plaintext,
            sizeof(plaintext) - 1, second, sizeof(second), &second_len));
        ASSERT_EQ(first_len, second_len);
        ASSERT(memcmp(first, second, first_len) != 0);
        ASSERT(!wme_contains(first, first_len, plaintext,
                             sizeof(plaintext) - 1));
        ASSERT(wallet_metadata_decrypt(&ndb, aad, sizeof(aad), first,
            first_len, opened, sizeof(opened), &opened_len));
        ASSERT_EQ(opened_len, sizeof(plaintext) - 1);
        ASSERT(memcmp(opened, plaintext, opened_len) == 0);

        uint8_t large[4096], large_envelope[4096 + WALLET_METADATA_OVERHEAD];
        uint8_t large_opened[4096];
        size_t large_envelope_len = 0, large_opened_len = 0;
        for (size_t i = 0; i < sizeof(large); i++)
            large[i] = (uint8_t)(i * 37u + 11u);
        ASSERT(wallet_metadata_encrypt(&ndb, aad, sizeof(aad), large,
            sizeof(large), large_envelope, sizeof(large_envelope),
            &large_envelope_len));
        ASSERT(wallet_metadata_decrypt(&ndb, aad, sizeof(aad), large_envelope,
            large_envelope_len, large_opened, sizeof(large_opened),
            &large_opened_len));
        ASSERT_EQ(large_opened_len, sizeof(large));
        ASSERT(memcmp(large, large_opened, sizeof(large)) == 0);

        wallet_lock_lock(NULL);
        ASSERT(!wallet_metadata_decrypt(&ndb, aad, sizeof(aad), first,
            first_len, opened, sizeof(opened), &opened_len));
        ASSERT(wallet_lock_unlock(NULL, NULL, "wrong-pass").ok);
        ASSERT(!wallet_metadata_decrypt(&ndb, aad, sizeof(aad), first,
            first_len, opened, sizeof(opened), &opened_len));
        wallet_lock_lock(NULL);
        ASSERT(wallet_lock_unlock(NULL, NULL, "metadata-test-pass").ok);

        uint8_t aad_wide[WALLET_METADATA_AAD_LEN + 1] = {0x42};
        uint8_t refused[256];
        memset(refused, 0xa5, sizeof(refused));
        size_t refused_len = SIZE_MAX;
        ASSERT(!wallet_metadata_encrypt(&ndb, aad, sizeof(aad) - 1,
            plaintext, sizeof(plaintext) - 1, refused, sizeof(refused),
            &refused_len));
        ASSERT_EQ(refused_len, 0);
        refused_len = SIZE_MAX;
        ASSERT(!wallet_metadata_encrypt(&ndb, aad_wide, sizeof(aad_wide),
            plaintext, sizeof(plaintext) - 1, refused, sizeof(refused),
            &refused_len));
        ASSERT_EQ(refused_len, 0);
        refused_len = SIZE_MAX;
        ASSERT(!wallet_metadata_encrypt(&ndb, aad, SIZE_MAX, plaintext,
            sizeof(plaintext) - 1, refused, sizeof(refused), &refused_len));
        ASSERT_EQ(refused_len, 0);
        refused_len = SIZE_MAX;
        ASSERT(!wallet_metadata_decrypt(&ndb, aad, sizeof(aad) - 1, first,
            first_len, refused, sizeof(refused), &refused_len));
        ASSERT_EQ(refused_len, 0);
        refused_len = SIZE_MAX;
        ASSERT(!wallet_metadata_decrypt(&ndb, aad_wide, sizeof(aad_wide),
            first, first_len, refused, sizeof(refused), &refused_len));
        ASSERT_EQ(refused_len, 0);
        refused_len = SIZE_MAX;
        ASSERT(!wallet_metadata_decrypt(&ndb, aad, SIZE_MAX, first,
            first_len, refused, sizeof(refused), &refused_len));
        ASSERT_EQ(refused_len, 0);
        for (size_t i = 0; i < sizeof(refused); i++)
            ASSERT_EQ(refused[i], 0xa5);

        first[first_len - 1] ^= 1;
        ASSERT(!wallet_metadata_decrypt(&ndb, aad, sizeof(aad), first,
            first_len, opened, sizeof(opened), &opened_len));
        wallet_lock_lock(NULL);
        node_db_close(&ndb);
        PASS();
    }

_test_next:;
    if (ndb.open) node_db_close(&ndb);
    wallet_lock_reset_for_test();
    return failures;
}
