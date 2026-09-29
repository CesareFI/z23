/* Copyright 2026 Rhett Creighton; SPDX-License-Identifier: Apache-2.0.
 * Fixed-result verifier store: exact immutable observations or named refusal.
 * The same-UID fixture exists only in ZCL_TESTING and proves no production HIT. */
#define _XOPEN_SOURCE 700
#include "test/test_core.h"

#if defined(_WIN32)
int test_verify_store(void) { return 0; }
#else
#include "base/hex.h"
#include "crypto/ed25519.h"
#include "sha3/sha3.h"
#include "verify_attest.h"
#include "verify_store.h"
#include "test/verify_contract_fixture.h"

#include <fcntl.h>
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define VS_OBJ TEST_VC_OBJ
#define VS_KEY_ENV "ZCL_TEST_VERIFY_ATTEST_PUBKEY"

static const uint8_t vs_seed[32] = {0x31};
static const uint8_t vs_other_seed[32] = {0x42};

/* Build in place and never copy: vc borrows from itself. */
struct vs_fixture {
    char root[PATH_MAX], store[PATH_MAX], key_dir[PATH_MAX];
    char lock[PATH_MAX], key_root[PATH_MAX], pubfile[PATH_MAX];
    char pass_name[65], fail_name[65], forged_name[65];
    struct test_vc_fixture vc;
    struct zcl_verify_attest_expected expected;
    struct zcl_verify_attest_box_key box;
};

static bool vs_path(char out[PATH_MAX], const char *a, const char *b)
{
    int n = snprintf(out, PATH_MAX, "%s/%s", a, b);
    return n > 0 && n < PATH_MAX;
}

static bool vs_write(const char *path, const void *bytes, size_t len,
                     mode_t mode)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    if (fd < 0) return false;
    const uint8_t *p = bytes;
    size_t at = 0;
    while (at < len) {
        ssize_t n = write(fd, p + at, len - at);
        if (n <= 0) { (void)close(fd); return false; }
        at += (size_t)n;
    }
    bool ok = fchmod(fd, mode) == 0;
    return close(fd) == 0 && ok;
}

static void vs_pub(const uint8_t seed[32], uint8_t pub[32])
{
    uint8_t secret[32];
    ed25519_keypair(pub, secret, seed);
}

static bool vs_fixture_make(struct vs_fixture *f)
{
    memset(f, 0, sizeof(*f));
    char temporary[PATH_MAX];
    char *made = test_mkdtemp(temporary, sizeof(temporary), "z23-verify-store");
    if (!made || !realpath(made, f->root)) return false;
    /* The verifier-key loader checks every ancestor for writable-path
     * attacks. This host's checkout ancestors are group writable, so the
     * pinned-key fixture alone needs a private system-temp directory. */
    char key_temporary[] = "/tmp/z23-verify-store-key.XXXXXX";
    if (!mkdtemp(key_temporary) || !realpath(key_temporary, f->key_root))
        return false;
    char locks[PATH_MAX];
    if (!vs_path(f->store, f->root, "store") ||
        !vs_path(locks, f->root, "locks") ||
        !vs_path(f->lock, locks, "fixed_result.lock") ||
        !vs_path(f->pubfile, f->key_root, "verifier.pub") ||
        mkdir(f->store, 0755) != 0 || mkdir(locks, 0755) != 0 ||
        !vs_write(f->lock, "", 0, 0644)) return false;
    if (!test_vc_fixture_make(&f->vc, 'a')) return false;
    f->expected = f->vc.expected;
    char key[ZCL_VERIFY_ATTEST_STORE_KEY_HEX];
    zcl_verify_attest_store_key_hex(&f->expected.toolchain_id,
                                    &f->expected.argv_norm,
                                    &f->expected.recorded_cwd,
                                    f->expected.pp_sha3,
                                    f->expected.closure_sha3, key);
    if (!vs_path(f->key_dir, f->store, key) ||
        mkdir(f->key_dir, 0755) != 0) return false;
    uint8_t pub[32];
    char hex[66];
    vs_pub(vs_seed, pub);
    zcl_hex_encode(pub, 32, hex);
    hex[64] = '\n'; hex[65] = '\0';
    if (!vs_write(f->pubfile, hex, 65, 0644)) return false;
    f->box.known = true;
    return setenv(VS_KEY_ENV, f->pubfile, 1) == 0;
}

static bool vs_publish(const struct vs_fixture *f, const uint8_t seed[32],
                       int exit_code, char out_name[65])
{
    struct zcl_verify_attest_record record = test_vc_record(&f->vc, exit_code);
    uint8_t *bytes = NULL, hash[32];
    size_t len = 0;
    const char *why = NULL;
    if (!zcl_verify_attest_seal(&record, seed, &bytes, &len, &why))
        return false;
    zcl_sha3_256(bytes, len, hash);
    zcl_hex_encode(hash, sizeof(hash), out_name);
    char dir[PATH_MAX], path[PATH_MAX];
    bool ok = vs_path(dir, f->key_dir, out_name) && mkdir(dir, 0755) == 0 &&
              vs_path(path, dir, "attest.bin") && vs_write(path, bytes, len, 0644) &&
              vs_path(path, dir, "object.o") &&
              vs_write(path, VS_OBJ, sizeof(VS_OBJ) - 1u, 0644) &&
              vs_path(path, dir, "deps.d") &&
              vs_write(path, f->vc.dep, f->vc.dep_len, 0644) &&
              vs_path(path, dir, "stderr.bin") && vs_write(path, "", 0, 0644) &&
              vs_path(path, dir, "launch.bin") &&
              vs_write(path, f->vc.receipt_bytes, f->vc.receipt_len, 0644);
    free(bytes);
    return ok;
}

static bool vs_remove_observation(const struct vs_fixture *f,
                                  const char *name)
{
    char dir[PATH_MAX], path[PATH_MAX];
    if (!vs_path(dir, f->key_dir, name)) return false;
    const char *files[] = {"attest.bin", "object.o", "deps.d", "stderr.bin",
                           "launch.bin"};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        if (!vs_path(path, dir, files[i])) return false;
        if (unlink(path) != 0 && errno != ENOENT) return false;
    }
    return rmdir(dir) == 0;
}

static int vs_remove_fixture_entry(const char *path, const struct stat *st,
                                   int type, struct FTW *walk)
{
    (void)st; (void)type; (void)walk;
    return remove(path);
}

static struct zcl_verify_store_result vs_lookup(const struct vs_fixture *f,
                                                bool allow_same_uid)
{
    struct zcl_verify_store_result r;
    zcl_verify_store_lookup_fixture(f->root, (unsigned)geteuid(),
                                     (unsigned)geteuid(),
                                     allow_same_uid, &f->expected,
                                     &f->vc.pins,
                                     &f->box, &r);
    return r;
}

static bool vs_reason(const struct zcl_verify_store_result *r,
                      enum zcl_verify_store_verdict verdict,
                      const char *reason)
{
    return r->verdict == verdict && r->reason &&
           strcmp(r->reason, reason) == 0;
}

static bool vs_expect(const struct vs_fixture *f, bool same_uid,
                      enum zcl_verify_store_verdict verdict,
                      const char *reason)
{
    struct zcl_verify_store_result r = vs_lookup(f, same_uid);
    bool ok = vs_reason(&r, verdict, reason);
    zcl_verify_store_result_release(&r);
    return ok;
}

static int vs_test_artifacts(struct vs_fixture *f)
{
    int failures = 0;
    TEST("verify store: exact PASS bytes and unsafe artifact refusals") {
        struct zcl_verify_store_result r = vs_lookup(f, true);
        if (r.verdict != ZCL_VERIFY_STORE_HIT)
            fprintf(stderr, "verify store fixture cold: %s\n", r.reason);
        ASSERT(r.verdict == ZCL_VERIFY_STORE_HIT);
        ASSERT(r.lock_fd >= 0 && r.object_len == sizeof(VS_OBJ) - 1u);
        ASSERT(memcmp(r.object, VS_OBJ, r.object_len) == 0);
        ASSERT(strlen(r.store_key) == 64);
        ASSERT(strcmp(r.record_sha3, f->pass_name) == 0);
        zcl_verify_store_result_release(&r);
        ASSERT(vs_expect(f, false, ZCL_VERIFY_STORE_COLD,
                         "store_owner_same_uid"));
        zcl_verify_store_lookup_fixture(f->root, (unsigned)geteuid(), 0,
                                         true, &f->expected, &f->vc.pins,
                                         &f->box, &r);
        ASSERT(vs_reason(&r, ZCL_VERIFY_STORE_COLD, "store_path_unsafe"));
        zcl_verify_store_result_release(&r);

        char dir[PATH_MAX], path[PATH_MAX];
        ASSERT(vs_path(dir, f->key_dir, f->pass_name));
        ASSERT(vs_path(path, dir, "object.o"));
        ASSERT(unlink(path) == 0);
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_COLD,
                         "store_artifact_missing"));
        ASSERT(vs_write(path, VS_OBJ, sizeof(VS_OBJ) - 1u, 0644));
        ASSERT(unlink(path) == 0);
        ASSERT(mkfifo(path, 0644) == 0);
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_COLD,
                         "store_file_unsafe"));
        ASSERT(unlink(path) == 0);
        ASSERT(vs_write(path, VS_OBJ, sizeof(VS_OBJ) - 1u, 0644));
        ASSERT(unlink(path) == 0);
        ASSERT(symlink(f->pubfile, path) == 0);
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_COLD,
                         "store_file_unreadable"));
        ASSERT(unlink(path) == 0);
        ASSERT(vs_write(path, VS_OBJ, sizeof(VS_OBJ) - 1u, 0644));
        PASS();
    } _test_next:;
    return failures;
}

static bool vs_replace_receipt(const struct vs_fixture *f, const char *name,
                               const uint8_t *bytes, size_t len)
{
    char dir[PATH_MAX], path[PATH_MAX];
    return vs_path(dir, f->key_dir, name) &&
           vs_path(path, dir, "launch.bin") &&
           (unlink(path) == 0 || errno == ENOENT) &&
           (!bytes || vs_write(path, bytes, len, 0644));
}

static bool vs_expect_pins(const struct vs_fixture *f,
                           const struct zcl_fixed_result_v2_roots *pins,
                           const char *reason)
{
    struct zcl_verify_store_result r;
    zcl_verify_store_lookup_fixture(f->root, (unsigned)geteuid(),
                                    (unsigned)geteuid(), true, &f->expected,
                                    pins, &f->box, &r);
    bool ok = vs_reason(&r, ZCL_VERIFY_STORE_COLD, reason);
    zcl_verify_store_result_release(&r);
    return ok;
}

static int vs_test_receipts(struct vs_fixture *f)
{
    int failures = 0;
    TEST("verify store: a developer-published entry, a missing or foreign "
         "launch receipt, and unpinned receivers never HIT") {
        struct zcl_verify_store_result r;
        /* The publisher alone being the developer uid is enough. */
        zcl_verify_store_lookup_fixture(f->root, 0u, (unsigned)geteuid(),
                                         false, &f->expected, &f->vc.pins,
                                         &f->box, &r);
        ASSERT(vs_reason(&r, ZCL_VERIFY_STORE_COLD, "store_owner_same_uid"));
        zcl_verify_store_result_release(&r);
        zcl_verify_store_lookup_fixture(f->root, (unsigned)geteuid(), 0u,
                                         false, &f->expected, &f->vc.pins,
                                         &f->box, &r);
        ASSERT(vs_reason(&r, ZCL_VERIFY_STORE_COLD, "store_owner_same_uid"));
        zcl_verify_store_result_release(&r);

        ASSERT(vs_replace_receipt(f, f->pass_name, NULL, 0));
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_COLD,
                         "store_artifact_missing"));
        /* A valid receipt for another launch: the record names the first. */
        struct test_vc_fixture other;
        ASSERT(test_vc_fixture_make(&other, 'a'));
        other.receipt.mount_namespace_ino += 1u;
        ASSERT(test_vc_receipt_reencode(&other, NULL));
        ASSERT(vs_replace_receipt(f, f->pass_name, other.receipt_bytes,
                                  other.receipt_len));
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_COLD,
                         ZCL_VERIFY_ATTEST_WHY_RECEIPT_MISMATCH));
        ASSERT(vs_replace_receipt(f, f->pass_name, f->vc.receipt_bytes,
                                  f->vc.receipt_len));
        r = vs_lookup(f, true);
        ASSERT(r.verdict == ZCL_VERIFY_STORE_HIT);
        zcl_verify_store_result_release(&r);

        struct zcl_fixed_result_v2_roots pins = f->vc.pins;
        pins.tree_checker[0] ^= 1u;
        ASSERT(vs_expect_pins(f, &pins, ZCL_FR_WHY_PIN_MISMATCH));
        ASSERT(vs_expect_pins(f, NULL, "store_pins_unqualified"));
        memset(pins.tree_checker, 0, sizeof(pins.tree_checker));
        ASSERT(vs_expect_pins(f, &pins, ZCL_FR_WHY_HASH_ZERO));
        PASS();
    } _test_next:;
    return failures;
}

static int vs_test_bounds(struct vs_fixture *f)
{
    int failures = 0;
    TEST("verify store: malformed expected key and unbounded children refuse") {
        struct zcl_verify_attest_expected oversized = f->expected;
        oversized.toolchain_id.len = ZCL_VERIFY_ATTEST_TOOLCHAIN_MAX + 1u;
        struct zcl_verify_store_result r;
        zcl_verify_store_lookup_fixture(f->root, (unsigned)geteuid(),
                                         (unsigned)geteuid(), true,
                                         &oversized, &f->vc.pins, &f->box,
                                         &r);
        ASSERT(vs_reason(&r, ZCL_VERIFY_STORE_COLD,
                         "store_expected_unqualified"));
        zcl_verify_store_result_release(&r);
        char unknown[PATH_MAX];
        for (unsigned i = 0; i < 97u; ++i) {
            char name[24];
            ASSERT(snprintf(name, sizeof(name), "unknown-%03u", i) > 0);
            ASSERT(vs_path(unknown, f->key_dir, name));
            ASSERT(vs_write(unknown, "", 0, 0644));
        }
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_BLOCK,
                         "store_scan_incomplete"));
        for (unsigned i = 0; i < 97u; ++i) {
            char name[24];
            ASSERT(snprintf(name, sizeof(name), "unknown-%03u", i) > 0);
            ASSERT(vs_path(unknown, f->key_dir, name));
            ASSERT(unlink(unknown) == 0);
        }
        PASS();
    } _test_next:;
    return failures;
}

static bool vs_replace_pubkey(const struct vs_fixture *f,
                              const uint8_t seed[32])
{
    uint8_t pub[32];
    char hex[66];
    vs_pub(seed, pub);
    zcl_hex_encode(pub, 32, hex);
    hex[64] = '\n'; hex[65] = '\0';
    return unlink(f->pubfile) == 0 && vs_write(f->pubfile, hex, 65, 0644);
}

static int vs_test_signers(struct vs_fixture *f)
{
    int failures = 0;
    TEST("verify store: forged and stale keys, symlink key path") {
        ASSERT(vs_publish(f, vs_other_seed, 0, f->forged_name));
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_COLD,
                         "attest_signer_not_verifier"));
        ASSERT(vs_remove_observation(f, f->forged_name));
        ASSERT(vs_replace_pubkey(f, vs_other_seed));
        struct zcl_verify_store_result r = vs_lookup(f, true);
        ASSERT(vs_reason(&r, ZCL_VERIFY_STORE_COLD,
                         "attest_signer_not_verifier"));
        ASSERT(r.store_key[0] == '\0' && r.record_sha3[0] == '\0');
        zcl_verify_store_result_release(&r);
        ASSERT(vs_replace_pubkey(f, vs_seed));
        char alternate[PATH_MAX];
        ASSERT(vs_path(alternate, f->store, "saved-key"));
        ASSERT(rename(f->key_dir, alternate) == 0);
        ASSERT(symlink(alternate, f->key_dir) == 0);
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_COLD,
                         "store_key_unsafe"));
        ASSERT(unlink(f->key_dir) == 0);
        ASSERT(rename(alternate, f->key_dir) == 0);
        PASS();
    } _test_next:;
    return failures;
}

static bool vs_restore_pass_record(const struct vs_fixture *f,
                                   const char *path)
{
    struct zcl_verify_attest_record pass_record = test_vc_record(&f->vc, 0);
    uint8_t *bytes = NULL;
    size_t len = 0;
    const char *why = NULL;
    if (!zcl_verify_attest_seal(&pass_record, vs_seed, &bytes, &len, &why))
        return false;
    bool ok = vs_write(path, bytes, len, 0644);
    free(bytes);
    return ok;
}

static int vs_test_conflicts(struct vs_fixture *f)
{
    int failures = 0;
    TEST("verify store: lock, incomplete scan, conflict, signed FAIL") {
        char dir[PATH_MAX], path[PATH_MAX], attest_path[PATH_MAX];
        ASSERT(vs_path(dir, f->key_dir, f->pass_name));
        ASSERT(vs_path(path, dir, "object.o"));
        ASSERT(vs_path(attest_path, dir, "attest.bin"));
        int lock_fd = open(f->lock, O_RDONLY | O_CLOEXEC);
        ASSERT(lock_fd >= 0);
        ASSERT(flock(lock_fd, LOCK_EX | LOCK_NB) == 0);
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_BLOCK,
                         "store_lock_unavailable"));
        ASSERT(vs_publish(f, vs_seed, 7, f->fail_name));
        ASSERT(flock(lock_fd, LOCK_UN) == 0);
        ASSERT(close(lock_fd) == 0);

        ASSERT(truncate(path, 8 * 1024 * 1024 + 1) == 0);
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_BLOCK,
                         "store_scan_incomplete"));
        ASSERT(unlink(path) == 0);
        ASSERT(vs_write(path, VS_OBJ, sizeof(VS_OBJ) - 1u, 0644));
        ASSERT(unlink(attest_path) == 0);
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_BLOCK,
                         "store_scan_incomplete"));
        ASSERT(vs_restore_pass_record(f, attest_path));
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_BLOCK,
                         "attest_eligible_conflict"));
        ASSERT(unlink(path) == 0);
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_BLOCK,
                         "attest_eligible_conflict"));
        ASSERT(vs_remove_observation(f, f->pass_name));
        ASSERT(vs_expect(f, true, ZCL_VERIFY_STORE_BLOCK,
                         "attest_exit_nonzero"));
        PASS();
    } _test_next:;
    return failures;
}

int test_verify_store(void)
{
    int failures = 0;
    struct vs_fixture f = {0};
    TEST("verify store: fixture setup") {
        ASSERT(zcl_verify_attest_test_override_compiled());
        ASSERT(vs_fixture_make(&f));
        ASSERT(vs_publish(&f, vs_seed, 0, f.pass_name));
        PASS();
    } _test_next:;
    if (!failures) failures += vs_test_artifacts(&f);
    if (!failures) failures += vs_test_receipts(&f);
    if (!failures) failures += vs_test_bounds(&f);
    if (!failures) failures += vs_test_signers(&f);
    if (!failures) failures += vs_test_conflicts(&f);
    (void)unsetenv(VS_KEY_ENV);
    if (f.root[0])
        (void)nftw(f.root, vs_remove_fixture_entry, 8, FTW_DEPTH | FTW_PHYS);
    if (f.key_root[0])
        (void)nftw(f.key_root, vs_remove_fixture_entry, 8,
                   FTW_DEPTH | FTW_PHYS);
    return failures;
}
#endif
