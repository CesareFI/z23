/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: semantic_sensor checks that facts describe the object: the clang-facts rule senses each TU with its dev object's own argv, and the object compiler enters the IDENTITY record.
 *
 * Part of the semantic_sensor group (test_semantic_manifest.c), so it runs
 * only where build/bin/z23-clang-manifest is built.
 *
 *   argv       `make -n` of dev objects and their clang-facts targets, for
 *              hot (-O2) and ordinary (-Og) directories: the sensor's
 *              --cc and argv after `--` equal the object's compiler (its
 *              compile-cache wrapper dropped) and flags, token for token.
 *              The identity TU, whose object bakes a receipt only its own
 *              rule may name, must be refused rather than sensed with other
 *              flags.
 *   compiler   --cc names the object compiler in IDENTITY by spelled path
 *              and SHA3-256 of its bytes; no --cc is "object-cc unknown";
 *              an unresolvable one refuses. Manifests of the same tree
 *              under two compilers differ in IDENTITY alone, and the
 *              consumer widens across them where it narrows under one.
 */

#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "test/test_core.h"

#include "base/hex.h"
#include "base/safe_alloc.h"
#include "sha3/sha3.h"
#include "test/semantic_consumer_fixture.h"
#include "test/semantic_facts_fixture.h"
#include "util/spawn.h"
#include "vcs/semantic_manifest.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SSI_SENSOR "build/bin/z23-clang-manifest"
#define SSI_MAKE_OUT (4u << 20)
#define SSI_TOKENS 1024

int semantic_sensor_identity_tests(void);

/* ── argv: the facts rule senses exactly what the object compiles ───────── */

/* Hot directories (DEV_HOT_CFLAGS), ordinary ones, and the identity TU. */
static const char *const k_ssi_probe_srcs[] = {
    "core/modules/chain/src/chain.c",
    "core/chainparams/src/chainparams.c",
    "core/modules/crypto/src/aes256.c",
    "core/modules/validation/src/accept_block_header.c",
    "engine/modules/hotswap/src/hotswap_activate.c",
    "platform/modules/util/src/signal_handler.c",
    "platform/modules/util/src/clientversion.c",
};
#define SSI_NPROBE (sizeof(k_ssi_probe_srcs) / sizeof(k_ssi_probe_srcs[0]))
#define SSI_IDENTITY_TU "platform/modules/util/src/clientversion.c"

/* Backslash-newline continuations joined, so one recipe is one line. */
static void ssi_join(char *s)
{
    char *w = s;
    for (char *r = s; *r != '\0'; r++) {
        if (r[0] == '\\' && r[1] == '\n') {
            *w++ = ' ';
            r++;
            continue;
        }
        *w++ = *r;
    }
    *w = '\0';
}

/* The line of `text` holding `needle` (and `also`, when set), copied. */
static bool ssi_line(const char *text, const char *needle, const char *also,
                     char *out, size_t cap)
{
    for (const char *p = strstr(text, needle); p != NULL;
         p = strstr(p + 1, needle)) {
        const char *b = p, *e = strchr(p, '\n');
        while (b > text && b[-1] != '\n')
            b--;
        if (e == NULL)
            e = p + strlen(p);
        if ((size_t)(e - b) >= cap)
            continue;
        memcpy(out, b, (size_t)(e - b));
        out[e - b] = '\0';
        if (also == NULL || strstr(out, also) != NULL)
            return true;
    }
    return false;
}

static size_t ssi_split(char *s, char **tok, size_t cap)
{
    size_t n = 0;
    for (char *t = strtok(s, " \t"); t != NULL && n < cap;
         t = strtok(NULL, " \t"))
        tok[n++] = t;
    return n;
}

static bool ssi_wrapper(const char *word)
{
    const char *base = strrchr(word, '/');
    base = base != NULL ? base + 1 : word;
    return strcmp(base, "zcc") == 0 || strcmp(base, "ccache") == 0 ||
           strcmp(base, "sccache") == 0;
}

/* Compare one source's object recipe and facts recipe. */
static bool ssi_same_argv(const char *src, char *obj, char *zsm)
{
    char **o = zcl_calloc(SSI_TOKENS, sizeof(char *), "ssi.otok");
    char **z = zcl_calloc(SSI_TOKENS, sizeof(char *), "ssi.ztok");
    char *otail = strstr(obj, " -- "), *ztail = strstr(zsm, " -- ");
    const char *cc = NULL;
    size_t on = 0, zn = 0, oat = 0;
    bool ok = o != NULL && z != NULL && otail != NULL && ztail != NULL;
    if (ok) {
        char *ccopt = strstr(zsm, " --cc ");
        if (ccopt != NULL && ccopt < ztail) {
            cc = ccopt + 6;
            ccopt[6 + strcspn(cc, " \t")] = '\0';
        }
        on = ssi_split(otail + 4, o, SSI_TOKENS);
        zn = ssi_split(ztail + 4, z, SSI_TOKENS);
        oat = on > 0 && ssi_wrapper(o[0]) ? 1 : 0;
    }
    if (ok && (on <= oat || cc == NULL || strcmp(o[oat], cc) != 0)) {
        printf("  argv %s: object compiler %s, sensor --cc %s\n", src,
               on > oat ? o[oat] : "(none)", cc != NULL ? cc : "(none)");
        ok = false;
    }
    for (size_t k = 0; ok && k < on - oat - 1 && k < zn; k++) {
        if (strcmp(o[oat + 1 + k], z[k]) != 0) {
            printf("  argv %s: word %zu: object %s, sensor %s\n", src, k,
                   o[oat + 1 + k], z[k]);
            ok = false;
        }
    }
    if (ok && on - oat - 1 != zn) {
        printf("  argv %s: object has %zu flags, sensor %zu\n", src,
               on - oat - 1, zn);
        ok = false;
    }
    free(o);
    free(z);
    return ok;
}

/* One source against the dry run: equal argv, or the identity TU refused. */
static bool ssi_probe_src(const char *text, const char *src)
{
    char needle[PATH_MAX * 2], stem[PATH_MAX];
    char *obj = zcl_malloc(1u << 16, "ssi.obj");
    char *zsm = zcl_malloc(1u << 16, "ssi.zsm");
    bool refused, ok = obj != NULL && zsm != NULL;
    (void)snprintf(stem, sizeof(stem), "%.*s", (int)(strlen(src) - 2), src);
    (void)snprintf(needle, sizeof(needle), "/%s.o\" \"%s\"", stem, src);
    if (ok && !ssi_line(text, needle, " -- ", obj, 1u << 16)) {
        printf("  argv %s: no object recipe in the dry run\n", src);
        ok = false;
    }
    (void)snprintf(needle, sizeof(needle), "build/clang-facts/%s.zsm", stem);
    refused = ok && ssi_line(text, needle, "clang-facts: refused", zsm, 1u << 16);
    if (ok && strcmp(src, SSI_IDENTITY_TU) == 0) {
        if (!refused)
            printf("  argv %s: the identity TU is sensed, not refused\n", src);
        ok = refused;
    } else if (ok && refused) {
        printf("  argv %s: refused: %s\n", src, zsm);
        ok = false;
    } else if (ok && !ssi_line(text, needle, "z23-clang-manifest emit", zsm,
                               1u << 16)) {
        printf("  argv %s: no clang-facts recipe in the dry run\n", src);
        ok = false;
    } else if (ok) {
        ok = ssi_same_argv(src, obj, zsm);
    }
    free(obj);
    free(zsm);
    return ok;
}

static bool ssi_write(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    bool ok = fp != NULL && fputs(text, fp) >= 0;
    if (fp != NULL && fclose(fp) != 0)
        ok = false;
    return ok;
}

/* A probe makefile read after the repository's: every sample's dev object
 * and facts target, named as the Makefile itself names them. */
static bool ssi_probe_makefile(const char *path)
{
    char text[4096];
    size_t w = (size_t)snprintf(text, sizeof(text), "SSI_SRCS :=");
    for (size_t k = 0; k < SSI_NPROBE && w < sizeof(text); k++)
        w += (size_t)snprintf(text + w, sizeof(text) - w, " %s",
                              k_ssi_probe_srcs[k]);
    if (w >= sizeof(text))
        return false;
    w += (size_t)snprintf(
        text + w, sizeof(text) - w,
        "\n.PHONY: ssi-argv-probe\n"
        "ssi-argv-probe: $(patsubst %%.c,$(DEV_OBJ_DIR)/%%.o,$(SSI_SRCS)) "
        "$(patsubst %%.c,$(CLANG_FACTS_OUT_DIR)/%%.zsm,$(SSI_SRCS))\n");
    return w < sizeof(text) && ssi_write(path, text);
}

static bool ssi_dry_run(const char *probe, char *out, size_t cap)
{
    const char *argv[64];
    size_t k = 0;
    bool timed_out = false;
    const char *const env[] = {"env",         "-u", "MAKEFLAGS", "-u",
                               "MFLAGS",      "-u", "MAKELEVEL", "-u",
                               "MAKEOVERRIDES", "-u", "GNUMAKEFLAGS"};
    for (size_t i = 0; i < sizeof(env) / sizeof(env[0]); i++)
        argv[k++] = env[i];
    argv[k++] = "make";
    argv[k++] = "--no-print-directory";
    argv[k++] = "-n";
    argv[k++] = "-f";
    argv[k++] = "Makefile";
    argv[k++] = "-f";
    argv[k++] = probe;
    for (size_t i = 0; i < SSI_NPROBE; i++) {
        argv[k++] = "-W";
        argv[k++] = k_ssi_probe_srcs[i];
    }
    argv[k++] = "ssi-argv-probe";
    argv[k] = NULL;
    int rc = zcl_spawn_capture_merged_observed(argv, out, cap, 600000,
                                               &timed_out);
    if (rc != 0 || timed_out)
        printf("  make -n exited %d%s: %.2000s\n", rc,
               timed_out ? " (timed out)" : "", out);
    return rc == 0 && !timed_out;
}

static int ssi_t_argv(void)
{
    int failures = 0;
    char dir[1024] = {0}, probe[PATH_MAX];
    char *out = zcl_malloc(SSI_MAKE_OUT, "ssi.make_out");
    TEST_CASE("semantic_sensor: clang-facts senses each TU with its dev object's own compiler and argv") {
        ASSERT(out != NULL);
        ASSERT(test_mkdtemp(dir, sizeof(dir), "semsensor_argv") != NULL);
        (void)snprintf(probe, sizeof(probe), "%s/probe.mk", dir);
        ASSERT(ssi_probe_makefile(probe));
        ASSERT(ssi_dry_run(probe, out, SSI_MAKE_OUT));
        ssi_join(out);
        size_t bad = 0;
        for (size_t k = 0; k < SSI_NPROBE; k++)
            bad += !ssi_probe_src(out, k_ssi_probe_srcs[k]);
        ASSERT_EQ(bad, 0);
    } TEST_END
    free(out);
    if (dir[0] != '\0')
        (void)test_rm_rf_recursive(dir);
    return failures;
}

/* ── compiler: the object compiler is part of the identity ──────────────── */

#define SSI_CC_A "bin-a/cc"
#define SSI_CC_B "bin-b/cc"

static bool ssi_emit(const char *root, const char *source, const char *out,
                     const char *cc, const char *const *flags, size_t nflags,
                     char *message, size_t cap)
{
    const char *argv[48];
    size_t k = 0;
    bool timed_out = false;
    argv[k++] = SSI_SENSOR;
    argv[k++] = "emit";
    argv[k++] = "--root";
    argv[k++] = root;
    argv[k++] = "--source";
    argv[k++] = source;
    argv[k++] = "--out";
    argv[k++] = out;
    argv[k++] = "--facts";
    if (cc != NULL) {
        argv[k++] = "--cc";
        argv[k++] = cc;
    }
    argv[k++] = "--";
    for (size_t i = 0; i < nflags && k < 47; i++)
        argv[k++] = flags[i];
    argv[k] = NULL;
    int rc = zcl_spawn_capture_merged_observed(argv, message, cap, 60000,
                                               &timed_out);
    return rc == 0 && !timed_out;
}

static bool ssi_emit_ok(const char *root, const char *source, const char *out,
                        const char *cc, const char *const *flags, size_t nflags)
{
    char message[4096];
    bool ok = ssi_emit(root, source, out, cc, flags, nflags, message,
                       sizeof(message));
    if (!ok)
        printf("  emit %s --cc %s failed: %s\n", source,
               cc != NULL ? cc : "(none)", message);
    return ok;
}

static bool ssi_has(const uint8_t *b, size_t n, const char *text)
{
    size_t t = strlen(text);
    for (size_t k = 0; t <= n && k + t <= n; k++)
        if (memcmp(b + k, text, t) == 0)
            return true;
    return false;
}

/* Two stand-in compilers inside the root: never run, only hashed. */
static bool ssi_write_compilers(const char *root, char *a_text, size_t cap)
{
    char path[PATH_MAX];
    uint8_t d[32];
    char hex[65];
    static const char body_a[] = "#!/bin/sh\nexit 0 # compiler a\n";
    static const char body_b[] = "#!/bin/sh\nexit 0 # compiler b\n";
    (void)snprintf(path, sizeof(path), "%s/bin-a", root);
    if (!scx_mkdir(path))
        return false;
    (void)snprintf(path, sizeof(path), "%s/bin-b", root);
    if (!scx_mkdir(path))
        return false;
    (void)snprintf(path, sizeof(path), "%s/" SSI_CC_A, root);
    if (!ssi_write(path, body_a) || chmod(path, 0755) != 0)
        return false;
    (void)snprintf(path, sizeof(path), "%s/" SSI_CC_B, root);
    if (!ssi_write(path, body_b) || chmod(path, 0755) != 0)
        return false;
    zcl_sha3_256((const unsigned char *)body_a, sizeof(body_a) - 1, d);
    zcl_hex_encode(d, 32, hex);
    (void)snprintf(a_text, cap, "object-cc " SSI_CC_A " sha3-256 %s", hex);
    return true;
}

static uint32_t ssi_changed(const uint8_t *a, size_t an, const uint8_t *b,
                            size_t bn)
{
    struct vcs_semantic_diff_v1 d;
    if (!vcs_semantic_manifest_v1_diff(a, an, b, bn, &d, NULL, NULL))
        return UINT32_MAX;
    return d.changed_sections;
}

static int ssi_t_identity(void)
{
    int failures = 0;
    char root[1024] = {0}, cc_a[PATH_MAX], cc_b[PATH_MAX], out[PATH_MAX];
    char want_a[160], message[4096];
    uint8_t *m[5] = {0};
    size_t n[5] = {0};
    static const char *const flags[] = {"-std=c23", "-O1"};
    static const char src[] = "int ssi_value(int x) { return x + 1; }\n";
    TEST_CASE("semantic_sensor: IDENTITY names the object compiler by path and bytes") {
        ASSERT(test_mkdtemp(root, sizeof(root), "semsensor_cc") != NULL);
        ASSERT(ssi_write_compilers(root, want_a, sizeof(want_a)));
        (void)snprintf(out, sizeof(out), "%s/ssi.c", root);
        ASSERT(ssi_write(out, src));
        (void)snprintf(cc_a, sizeof(cc_a), "%s/" SSI_CC_A, root);
        (void)snprintf(cc_b, sizeof(cc_b), "%s/" SSI_CC_B, root);
        const char *ccs[5] = {cc_a, cc_a, cc_b, NULL, "sh"};
        for (size_t k = 0; k < 5; k++) {
            (void)snprintf(out, sizeof(out), "%s/m%zu.zsm", root, k);
            ASSERT(ssi_emit_ok(root, "ssi.c", out, ccs[k], flags, 2));
            ASSERT(sft_read(out, &m[k], &n[k]));
        }
        ASSERT(n[0] == n[1] && memcmp(m[0], m[1], n[0]) == 0);
        ASSERT(ssi_has(m[0], n[0], want_a));
        ASSERT_EQ(ssi_changed(m[0], n[0], m[2], n[2]),
                  1u << VCS_SEMANTIC_SECTION_V1_IDENTITY);
        ASSERT(ssi_has(m[3], n[3], "; object-cc unknown"));
        ASSERT_EQ(ssi_changed(m[0], n[0], m[3], n[3]),
                  1u << VCS_SEMANTIC_SECTION_V1_IDENTITY);
        ASSERT(ssi_has(m[4], n[4], "; object-cc @sys/"));
        (void)snprintf(out, sizeof(out), "%s/missing.zsm", root);
        ASSERT(!ssi_emit(root, "ssi.c", out, "zfx-no-such-compiler", flags, 2,
                         message, sizeof(message)));
        ASSERT(strstr(message, "object compiler") != NULL);
        ASSERT(access(out, F_OK) != 0);
    } TEST_END
    for (size_t k = 0; k < 5; k++)
        free(m[k]);
    if (root[0] != '\0')
        (void)test_rm_rf_recursive(root);
    return failures;
}

/* Sense every TU of variant v into <out>/<tag>/<tu>.zsm with compiler cc. */
static bool ssi_sense(const char *root, const char *out, const char *tag,
                      enum scx_variant v, const char *cc,
                      uint8_t *m[SCX_TU_COUNT], size_t n[SCX_TU_COUNT])
{
    char dir[PATH_MAX], path[PATH_MAX + 64];
    (void)snprintf(dir, sizeof(dir), "%s/%s", out, tag);
    if (!scx_mkdir(dir) || !scx_write_tree(root, v))
        return false;
    for (size_t tu = 0; tu < SCX_TU_COUNT; tu++) {
        (void)snprintf(path, sizeof(path), "%s/%s.zsm", dir,
                       strrchr(k_scx_tus[tu], '/') + 1);
        if (!ssi_emit_ok(root, k_scx_tus[tu], path, cc, k_scx_flags,
                         k_scx_nflags) ||
            !sft_read(path, &m[tu], &n[tu]))
            return false;
    }
    return true;
}

/* Plan variant v with before/after evidence; true when the plan widened
 * (verdict not narrowed) and every TU the report lists is affected. */
static bool ssi_plan(enum scx_variant v, uint8_t *const before[],
                     const size_t bn[], uint8_t *const after[],
                     const size_t an[], bool want_wide)
{
    char root[PATH_MAX] = {0};
    struct scx_evidence ev = {0};
    struct scx_result *res = zcl_calloc(1, sizeof(*res), "ssi.result");
    size_t unsafe = 0;
    bool ok = res != NULL &&
              test_mkdtemp(root, sizeof(root), "semsensor_ccplan") != NULL;
    for (size_t tu = 0; tu < SCX_TU_COUNT; tu++) {
        ev.before[tu] = before[tu];
        ev.before_len[tu] = bn[tu];
        ev.after[tu] = after[tu];
        ev.after_len[tu] = an[tu];
    }
    ok = ok && scx_consume(root, v, &ev, res);
    if (ok && !want_wide) {
        ok = scx_compare(v, res, &unsafe, stdout) == 0;
    } else if (ok) {
        ok = !res->verdict.narrowed && res->report.ntus > 0;
        for (size_t k = 0; ok && k < res->report.ntus; k++)
            ok = res->report.tus[k].affected && res->report.tus[k].broadened;
        if (!ok)
            printf("  %s across two compilers: narrowed %d reason %s, "
                   "universe %zu, affected %zu\n", k_scx_edits[v].name,
                   (int)res->verdict.narrowed, res->verdict.reason,
                   res->report.ntus, res->report.naffected);
    }
    if (res != NULL)
        scx_result_free(res);
    free(res);
    if (root[0] != '\0')
        (void)test_rm_rf_recursive(root);
    return ok;
}

static void ssi_free(uint8_t *m[SCX_TU_COUNT])
{
    for (size_t tu = 0; tu < SCX_TU_COUNT; tu++)
        free(m[tu]);
}

static int ssi_t_widen(void)
{
    int failures = 0;
    char out[1024] = {0}, root[PATH_MAX], want[160], cc_a[PATH_MAX],
        cc_b[PATH_MAX];
    uint8_t *base[SCX_TU_COUNT] = {0}, *same[SCX_TU_COUNT] = {0},
            *other[SCX_TU_COUNT] = {0};
    size_t bn[SCX_TU_COUNT] = {0}, sn[SCX_TU_COUNT] = {0},
           on[SCX_TU_COUNT] = {0};
    static const enum scx_variant variants[] = {SCX_TAIL, SCX_BODY};
    TEST_CASE("semantic_sensor: a compiler change between the sides widens the plan") {
        ASSERT(test_mkdtemp(out, sizeof(out), "semsensor_ccwiden") != NULL);
        (void)snprintf(root, sizeof(root), "%s/tree", out);
        ASSERT(scx_mkdir(root));
        ASSERT(ssi_write_compilers(root, want, sizeof(want)));
        (void)snprintf(cc_a, sizeof(cc_a), "%s/" SSI_CC_A, root);
        (void)snprintf(cc_b, sizeof(cc_b), "%s/" SSI_CC_B, root);
        ASSERT(ssi_sense(root, out, "base", SCX_BASE, cc_a, base, bn));
        for (size_t k = 0; k < sizeof(variants) / sizeof(variants[0]); k++) {
            enum scx_variant v = variants[k];
            char tag[64];
            (void)snprintf(tag, sizeof(tag), "%s_a", k_scx_edits[v].name);
            ASSERT(ssi_sense(root, out, tag, v, cc_a, same, sn));
            (void)snprintf(tag, sizeof(tag), "%s_b", k_scx_edits[v].name);
            ASSERT(ssi_sense(root, out, tag, v, cc_b, other, on));
            ASSERT(ssi_plan(v, base, bn, same, sn, false));
            ASSERT(ssi_plan(v, base, bn, other, on, true));
            ssi_free(same);
            ssi_free(other);
            memset(same, 0, sizeof(same));
            memset(other, 0, sizeof(other));
        }
    } TEST_END
    ssi_free(base);
    ssi_free(same);
    ssi_free(other);
    if (out[0] != '\0' && failures == 0)
        (void)test_rm_rf_recursive(out);
    return failures;
}

int semantic_sensor_identity_tests(void)
{
    int failures = 0;
    failures += ssi_t_argv();
    failures += ssi_t_identity();
    failures += ssi_t_widen();
    return failures;
}
