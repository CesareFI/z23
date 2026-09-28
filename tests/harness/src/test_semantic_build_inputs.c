/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: ACCEPTANCE BAR for fxc_build_inputs() (tools/dev/devloop_facts_consumer.c),
 * the non-C/H build-input rule: (a) a TU-included path routes through the
 * existing header path; (b) a path the Makefile text names (by name,
 * pattern or sub-make) widens to the whole catalog; (c) a path no TU reads
 * and no Makefile names narrows the compile, but the plain plan's own path
 * groups for it are still selected as a test obligation; (d) a path the
 * include graph cannot answer for (here: deleted/never-created) widens too.
 *
 * Each test hand-builds a minimal identity+files manifest (no libclang) for
 * one candidate TU and drives zcl_devloop_facts_consume() directly against a
 * throwaway root, so it needs no compiler and no checked-in fixture.
 */
#include "test/test_core.h"

#include "base/safe_alloc.h"
#include "devloop.h"
#include "devloop_facts.h"
#include "test/semantic_facts_fixture.h"
#include "vcs/semantic_manifest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SBI_TU "src/a.c"

static bool sbi_mkdirs(char *full)
{
    for (char *p = full + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        if (mkdir(full, 0755) != 0 && errno != EEXIST) {
            *p = '/';
            return false;
        }
        *p = '/';
    }
    return true;
}

static bool sbi_write(const char *root, const char *rel, const char *body)
{
    char full[4096];
    FILE *fp;
    bool ok;
    size_t n = strlen(body);
    if (snprintf(full, sizeof(full), "%s/%s", root, rel) >= (int)sizeof(full))
        return false;
    ok = sbi_mkdirs(full);
    fp = ok ? fopen(full, "wb") : NULL;
    ok = fp != NULL && fwrite(body, 1, n, fp) == n;
    if (fp != NULL && fclose(fp) != 0)
        ok = false;
    return ok;
}

/* A minimal, valid manifest for `main_path`: identity + FILES (the main file
 * and, when given, one more repo file read). No lookups/macros/decls/
 * layouts/enums/functions/spans: every one of those sections is legally
 * empty. */
static const char *const k_sbi_env[] = {VCS_SEMANTIC_ENV_V1_ALLOWLIST};
#define SBI_ENV_COUNT (sizeof(k_sbi_env) / sizeof(k_sbi_env[0]))

static bool sbi_manifest(const char *main_path, const char *extra_file,
                         uint8_t **out, size_t *out_len)
{
    struct vcs_semantic_builder_v1 *b = vcs_semantic_builder_v1_new();
    struct vcs_semantic_record_v1 r = {0};
    uint8_t digest[32] = {0};
    char why[128] = {0};
    bool ok = b != NULL;
    if (ok) {
        /* fxi_object_cc_known() requires "; object-cc " followed by
         * something other than "unknown" (see
         * tools/dev/devloop_facts_codegen.c): without it, an otherwise
         * byte-identical before/after pair still reports identity-drift.
         * SFT_OBJECT_CC/SFT_TOOLCHAIN_ID are the same fixed, known stand-in
         * identity test_semantic_facts_live.c passes to the real sensor. */
        vcs_semantic_record_v1_cstr(
            &r, "test-cc 1.0; object-cc " SFT_OBJECT_CC
                " sha3-256 "
                "0000000000000000000000000000000000000000000000000000"
                "000000000000 toolchain " SFT_TOOLCHAIN_ID);
        vcs_semantic_record_v1_cstr(&r, "");
        vcs_semantic_record_v1_cstr(&r, "x86_64-pc-linux-gnu");
        vcs_semantic_record_v1_cstr(&r, main_path);
        vcs_semantic_record_v1_u32(&r, 0); /* argv */
        vcs_semantic_record_v1_u32(&r, 0); /* quote_dirs */
        vcs_semantic_record_v1_u32(&r, 0); /* angled_dirs */
        vcs_semantic_record_v1_u32(&r, 0); /* ignored_dirs */
        vcs_semantic_record_v1_u32(&r, (uint32_t)SBI_ENV_COUNT); /* env */
        for (size_t k = 0; k < SBI_ENV_COUNT; k++) {
            vcs_semantic_record_v1_cstr(&r, k_sbi_env[k]);
            vcs_semantic_record_v1_u8(&r, 0);
            vcs_semantic_record_v1_cstr(&r, "");
        }
        ok = vcs_semantic_builder_v1_add(b, VCS_SEMANTIC_SECTION_V1_IDENTITY,
                                         &r);
    }
    if (ok) {
        vcs_semantic_record_v1_reset(&r);
        vcs_semantic_record_v1_cstr(&r, main_path);
        vcs_semantic_record_v1_digest(&r, digest);
        vcs_semantic_record_v1_u8(&r, VCS_SEMANTIC_ORIGIN_V1_MAIN);
        ok = vcs_semantic_builder_v1_add(b, VCS_SEMANTIC_SECTION_V1_FILES, &r);
    }
    if (ok && extra_file != NULL) {
        vcs_semantic_record_v1_reset(&r);
        vcs_semantic_record_v1_cstr(&r, extra_file);
        vcs_semantic_record_v1_digest(&r, digest);
        vcs_semantic_record_v1_u8(&r, VCS_SEMANTIC_ORIGIN_V1_REPO);
        ok = vcs_semantic_builder_v1_add(b, VCS_SEMANTIC_SECTION_V1_FILES, &r);
    }
    if (ok) {
        struct vcs_semantic_facts_v1 facts = {
            .max_records = 64, .max_section_bytes = 65536, .revision = 2};
        ok = vcs_semantic_builder_v1_enable_facts(b, &facts);
    }
    ok = ok && vcs_semantic_builder_v1_finish(b, out, out_len, why, sizeof why);
    vcs_semantic_record_v1_free(&r);
    vcs_semantic_builder_v1_free(b);
    return ok;
}

/* Write a.c's before/after manifest pair (byte-identical: the TU itself
 * never changes in these tests), plus the candidate scan marker. */
static bool sbi_write_tu(const char *root, const char *extra_file)
{
    uint8_t *m = NULL;
    size_t n = 0;
    bool ok = sbi_manifest(SBI_TU, extra_file, &m, &n);
    char full[4096];
    FILE *fp;
    if (!ok)
        return false;
    for (int pass = 0; ok && pass < 2; pass++) {
        if (snprintf(full, sizeof(full), "%s/facts/%s%s", root, SBI_TU,
                     pass == 0 ? ".before.zsm" : ".after.zsm") >=
            (int)sizeof(full)) {
            ok = false;
            break;
        }
        ok = sbi_mkdirs(full);
        fp = ok ? fopen(full, "wb") : NULL;
        ok = fp != NULL && fwrite(m, 1, n, fp) == n;
        if (fp != NULL && fclose(fp) != 0)
            ok = false;
    }
    free(m);
    /* A depfile naming only its own .c gives codeindex zero include edges,
     * which is globally indistinguishable from "no include graph built"
     * (CODEINDEX_INCLUDE_DIM_UNAVAILABLE for every path, not just this
     * one): a second, in-tree prerequisite gives it a real edge so a path
     * with no readers gets answered COMPLETE instead. */
    return ok && sbi_write(root, SBI_TU, "int a(void) { return 0; }\n") &&
           sbi_write(root, "src/a.h", "\n") &&
           sbi_write(root, "build/a.d",
                     "build/a.o: " SBI_TU " src/a.h\n");
}

static bool sbi_has_group(const struct zcl_devloop_plan *p, const char *g)
{
    for (size_t k = 0; k < p->path_groups_len; k++)
        if (strcmp(p->path_groups[k], g) == 0)
            return true;
    for (size_t k = 0; k < p->closure_groups_len; k++)
        if (strcmp(p->closure_groups[k], g) == 0)
            return true;
    return false;
}

static bool sbi_tu_affected(const struct zcl_devloop_facts_report *r,
                            const char *why)
{
    for (size_t k = 0; k < r->ntus; k++)
        if (strcmp(r->tus[k].path, SBI_TU) == 0)
            return r->tus[k].affected &&
                  (why == NULL || strcmp(r->tus[k].reason, why) == 0);
    return false;
}

/* (c): a changed .def no TU reads and no Makefile names narrows the compile
 * (the TU is not widened into the universe by it), and the plain plan's own
 * path groups for that path still become a test obligation: not dropped by
 * the narrow walk. */
static int sbit_t_narrow(void)
{
    int failures = 0;
    TEST_CASE("semantic_build_inputs: an unread, unmentioned .def narrows "
             "the compile and keeps the path's own test groups") {
        char root[4096];
        const char *changed = "tools/dev/test_group_catalog.def";
        struct zcl_devloop_plan plan = {0};
        struct zcl_devloop_facts_verdict v = {0};
        struct zcl_devloop_facts_report rep = {0};
        ASSERT(test_mkdtemp(root, sizeof(root), "sbi_narrow") != NULL);
        ASSERT(sbi_write_tu(root, NULL));
        ASSERT(sbi_write(root, "Makefile", "all:\n\t@true\n"));
        ASSERT(sbi_write(root, changed, ""));
        ASSERT(zcl_devloop_plan_files(&changed, 1, &plan));
        ASSERT(zcl_devloop_facts_consume(root, &changed, 1, "facts", NULL,
                                         &plan, &v, &rep));
        ASSERT(rep.complete);
        ASSERT(!sbi_tu_affected(&rep, "build-input-changed"));
        ASSERT(!plan.closure_universal);
        /* the path itself matches a real shared impact rule
         * (test_group_catalog.def -> test_group_selector, ...): its group
         * must still be an obligation even though nothing compiles it. */
        ASSERT(sbi_has_group(&plan, "test_group_selector"));
        zcl_devloop_facts_report_free(&rep);
    } TEST_END
    return failures;
}

/* (b): a changed path the root Makefile text names (even though no TU or
 * depfile reads it) is a make input: the whole catalog widens. */
static int sbit_t_makefile_mention(void)
{
    int failures = 0;
    TEST_CASE("semantic_build_inputs: a path the Makefile text names widens "
             "to the whole catalog") {
        char root[4096];
        const char *changed = "cfg/flags.def";
        struct zcl_devloop_plan plan = {0};
        struct zcl_devloop_facts_verdict v = {0};
        struct zcl_devloop_facts_report rep = {0};
        ASSERT(test_mkdtemp(root, sizeof(root), "sbi_makefile") != NULL);
        ASSERT(sbi_write_tu(root, NULL));
        ASSERT(sbi_write(root, "Makefile",
                         "build/a.o: " SBI_TU " cfg/flags.def\n"
                         "\t$(CC) -c $< -o $@\n"));
        ASSERT(sbi_write(root, changed, ""));
        ASSERT(zcl_devloop_plan_files(&changed, 1, &plan));
        ASSERT(zcl_devloop_facts_consume(root, &changed, 1, "facts", NULL,
                                         &plan, &v, &rep));
        ASSERT(!rep.complete);
        ASSERT(strcmp(rep.reason, "build-input-changed") == 0);
        ASSERT(sbi_tu_affected(&rep, "build-input-changed"));
        ASSERT(plan.closure_universal);
        zcl_devloop_facts_report_free(&rep);
    } TEST_END
    return failures;
}

/* (d): a changed path the include graph refuses to answer for (it does not
 * exist on disk, so codeindex will not vouch for its reverse-includes)
 * keeps today's full fallback, exactly like a Makefile mention. */
static int sbit_t_truncated(void)
{
    int failures = 0;
    TEST_CASE("semantic_build_inputs: a path the include graph cannot "
             "answer for keeps the full fallback") {
        char root[4096];
        const char *changed = "cfg/ghost.def";
        struct zcl_devloop_plan plan = {0};
        struct zcl_devloop_facts_verdict v = {0};
        struct zcl_devloop_facts_report rep = {0};
        ASSERT(test_mkdtemp(root, sizeof(root), "sbi_trunc") != NULL);
        ASSERT(sbi_write_tu(root, NULL));
        ASSERT(sbi_write(root, "Makefile", "all:\n\t@true\n"));
        /* changed is never written to root: codeindex cannot verify it. */
        ASSERT(zcl_devloop_plan_files(&changed, 1, &plan));
        ASSERT(zcl_devloop_facts_consume(root, &changed, 1, "facts", NULL,
                                         &plan, &v, &rep));
        ASSERT(!rep.complete);
        ASSERT(strcmp(rep.reason, "build-input-changed") == 0);
        ASSERT(sbi_tu_affected(&rep, "build-input-changed"));
        ASSERT(plan.closure_universal);
        zcl_devloop_facts_report_free(&rep);
    } TEST_END
    return failures;
}

/* (a): a changed .def some TU's manifest lists as a file it read routes
 * through the existing header path (it is never classified as a build
 * input candidate at all, make-mentioned or not). */
static int sbit_t_header_path(void)
{
    int failures = 0;
    TEST_CASE("semantic_build_inputs: a .def a TU includes goes through "
             "the header path, not the build-input widening") {
        char root[4096];
        const char *changed = "cfg/read.def";
        struct zcl_devloop_plan plan = {0};
        struct zcl_devloop_facts_verdict v = {0};
        struct zcl_devloop_facts_report rep = {0};
        ASSERT(test_mkdtemp(root, sizeof(root), "sbi_header") != NULL);
        ASSERT(sbi_write_tu(root, changed));
        ASSERT(sbi_write(root, "Makefile",
                         "build/a.o: " SBI_TU " cfg/read.def\n"
                         "\t$(CC) -c $< -o $@\n"));
        ASSERT(sbi_write(root, changed, "X\n"));
        ASSERT(sbi_write(root, "facts/cfg/read.def.before", "X\n"));
        ASSERT(zcl_devloop_plan_files(&changed, 1, &plan));
        ASSERT(zcl_devloop_facts_consume(root, &changed, 1, "facts", NULL,
                                         &plan, &v, &rep));
        /* Read by a.c's manifest: never the build-input reason, even though
         * the Makefile also names it (the header path decides first). */
        ASSERT(!sbi_tu_affected(&rep, "build-input-changed"));
        zcl_devloop_facts_report_free(&rep);
    } TEST_END
    return failures;
}

int test_semantic_build_inputs(void)
{
    return sbit_t_narrow() | sbit_t_makefile_mention() |
          sbit_t_truncated() | sbit_t_header_path();
}
