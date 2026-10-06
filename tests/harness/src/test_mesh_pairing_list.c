/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: contract coverage for mesh_pairing_service_list() and
 * mesh_pairing_service_list_after() — the redacted owner view through
 * which pairing state leaves the service. The load-bearing contract:
 * raw ZID master and Noise public keys NEVER cross this boundary; the
 * view carries only domain-separated SHA3 fingerprints. State
 * classification (revoked beats expired beats active), the counts
 * tally, the max clamp, and paging are pinned with deterministic fixtures. */

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

/* Exercise both service entry points with a fresh poison for each refusal. */
static bool mpl_list(bool paged, struct node_db *ndb, int64_t now,
                     struct mesh_pairing_public_view *view, size_t max,
                     size_t *count, struct db_mesh_pairing_counts *counts)
{
    if (paged)
        return mesh_pairing_service_list_after(ndb, now, 0, view, max,
                                               count, counts);
    return mesh_pairing_service_list(ndb, now, view, max, count, counts);
}

static int test_mpl_refusal_matrix(bool paged)
{
    int failures = 0;
    struct node_db ndb = {0};
    struct node_db closed = {0};
    TEST(paged ? "mesh pairing list_after: each refusal resets count" :
                 "mesh pairing list: each refusal resets count") {
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct mesh_pairing_public_view view[1];
        struct db_mesh_pairing_counts counts;
        size_t count = 77;
        const struct {
            struct node_db *db;
            int64_t now;
            struct mesh_pairing_public_view *out;
            size_t max;
            size_t *count;
            struct db_mesh_pairing_counts *counts;
        } cases[] = {
            {NULL, MPL_NOW, view, 1, &count, &counts},
            {&closed, MPL_NOW, view, 1, &count, &counts},
            {&ndb, 0, view, 1, &count, &counts},
            {&ndb, -1, view, 1, &count, &counts},
            {&ndb, MPL_NOW, NULL, 1, &count, &counts},
            {&ndb, MPL_NOW, view, 0, &count, &counts},
            {&ndb, MPL_NOW, view, 1, NULL, &counts},
            {&ndb, MPL_NOW, view, 1, &count, NULL},
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            count = 77;
            ASSERT(!mpl_list(paged, cases[i].db, cases[i].now,
                             cases[i].out, cases[i].max, cases[i].count,
                             cases[i].counts));
            if (cases[i].count)
                ASSERT_EQ(count, 0);
        }
        PASS();
    } _test_next:;
    if (ndb.open)
        node_db_close(&ndb);
    return failures;
}

static int test_mpl_empty_store(void)
{
    int failures = 0;
    struct node_db ndb = {0};
    TEST("mesh pairing list: an empty store lists cleanly") {
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct mesh_pairing_public_view view[1];
        struct db_mesh_pairing_counts counts;
        memset(&counts, 0xA5, sizeof(counts));
        size_t count = 99;
        ASSERT(mesh_pairing_service_list(&ndb, MPL_NOW, view, 1, &count,
                                         &counts));
        ASSERT(count == 0 && counts.total == 0 && counts.active == 0);
        ASSERT(counts.expired == 0 && counts.revoked == 0);
        count = 99;
        memset(&counts, 0xA5, sizeof(counts));
        ASSERT(mesh_pairing_service_list_after(&ndb, MPL_NOW, 0, view, 1,
                                               &count, &counts));
        ASSERT_EQ(count, 0);
        ASSERT(counts.total == 0 && counts.active == 0 &&
               counts.expired == 0 && counts.revoked == 0);
        PASS();
    } _test_next:;
    if (ndb.open)
        node_db_close(&ndb);
    return failures;
}

static int test_mpl_redaction_fingerprints(bool paged)
{
    int failures = 0;
    struct node_db ndb = {0};
    TEST(paged ? "mesh pairing list_after: raw keys never leave; fingerprints match" :
                 "mesh pairing list: raw keys never leave; fingerprints match") {
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct db_mesh_pairing row;
        ASSERT(mpl_insert(&ndb, 0x42, MPL_NOW + 5000, 0, 0x43, &row));
        struct mesh_pairing_public_view view[1];
        struct db_mesh_pairing_counts counts;
        size_t count = 0;
        ASSERT(mpl_list(paged, &ndb, MPL_NOW, view, 1, &count,
                                         &counts));
        ASSERT(count == 1);
        ASSERT(strcmp(view[0].pairing_id, row.pairing_id) == 0);
        ASSERT(mpl_view_hides(&view[0], row.peer_master_pubkey));
        ASSERT(mpl_view_hides(&view[0], row.peer_noise_pubkey));
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
        ASSERT(strcmp(view[0].state, "active") == 0);
        PASS();
    } _test_next:;
    if (ndb.open)
        node_db_close(&ndb);
    return failures;
}

static int test_mpl_state_classification_and_counts(bool paged)
{
    int failures = 0;
    struct node_db ndb = {0};
    TEST(paged ? "mesh pairing list_after: revoked beats expired beats active" :
                 "mesh pairing list: revoked beats expired beats active") {
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct db_mesh_pairing row;
        ASSERT(mpl_insert(&ndb, 0x21, MPL_NOW + 5000, 0, 0x22, &row));
        ASSERT(mpl_insert(&ndb, 0x22, MPL_NOW, 0, 0x23, &row));
        /* revoked AND past expiry: revocation must win */
        ASSERT(mpl_insert(&ndb, 0x23, MPL_NOW - 10, MPL_NOW - 5, 0x24, &row));
        struct mesh_pairing_public_view view[4];
        struct db_mesh_pairing_counts counts;
        size_t count = 0;
        ASSERT(mpl_list(paged, &ndb, MPL_NOW, view, 4, &count,
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
        PASS();
    } _test_next:;
    if (ndb.open)
        node_db_close(&ndb);
    return failures;
}

static int test_mpl_paging(void)
{
    int failures = 0;
    struct node_db ndb = {0};
    TEST("mesh pairing list: max page and skip paging") {
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
        ASSERT(strcmp(view[0].pairing_id, first_id) > 0);
        const char *low = r1.pairing_id;
        const char *high = r2.pairing_id;
        if (strcmp(low, high) > 0) {
            low = r2.pairing_id;
            high = r1.pairing_id;
        }
        ASSERT_STR_EQ(first_id, low);
        ASSERT_STR_EQ(view[0].pairing_id, high);
        ASSERT_EQ(counts.total, 2);
        count = 99;
        ASSERT(mesh_pairing_service_list_after(&ndb, MPL_NOW, 2, view, 4,
                                               &count, &counts));
        ASSERT_EQ(count, 0);
        ASSERT_EQ(counts.total, 2);
        PASS();
    } _test_next:;
    if (ndb.open)
        node_db_close(&ndb);
    return failures;
}

/* A 65th valid row distinguishes the clamp from the caller's page limit. */
static int test_mpl_max_clamp(bool paged)
{
    int failures = 0;
    struct node_db ndb = {0};
    TEST(paged ? "mesh pairing list_after: clamps max and preserves tail" :
                 "mesh pairing list: clamps max and preserves tail") {
        ASSERT(node_db_open(&ndb, ":memory:"));
        struct db_mesh_pairing row;
        for (size_t i = 0; i <= MESH_PAIRING_LIST_MAX; i++)
            ASSERT(mpl_insert(&ndb, (uint8_t)(i + 1), MPL_NOW + 5000,
                              0, 0x76, &row));
        struct mesh_pairing_public_view view[MESH_PAIRING_LIST_MAX + 1];
        struct mesh_pairing_public_view sentinel;
        memset(view, 0xA5, sizeof(view));
        memcpy(&sentinel, &view[MESH_PAIRING_LIST_MAX], sizeof(sentinel));
        struct db_mesh_pairing_counts counts;
        size_t count = 99;
        ASSERT(mpl_list(paged, &ndb, MPL_NOW, view,
                        MESH_PAIRING_LIST_MAX + 1, &count, &counts));
        ASSERT_EQ(count, MESH_PAIRING_LIST_MAX);
        ASSERT_EQ(counts.total, MESH_PAIRING_LIST_MAX + 1);
        ASSERT_EQ(counts.active, MESH_PAIRING_LIST_MAX + 1);
        ASSERT(counts.expired == 0 && counts.revoked == 0);
        ASSERT(memcmp(&view[MESH_PAIRING_LIST_MAX], &sentinel,
                      sizeof(sentinel)) == 0);
        for (size_t i = 1; i < count; i++)
            ASSERT(strcmp(view[i - 1].pairing_id, view[i].pairing_id) < 0);
        PASS();
    } _test_next:;
    if (ndb.open)
        node_db_close(&ndb);
    return failures;
}

int test_mesh_pairing_list(void)
{
    int failures = 0;
    failures += test_mpl_refusal_matrix(false);
    failures += test_mpl_refusal_matrix(true);
    failures += test_mpl_empty_store();
    failures += test_mpl_redaction_fingerprints(false);
    failures += test_mpl_redaction_fingerprints(true);
    failures += test_mpl_state_classification_and_counts(false);
    failures += test_mpl_state_classification_and_counts(true);
    failures += test_mpl_paging();
    failures += test_mpl_max_clamp(false);
    failures += test_mpl_max_clamp(true);
    printf("=== mesh_pairing_list: %d failures ===\n", failures);
    return failures;
}
