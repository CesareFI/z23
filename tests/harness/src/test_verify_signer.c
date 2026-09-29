/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
 * The fixed-result signer and publisher: a no-root dress rehearsal from the
 * real worker's qualify output to a receiver HIT byte-equal to its own cold
 * compile, and every custody, binding, conflict and identity refusal by its
 * stable token. Same-UID fixtures pass only through allow_same_uid; the
 * same calls without it, and the production entries, refuse. */
#include "test/test_core.h"

#if !defined(__linux__)
int test_verify_signer(void) { return 0; }
#else
#include "test/verify_signer_fixture.h"

#include "base/hex.h"
#include "sha3/sha3.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define VSIG_SEALED "sealed"
#define VSIG_PUBLISHED "published"

/* ── helpers ──────────────────────────────────────────────────────────── */

static struct vsg_state *vsig_state(struct vsg_world *w)
{
    struct vsg_state *s = calloc(1u, sizeof(*s));
    if (s && !vsg_state_make(w, s)) {
        free(s);
        s = NULL;
    }
    return s;
}

static const char *vsig_seal_with(const struct zcl_frs_fixture *fx,
                                  const char *launch,
                                  struct zcl_frs_result *out)
{
    vsg_seal(fx, launch, out);
    return out->reason ? out->reason : VSIG_SEALED;
}

static const char *vsig_seal(const struct vsg_state *s, bool pass,
                             struct zcl_frs_result *out)
{
    struct zcl_frs_fixture fx = vsg_signer(s);
    return vsig_seal_with(&fx, pass ? s->pass_launch : s->fail_launch, out);
}

static const char *vsig_publish_with(const struct zcl_frp_fixture *fx,
                                     const char *record,
                                     struct zcl_frp_result *out)
{
    zcl_frp_publish_fixture(fx, record, out);
    return out->reason ? out->reason : VSIG_PUBLISHED;
}

static const char *vsig_publish(const struct vsg_state *s, const char *record,
                                struct zcl_frp_result *out)
{
    struct zcl_frp_fixture fx = vsg_publisher(s);
    return vsig_publish_with(&fx, record, out);
}

/* Seal the PASS launch, for cases that tamper with staging afterwards. */
static bool vsig_sealed(const struct vsg_state *s,
                        struct zcl_frs_result *out)
{
    return strcmp(vsig_seal(s, true, out), VSIG_SEALED) == 0;
}

/* "hit:", "block:" or "cold:" and the lookup's reason, for one assert. */
static const char *vsig_verdict(const struct zcl_verify_store_result *r,
                                char out[128])
{
    static const char *const names[] = {"cold", "hit", "block"};
    unsigned v = (unsigned)r->verdict;
    (void)snprintf(out, 128u, "%s:%s", v < 3u ? names[v] : "unknown",
                   r->reason ? r->reason : "");
    return out;
}

static const char *vsig_lookup(struct vsg_world *w, const struct vsg_state *s,
                               const char *tag, char out[128])
{
    struct zcl_verify_store_result r = {0};
    struct vsg_bytes cold = {0};
    const char *v = vsg_receive(w, s, tag, true, &r, &cold)
                        ? vsig_verdict(&r, out) : "receiver_failed";
    zcl_verify_store_result_release(&r);
    vsg_bytes_free(&cold);
    return v;
}

static bool vsig_has_prefix(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool vsig_staged(const struct vsg_state *s, const char *record,
                        const char *name, char out[PATH_MAX])
{
    char dir[PATH_MAX];
    return vsg_path(dir, s->staging, record) && vsg_path(out, dir, name);
}

static bool vsig_stored(const struct vsg_state *s,
                        const struct zcl_frp_result *p, const char *name,
                        char out[PATH_MAX])
{
    char store[PATH_MAX], key[PATH_MAX], rec[PATH_MAX];
    return vsg_path(store, s->dir, "store") &&
           vsg_path(key, store, p->store_key) &&
           vsg_path(rec, key, p->record_sha3) && vsg_path(out, rec, name);
}

static bool vsig_file_hash(const char *path, uint8_t out[32])
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint8_t buf[65536];
    size_t n = fread(buf, 1, sizeof(buf), f);
    bool ok = ferror(f) == 0 && feof(f) != 0;
    (void)fclose(f);
    zcl_sha3_256(buf, n, out);
    return ok;
}

static bool vsig_dir_nonempty(const char *parent, const char *name)
{
    char path[PATH_MAX];
    struct stat st;
    return vsg_path(path, parent, name) && stat(path, &st) == 0 &&
           st.st_nlink >= 2u && rmdir(path) != 0 && errno == ENOTEMPTY;
}

/* A launch receipt for the same compile under a new request nonce. */
static bool vsig_renonce(const struct vsg_world *w, uint8_t *out, size_t *len)
{
    struct zcl_fr_receipt r;
    if (!zcl_fr_receipt_parse(w->pass.bytes, w->pass.len, &r, NULL))
        return false;
    memcpy(r.request_nonce, "ffeeddccbbaa99887766554433221100", ZCL_FR_ID_LEN);
    return zcl_fr_receipt_encode(&r, out, VSG_RECEIPT_CAP, len, NULL);
}

/* ── the dress rehearsal ──────────────────────────────────────────────── */

static void vsig_evidence(const struct vsg_world *w,
                          const struct zcl_frp_result *p,
                          const struct vsg_bytes *cold)
{
    char object[65], receipt[65];
    vsg_hex(cold->p, cold->n, object);
    vsg_hex(w->pass.bytes, w->pass.len, receipt);
    printf("verify signer evidence: object_bytes=%zu object_sha3=%s "
           "receipt_sha3=%s store_key=%s record_sha3=%s verdict=hit "
           "cold_equal=1\n", cold->n, object, receipt, p->store_key,
           p->record_sha3);
}

static int vsig_test_rehearsal(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    struct zcl_verify_store_result r = {0};
    struct vsg_bytes cold = {0};
    struct zcl_frp_result p = {0};
    char verdict[128];
    TEST("verify signer: worker, launcher, signer, publisher, receiver HIT") {
        struct zcl_frs_result sealed;
        ASSERT(s);
        ASSERT_STR_EQ(vsig_seal(s, true, &sealed), VSIG_SEALED);
        ASSERT(!sealed.failure && sealed.exit_code == 0);
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &p),
                      VSIG_PUBLISHED);
        ASSERT(!p.failure && !p.conflict_recorded);
        ASSERT_STR_EQ(p.store_key, sealed.store_key);
        ASSERT_STR_EQ(p.record_sha3, sealed.record_sha3);
        ASSERT(vsg_receive(w, s, "rehearsal", true, &r, &cold));
        ASSERT(vsig_has_prefix(vsig_verdict(&r, verdict), "hit:"));
        ASSERT_STR_EQ(r.store_key, p.store_key);
        ASSERT(r.object_len == cold.n && cold.n > 0 &&
               memcmp(r.object, cold.p, cold.n) == 0);
        ASSERT(w->object.n == cold.n &&
               memcmp(w->object.p, cold.p, cold.n) == 0);
        ASSERT(r.depfile_len == w->deps.n &&
               memcmp(r.depfile, w->deps.p, w->deps.n) == 0);
        PASS();
        vsig_evidence(w, &p, &cold);
    } _test_next:;
    zcl_verify_store_result_release(&r);
    vsg_bytes_free(&cold);
    free(s);
    return failures;
}

/* A published record in the developer's own site, looked up as a
 * production receiver would: without allow_same_uid it never HITs. */
static int vsig_test_developer_uid(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    struct zcl_verify_store_result r = {0};
    struct vsg_bytes cold = {0};
    char verdict[128];
    TEST("verify signer: the developer UID's record is refused without the "
         "fixture flag") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result p;
        ASSERT(s && vsig_sealed(s, &sealed));
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &p),
                      VSIG_PUBLISHED);
        ASSERT(vsg_receive(w, s, "developer", false, &r, &cold));
        ASSERT_STR_EQ(vsig_verdict(&r, verdict), "cold:store_owner_same_uid");
        PASS();
    } _test_next:;
    zcl_verify_store_result_release(&r);
    vsg_bytes_free(&cold);
    free(s);
    return failures;
}

/* ── signer: launch custody ───────────────────────────────────────────── */

static int vsig_test_receipt_custody(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: a receipt the launcher UID did not write refuses") {
        struct zcl_frs_result out;
        ASSERT(s);
        struct zcl_frs_fixture fx = vsg_signer(s);
        fx.trust.launcher_uid += 1u;
        ASSERT_STR_EQ(vsig_seal_with(&fx, s->pass_launch, &out),
                      ZCL_FRS_WHY_RECEIPT_OWNER);
        ASSERT(vsg_file_replace(s->pass_launch, "launch.bin", w->pass.bytes,
                                w->pass.len, 0644u));
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRS_WHY_RECEIPT_UNSAFE);
        ASSERT(vsg_file_replace(s->pass_launch, "launch.bin", w->pass.bytes,
                                w->pass.len, 0444u));
        ASSERT(vsg_file_replace(s->fail_launch, "object.o", w->object.p,
                                w->object.n, 0400u));
        ASSERT_STR_EQ(vsig_seal(s, false, &out), ZCL_FRS_WHY_INPUTS);
        char path[PATH_MAX];
        ASSERT(vsg_path(path, s->pass_launch, "launch.bin") &&
               unlink(path) == 0);
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRS_WHY_RECEIPT_UNSAFE);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

static int vsig_test_artifact_swap(struct vsg_world *w)
{
    int failures = 0;
    static const char *const names[] = {
        "object.o", "deps.d", "stderr.bin", "preprocessed.i"};
    TEST("verify signer: an artifact swapped after the receipt refuses") {
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
            struct vsg_state *s = vsig_state(w);
            struct zcl_frs_result out;
            bool swapped = s && vsg_file_replace(s->pass_launch, names[i],
                                                 "swapped\n", 8u, 0400u);
            const char *why = swapped ? vsig_seal(s, true, &out) : "setup";
            free(s);
            ASSERT_STR_EQ(why, ZCL_FR_WHY_RECEIPT_ARTIFACT);
        }
        PASS();
    } _test_next:;
    return failures;
}

static int vsig_test_trust_drift(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    struct zcl_fixed_result_v2_roots drift = w->pins;
    drift.worker[0] ^= 0x01u;
    TEST("verify signer: installed pins or profile drift refuses") {
        struct zcl_frs_result out;
        ASSERT(s);
        ASSERT(vsg_etc_pins(s, &drift));
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FR_WHY_PIN_MISMATCH);
        ASSERT(vsg_etc_pins(s, &w->pins));
        ASSERT(vsg_etc_write(s, ZCL_FRT_PROFILE, "-O2\n", 4u));
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRT_WHY_PROFILE_MISMATCH);
        ASSERT(vsg_etc_write(s, ZCL_FRT_PROFILE, w->profile.p, w->profile.n));
        ASSERT(vsg_file_replace(s->etc, ZCL_FRT_PINS, "x", 1u, 0644u));
        ASSERT(!vsig_has_prefix(vsig_seal(s, true, &out), VSIG_SEALED));
        ASSERT(vsg_etc_pins(s, &w->pins));
        ASSERT_STR_EQ(vsig_seal(s, true, &out), VSIG_SEALED);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

/* ── signer: key custody ──────────────────────────────────────────────── */

static int vsig_test_key(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    static const uint8_t other[32] = {0x77};
    TEST("verify signer: key custody, box key and pinned key refusals") {
        struct zcl_frs_result out;
        ASSERT(s);
        ASSERT(chmod(s->key_dir, 0700) == 0);
        char seed[PATH_MAX];
        ASSERT(vsg_path(seed, s->key_dir, ZCL_FRS_KEY_NAME));
        ASSERT(chmod(seed, 0440) == 0);
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRS_WHY_KEY_ACCESSIBLE);
        ASSERT(chmod(seed, 0404) == 0);
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRS_WHY_KEY_ACCESSIBLE);
        ASSERT(vsg_file_replace(s->key_dir, ZCL_FRS_KEY_NAME, w->box_seed,
                                32u, 0400u));
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRS_WHY_KEY_IS_BOX);
        ASSERT(vsg_file_replace(s->key_dir, ZCL_FRS_KEY_NAME, other, 32u,
                                0400u));
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRS_WHY_KEY_NOT_PINNED);
        ASSERT(unlink(seed) == 0);
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRS_WHY_KEY_MISSING);
        ASSERT(vsg_file_replace(s->key_dir, ZCL_FRS_KEY_NAME, w->seed, 32u,
                                0400u));
        ASSERT_STR_EQ(vsig_seal(s, true, &out), VSIG_SEALED);
        ASSERT_STR_EQ(vsig_seal(s, true, &out), ZCL_FRS_WHY_STAGING_EXISTS);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

static int vsig_test_box_pin(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: a verifier key equal to the box signer key refuses") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result pub;
        ASSERT(s && vsig_sealed(s, &sealed));
        ASSERT(vsg_etc_pubkey(s, ZCL_FRT_BOX_PUB, w->pub));
        ASSERT_STR_EQ(vsig_seal(s, false, &sealed),
                      ZCL_FRT_WHY_VERIFIER_IS_BOX);
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      ZCL_FRT_WHY_VERIFIER_IS_BOX);
        ASSERT(vsg_etc_pubkey(s, ZCL_FRT_BOX_PUB, w->box_pub));
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

/* ── identity ─────────────────────────────────────────────────────────── */

static int vsig_test_identity(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: same-UID and production identity refusals") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result pub;
        ASSERT(s);
        struct zcl_frs_fixture sfx = vsg_signer(s);
        sfx.allow_same_uid = false;
        ASSERT_STR_EQ(vsig_seal_with(&sfx, s->pass_launch, &sealed),
                      ZCL_FRS_WHY_SAME_UID);
        sfx = vsg_signer(s);
        sfx.trust.signer_uid += 1u;
        ASSERT_STR_EQ(vsig_seal_with(&sfx, s->pass_launch, &sealed),
                      ZCL_FRS_WHY_UID_MISMATCH);
        ASSERT(vsig_sealed(s, &sealed));
        struct zcl_frp_fixture pfx = vsg_publisher(s);
        pfx.allow_same_uid = false;
        ASSERT_STR_EQ(vsig_publish_with(&pfx, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_SAME_UID);
        pfx = vsg_publisher(s);
        pfx.trust.publisher_uid += 1u;
        ASSERT_STR_EQ(vsig_publish_with(&pfx, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_UID_MISMATCH);
        int none[ZCL_FRS_INPUTS] = {-1, -1, -1, -1, -1};
        if (geteuid() != ZCL_FRT_SIGNER_UID) {
            zcl_frs_seal_production(none, &sealed);
            ASSERT_STR_EQ(sealed.reason, ZCL_FRS_WHY_UID_NOT_VERIFIER);
        }
        if (geteuid() != 0) {
            zcl_frp_publish_production(
                "00000000000000000000000000000000"
                "00000000000000000000000000000000", &pub);
            ASSERT_STR_EQ(pub.reason, ZCL_FRP_WHY_ROOT_REQUIRED);
        }
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

/* ── publisher: staging custody ───────────────────────────────────────── */

static int vsig_test_staging_owner(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: staging not owned by the signer UID refuses") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result pub;
        ASSERT(s && vsig_sealed(s, &sealed));
        struct zcl_frp_fixture fx = vsg_publisher(s);
        fx.trust.signer_uid += 1u;
        ASSERT_STR_EQ(vsig_publish_with(&fx, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_STAGING_OWNER);
        ASSERT_STR_EQ(vsig_publish(s, "0000000000000000000000000000000000"
                                         "000000000000000000000000000000",
                                   &pub),
                      ZCL_FRP_WHY_STAGING_MISSING);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

static int vsig_test_staging_links(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: a symlinked staging entry or record refuses") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result pub;
        char entry[PATH_MAX], target[PATH_MAX], dir[PATH_MAX], moved[PATH_MAX];
        ASSERT(s && vsig_sealed(s, &sealed));
        ASSERT(vsig_staged(s, sealed.record_sha3, "object.o", entry));
        ASSERT(vsg_path(target, s->pass_launch, "object.o"));
        ASSERT(unlink(entry) == 0 && symlink(target, entry) == 0);
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_STAGING_ENTRY);
        ASSERT(vsg_path(dir, s->staging, sealed.record_sha3));
        ASSERT(vsg_path(moved, s->staging, "moved"));
        ASSERT(rename(dir, moved) == 0 && symlink(moved, dir) == 0);
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_STAGING_UNSAFE);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

static int vsig_test_staging_shape(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: extra, missing or renamed staging refuses") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result pub;
        char path[PATH_MAX], dir[PATH_MAX], renamed[PATH_MAX];
        char other[ZCL_VERIFY_ATTEST_STORE_KEY_HEX];
        ASSERT(s && vsig_sealed(s, &sealed));
        ASSERT(vsig_staged(s, sealed.record_sha3, "extra.bin", path));
        ASSERT(vsg_path(dir, s->staging, sealed.record_sha3));
        ASSERT(vsg_file_replace(dir, "extra.bin", "x", 1u, 0400u));
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_STAGING_CHILD);
        ASSERT(unlink(path) == 0);
        ASSERT(vsig_staged(s, sealed.record_sha3, "deps.d", path));
        ASSERT(unlink(path) == 0);
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_STAGING_INCOMPLETE);
        ASSERT(vsg_file_replace(dir, "deps.d", w->deps.p, w->deps.n, 0400u));
        memcpy(other, sealed.record_sha3, sizeof(other));
        other[0] = other[0] == '0' ? '1' : '0';
        ASSERT(vsg_path(renamed, s->staging, other));
        ASSERT(rename(dir, renamed) == 0);
        ASSERT_STR_EQ(vsig_publish(s, other, &pub), ZCL_FRP_WHY_RECORD_NAME);
        ASSERT(rename(renamed, dir) == 0);
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      VSIG_PUBLISHED);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

/* ── publisher: receipt binding ───────────────────────────────────────── */

static int vsig_test_receipt_binding(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    static uint8_t renonced[VSG_RECEIPT_CAP];
    size_t len = 0;
    TEST("verify signer: a staged or root receipt that differs refuses") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result pub;
        char dir[PATH_MAX];
        ASSERT(s && vsig_sealed(s, &sealed) &&
               vsig_renonce(w, renonced, &len));
        ASSERT(vsg_path(dir, s->staging, sealed.record_sha3));
        ASSERT(vsg_file_replace(dir, "launch.bin", renonced, len, 0400u));
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_ROOT_LAUNCH);
        ASSERT(vsg_file_replace(s->pass_launch, "launch.bin", renonced, len,
                                0444u));
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      ZCL_VERIFY_ATTEST_WHY_RECEIPT_MISMATCH);
        ASSERT(!pub.conflict_recorded);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

/* ── publisher: the store ─────────────────────────────────────────────── */

static int vsig_test_no_clobber(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: an existing record directory is kept, not replaced") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result pub, again;
        char attest[PATH_MAX];
        uint8_t before[32], after[32];
        ASSERT(s && vsig_sealed(s, &sealed));
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      VSIG_PUBLISHED);
        ASSERT(vsig_stored(s, &pub, "attest.bin", attest));
        ASSERT(vsig_file_hash(attest, before));
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &again),
                      ZCL_FRP_WHY_RECORD_EXISTS);
        ASSERT(vsig_file_hash(attest, after));
        ASSERT(memcmp(before, after, 32u) == 0);
        char verdict[128];
        ASSERT(vsig_has_prefix(vsig_lookup(w, s, "clobber", verdict), "hit:"));
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

/* Both records of a conflict live in the key directory the receiver scans,
 * so its admit_set sees them together. */
static bool vsig_key_records(const struct vsg_state *s, const char *key,
                             const char *a, const char *b)
{
    char store[PATH_MAX], dir[PATH_MAX], rec[PATH_MAX];
    struct stat st;
    return vsg_path(store, s->dir, "store") && vsg_path(dir, store, key) &&
           vsg_path(rec, dir, a) && lstat(rec, &st) == 0 &&
           S_ISDIR(st.st_mode) && vsg_path(rec, dir, b) &&
           lstat(rec, &st) == 0 && S_ISDIR(st.st_mode);
}

static int vsig_test_fail_then_pass(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: FAIL then PASS publishes both and the receiver "
         "blocks") {
        struct zcl_frs_result fail, pass;
        struct zcl_frp_result pub;
        char verdict[128];
        ASSERT(s);
        ASSERT_STR_EQ(vsig_seal(s, false, &fail), VSIG_SEALED);
        ASSERT(fail.failure && fail.exit_code == 1);
        ASSERT_STR_EQ(vsig_publish(s, fail.record_sha3, &pub),
                      VSIG_PUBLISHED);
        ASSERT(pub.failure && !pub.conflict);
        ASSERT_STR_EQ(vsig_lookup(w, s, "fail-first", verdict),
                      "block:attest_exit_nonzero");
        ASSERT(vsig_sealed(s, &pass));
        ASSERT_STR_EQ(pass.store_key, fail.store_key);
        ASSERT_STR_EQ(vsig_publish(s, pass.record_sha3, &pub),
                      VSIG_PUBLISHED);
        ASSERT(pub.conflict && pub.conflict_recorded);
        ASSERT_STR_EQ(pub.conflict, ZCL_FRP_WHY_CONFLICT_FAIL);
        ASSERT(vsig_key_records(s, pass.store_key, fail.record_sha3,
                                pass.record_sha3));
        ASSERT(vsig_dir_nonempty(s->dir, "conflicts"));
        ASSERT_STR_EQ(vsig_lookup(w, s, "fail-then-pass", verdict),
                      "block:" ZCL_VERIFY_ATTEST_WHY_ELIGIBLE_CONFLICT);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

static int vsig_test_pass_then_fail(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    TEST("verify signer: PASS then FAIL publishes both and the receiver "
         "blocks, never HITs") {
        struct zcl_frs_result fail, pass;
        struct zcl_frp_result pub;
        char verdict[128];
        ASSERT(s && vsig_sealed(s, &pass));
        ASSERT_STR_EQ(vsig_publish(s, pass.record_sha3, &pub),
                      VSIG_PUBLISHED);
        ASSERT(!pub.conflict);
        ASSERT(vsig_has_prefix(vsig_lookup(w, s, "pass-first", verdict),
                               "hit:"));
        ASSERT_STR_EQ(vsig_seal(s, false, &fail), VSIG_SEALED);
        ASSERT_STR_EQ(vsig_publish(s, fail.record_sha3, &pub),
                      VSIG_PUBLISHED);
        ASSERT(pub.failure && pub.conflict && pub.conflict_recorded);
        ASSERT_STR_EQ(pub.conflict, ZCL_FRP_WHY_CONFLICT_PASS);
        ASSERT(vsig_key_records(s, pass.store_key, pass.record_sha3,
                                fail.record_sha3));
        ASSERT(vsig_dir_nonempty(s->dir, "conflicts"));
        ASSERT_STR_EQ(vsig_lookup(w, s, "pass-then-fail", verdict),
                      "block:" ZCL_VERIFY_ATTEST_WHY_ELIGIBLE_CONFLICT);
        PASS();
    } _test_next:;
    free(s);
    return failures;
}

static int vsig_test_lock(struct vsg_world *w)
{
    int failures = 0;
    struct vsg_state *s = vsig_state(w);
    int held = -1;
    TEST("verify signer: a lock held past the deadline refuses") {
        struct zcl_frs_result sealed;
        struct zcl_frp_result pub;
        ASSERT(s && vsig_sealed(s, &sealed));
        held = open(s->lock, O_RDONLY | O_CLOEXEC);
        ASSERT(held >= 0 && flock(held, LOCK_EX | LOCK_NB) == 0);
        struct zcl_frp_fixture fx = vsg_publisher(s);
        fx.lock_deadline_ms = 100u;
        ASSERT_STR_EQ(vsig_publish_with(&fx, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_LOCK_DEADLINE);
        ASSERT(flock(held, LOCK_UN) == 0);
        ASSERT(chmod(s->lock, 0666) == 0);
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      ZCL_FRP_WHY_LOCK_UNSAFE);
        ASSERT(chmod(s->lock, 0644) == 0);
        ASSERT_STR_EQ(vsig_publish(s, sealed.record_sha3, &pub),
                      VSIG_PUBLISHED);
        PASS();
    } _test_next:;
    if (held >= 0) (void)close(held);
    free(s);
    return failures;
}

typedef int (*vsig_case)(struct vsg_world *w);

int test_verify_signer(void)
{
    static const vsig_case cases[] = {
        vsig_test_rehearsal,      vsig_test_developer_uid,
        vsig_test_receipt_custody,
        vsig_test_artifact_swap,  vsig_test_trust_drift,
        vsig_test_key,            vsig_test_box_pin,
        vsig_test_identity,       vsig_test_staging_owner,
        vsig_test_staging_links,  vsig_test_staging_shape,
        vsig_test_receipt_binding, vsig_test_no_clobber,
        vsig_test_fail_then_pass, vsig_test_pass_then_fail,
        vsig_test_lock,
    };
    int failures = 0;
    struct vsg_world *w = calloc(1u, sizeof(*w));
    TEST("verify signer: fixture world from the real worker") {
        ASSERT(w);
        ASSERT(zcl_verify_attest_test_override_compiled());
        ASSERT(vsg_world_make(w));
        PASS();
    } _test_next:;
    for (size_t i = 0; !failures && i < sizeof(cases) / sizeof(cases[0]); i++)
        failures += cases[i](w);
    if (w) vsg_world_free(w);
    free(w);
    return failures;
}
#endif
