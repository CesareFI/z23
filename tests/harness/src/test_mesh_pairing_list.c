/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: contract coverage for mesh_pairing_service_list() and
 * mesh_pairing_service_list_after() — the redacted owner view through
 * which pairing state leaves the service. The load-bearing contract:
 * raw ZID master and Noise public keys NEVER cross this boundary; the
 * view carries only domain-separated SHA3 fingerprints. State
 * classification (revoked beats expired beats active), the counts
 * tally, the max clamp, and paging had no assertions at all. */

#include "test/test_core.h"

#include "services/mesh_pairing_service.h"
#include "models/mesh_pairing.h"
#include "crypto/sha3.h"
#include "net/v2_identity.h"
#include "base/hex.h"

#include <string.h>

#define MPL_NOW 1000000LL

static bool mpl_insert(struct node_db *ndb, uint8_t identity,
                       int64_t expires_at, int64_t revoked_at,
                       uint8_t noise_fill, struct db_mesh_pairing *out)
{
    memset(out, 0, sizeof(*out));
    memset(out->network_genesis, 0x11, 32);
    memset(out->peer_master_pubkey, identity, 32);
    memset(out->peer_noise_pubkey, noise_fill, 32);
    out->capability_mask = MESH_PAIRING_CAP_STATUS_READ;
    out->delegation_sequence = 1;
    out->paired_at = MPL_NOW - 1000;
    out->expires_at = expires_at;
    out->revoked_at = revoked_at;
    if (revoked_at != 0)
        out->revocation_generation = 1;
    return mesh_pairing_id_derive(out->network_genesis,
                                  out->peer_master_pubkey,
                                  out->peer_noise_pubkey,
                                  out->pairing_id) &&
           db_mesh_pairing_insert(ndb, out);
}

/* True when the 32-byte needle appears nowhere in the view's bytes —
 * the redaction guarantee, checked directly. */
static bool mpl_view_hides(const struct mesh_pairing_public_view *view,
                           const uint8_t needle[32])
{
    const uint8_t *hay = (const uint8_t *)view;
    for (size_t i = 0; i + 32 <= sizeof(*view); i++)
        if (memcmp(hay + i, needle, 32) == 0)
            return false;
    return true;
}

static void mpl_expected_master(uint8_t identity, char out[65])
{
    static const char domain[] = "zcl.mesh.master.fingerprint.v1";
    uint8_t key[32];
    memset(key, identity, sizeof(key));
    struct sha3_256_ctx ctx;
    uint8_t digest[32];
    sha3_256_init(&ctx);
    sha3_256_write(&ctx, (const uint8_t *)domain, strlen(domain));
    sha3_256_write(&ctx, key, 32);
    sha3_256_finalize(&ctx, digest);
    zcl_hex_encode(digest, sizeof(digest), out);
}

static int test_mpl_refusal_matrix(void)
{
    int failures = 0;
    TEST("mesh pairing list: invalid arguments refuse with count zeroed") {
        struct node_db ndb;
        memset(&ndb, 0, sizeof(ndb));
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct mesh_pairing_public_view view[1];
        struct db_mesh_pairing_counts counts;
        size_t count = 77; /* poison: must be reset before validation */
        bool ok = !mesh_pairing_service_list(NULL, MPL_NOW, view, 1,
                                             &count, &counts) &&
                  !mesh_pairing_service_list(&ndb, 0, view, 1, &count,
                                             &counts) &&
                  !mesh_pairing_service_list(&ndb, MPL_NOW, NULL, 1, &count,
                                             &counts) &&
                  !mesh_pairing_service_list(&ndb, MPL_NOW, view, 0, &count,
                                             &counts) &&
                  !mesh_pairing_service_list(&ndb, MPL_NOW, view, 1, NULL,
                                             &counts) &&
                  !mesh_pairing_service_list(&ndb, MPL_NOW, view, 1, &count,
                                             NULL) &&
                  count == 0;
        node_db_close(&ndb);
        ASSERT(ok);
        PASS();
    } _test_next:;
    return failures;
}

static int test_mpl_empty_store(void)
{
    int failures = 0;
    TEST("mesh pairing list: an empty store lists cleanly") {
        struct node_db ndb;
        memset(&ndb, 0, sizeof(ndb));
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct mesh_pairing_public_view view[1];
        struct db_mesh_pairing_counts counts;
        memset(&counts, 0xA5, sizeof(counts));
        size_t count = 99;
        ASSERT(mesh_pairing_service_list(&ndb, MPL_NOW, view, 1, &count,
                                         &counts));
        ASSERT(count == 0 && counts.total == 0 && counts.active == 0);
        node_db_close(&ndb);
        PASS();
    } _test_next:;
    return failures;
}

static int test_mpl_redaction_fingerprints(void)
{
    int failures = 0;
    TEST("mesh pairing list: raw keys never leave; fingerprints match") {
        struct node_db ndb;
        memset(&ndb, 0, sizeof(ndb));
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct db_mesh_pairing row;
        ASSERT(mpl_insert(&ndb, 0x42, MPL_NOW + 5000, 0, 0x43, &row));
        struct mesh_pairing_public_view view[1];
        struct db_mesh_pairing_counts counts;
        size_t count = 0;
        ASSERT(mesh_pairing_service_list(&ndb, MPL_NOW, view, 1, &count,
                                         &counts));
        ASSERT(count == 1);
        ASSERT(strcmp(view[0].pairing_id, row.pairing_id) == 0);
        char expected_master[65];
        mpl_expected_master(0x42, expected_master);
        ASSERT(strcmp(view[0].peer_master_fingerprint,
                      expected_master) == 0);
        uint8_t noise_fp[32];
        ASSERT(v2_identity_public_fingerprint(row.peer_noise_pubkey,
                                              noise_fp));
        char expected_noise[65];
        zcl_hex_encode(noise_fp, sizeof(noise_fp), expected_noise);
        ASSERT(strcmp(view[0].peer_noise_fingerprint,
                      expected_noise) == 0);
        /* The redaction guarantee itself. */
        ASSERT(mpl_view_hides(&view[0], row.peer_master_pubkey));
        ASSERT(mpl_view_hides(&view[0], row.peer_noise_pubkey));
        ASSERT(strcmp(view[0].state, "active") == 0);
        node_db_close(&ndb);
        PASS();
    } _test_next:;
    return failures;
}

static int test_mpl_state_classification_and_counts(void)
{
    int failures = 0;
    TEST("mesh pairing list: revoked beats expired beats active") {
        struct node_db ndb;
        memset(&ndb, 0, sizeof(ndb));
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct db_mesh_pairing row;
        ASSERT(mpl_insert(&ndb, 0x21, MPL_NOW + 5000, 0, 0x22, &row));
        ASSERT(mpl_insert(&ndb, 0x22, MPL_NOW - 10, 0, 0x23, &row));
        /* revoked AND past expiry: revocation must win */
        ASSERT(mpl_insert(&ndb, 0x23, MPL_NOW - 10, MPL_NOW - 5, 0x24, &row));
        struct mesh_pairing_public_view view[4];
        struct db_mesh_pairing_counts counts;
        size_t count = 0;
        ASSERT(mesh_pairing_service_list(&ndb, MPL_NOW, view, 4, &count,
                                         &counts));
        ASSERT(count == 3);
        ASSERT(counts.total == 3 && counts.active == 1 &&
               counts.expired == 1 && counts.revoked == 1);
        int seen_active = 0, seen_expired = 0, seen_revoked = 0;
        for (size_t i = 0; i < count; i++) {
            if (strcmp(view[i].state, "active") == 0) seen_active++;
            if (strcmp(view[i].state, "expired") == 0) seen_expired++;
            if (strcmp(view[i].state, "revoked") == 0) seen_revoked++;
        }
        ASSERT(seen_active == 1 && seen_expired == 1 && seen_revoked == 1);
        node_db_close(&ndb);
        PASS();
    } _test_next:;
    return failures;
}

static int test_mpl_paging(void)
{
    int failures = 0;
    TEST("mesh pairing list: max page and skip paging") {
        struct node_db ndb;
        memset(&ndb, 0, sizeof(ndb));
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct db_mesh_pairing r1, r2;
        ASSERT(mpl_insert(&ndb, 0x31, MPL_NOW + 5000, 0, 0x32, &r1));
        ASSERT(mpl_insert(&ndb, 0x32, MPL_NOW + 5000, 0, 0x33, &r2));
        struct mesh_pairing_public_view view[4];
        struct db_mesh_pairing_counts counts;
        size_t count = 0;
        ASSERT(mesh_pairing_service_list(&ndb, MPL_NOW, view, 1, &count,
                                         &counts));
        ASSERT(count == 1);
        char first_id[MESH_PAIRING_ID_HEX + 1];
        memcpy(first_id, view[0].pairing_id, sizeof(first_id));
        ASSERT(mesh_pairing_service_list_after(&ndb, MPL_NOW, 1, view, 4,
                                               &count, &counts));
        ASSERT(count == 1);
        ASSERT(strcmp(view[0].pairing_id, first_id) != 0);
        node_db_close(&ndb);
        PASS();
    } _test_next:;
    return failures;
}

int test_mesh_pairing_list(void)
{
    int failures = 0;
    failures += test_mpl_refusal_matrix();
    failures += test_mpl_empty_store();
    failures += test_mpl_redaction_fingerprints();
    failures += test_mpl_state_classification_and_counts();
    failures += test_mpl_paging();
    printf("=== mesh_pairing_list: %d failures ===\n", failures);
    return failures;
}
