/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: semantic_sensor checks that the conditional-lookup scan sees every probe the front end evaluates: one a -D value or a system header's macro body writes, one after a pre-C23 apostrophe, one a trigraph splices, and one after a comment before a header name.
 *
 * Each case emits one TU whose __has_include the scan could miss and
 * reads its LOOKUPS back: the probe must be there, replayed against its
 * search slots or recorded with no negative claim, so the facts consumer
 * widens on a created or deleted path. Part of the semantic_sensor group
 * (test_semantic_manifest.c); runs only where build/bin/z23-clang-manifest
 * is built. */

#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "test/test_core.h"

#include "util/spawn.h"
#include "vcs/semantic_manifest.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SSP_SENSOR "build/bin/z23-clang-manifest"

int semantic_sensor_probe_tests(void);

/* The T0 tail every case's main.c ends with. */
#define SSP_TAIL                                                              \
    "#define T 1\n"                                                           \
    "#else\n"                                                                 \
    "#define T 0\n"                                                           \
    "#endif\n"                                                                \
    "int f(void) { return T; }\n"

struct ssp_case {
    const char *name;
    const char *main;
    const char *flags[3];
    const char *sys_header;  /* written to an -isystem dir outside the root */
    const char *want;        /* a substring of the probe's recorded name */
    bool unbound;            /* the record must make no negative claim */
};

static const struct ssp_case k_ssp_cases[] = {
    {"a -D value's macro",
     "#if HAS(<opt.h>)\n" SSP_TAIL,
     {"-std=c23", "-DHAS(x)=__has_include(x)", NULL}, NULL, "__has_include",
     true},
    {"a system header's macro body",
     "#include <probe.h>\n"
     "#if SYS_HAS(<opt.h>)\n" SSP_TAIL,
     {"-std=c23", NULL, NULL}, "#define SYS_HAS(x) __has_include(x)\n",
     "__has_include", true},
    {"after an apostrophe that opens a character literal before C23",
     "#if 0\n"
     "int k = 1'a/*';\n"
     "#endif\n"
     "#if __has_include(\"opt.h\")\n" SSP_TAIL "/* end */\n",
     {"-std=c17", NULL, NULL}, NULL, "opt.h", false},
    {"a word a trigraph line splice joins",
     "#if __has_in?\?/\n"
     "clude(<opt.h>)\n" SSP_TAIL,
     {"-std=c17", NULL, NULL}, NULL, "opt.h", false},
    {"after a comment between the word and a header name holding /*",
     "#if __has_include(/**/<x/*y.h>)\n"
     "#endif\n"
     "#if __has_include(\"opt.h\")\n" SSP_TAIL "/* end */\n",
     {"-std=c23", NULL, NULL}, NULL, "opt.h", false},
};

static bool ssp_write(const char *dir, const char *rel, const char *body)
{
    char path[PATH_MAX];
    FILE *fp;
    bool ok;
    if (snprintf(path, sizeof(path), "%s/%s", dir, rel) >= (int)sizeof(path))
        return false;
    fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    ok = fwrite(body, 1, strlen(body), fp) == strlen(body);
    return fclose(fp) == 0 && ok;
}

static bool ssp_read(const char *path, uint8_t **out, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    long n;
    bool ok;
    *out = NULL;
    *len = 0;
    if (fp == NULL)
        return false;
    ok = fseek(fp, 0, SEEK_END) == 0 && (n = ftell(fp)) > 0 &&
         fseek(fp, 0, SEEK_SET) == 0 && (*out = malloc((size_t)n)) != NULL &&
         fread(*out, 1, (size_t)n, fp) == (size_t)n;
    if (ok)
        *len = (size_t)n;
    (void)fclose(fp);
    return ok;
}

struct ssp_seen {
    const char *want;
    bool unbound;
    size_t hits;
};

static bool ssp_absent_cb(void *ctx, const char *dir, size_t dir_len,
                          const char *name, size_t name_len)
{
    struct ssp_seen *s = ctx;
    char spelled[PATH_MAX];
    (void)dir_len;
    (void)snprintf(spelled, sizeof(spelled), "%.*s", (int)name_len, name);
    if (strstr(spelled, s->want) != NULL && (!s->unbound || dir == NULL))
        s->hits++;
    return true;
}

/* A case's tree: base/fakehome/root holds main.c, base/sys an -isystem dir.
 * The sensor runs with HOME=base/fakehome, so base/sys is outside both the
 * checkout and the home and spells as @sys, as a real system dir does. */
struct ssp_tree {
    char base[PATH_MAX];
    char home[PATH_MAX + 8];
    char root[PATH_MAX + 16];
    char sys[PATH_MAX + 8];
    char env_home[PATH_MAX + 16];
};

static bool ssp_layout(struct ssp_tree *t)
{
    if (test_mkdtemp(t->base, sizeof(t->base), "semsensor_probe") == NULL)
        return false;
    (void)snprintf(t->home, sizeof(t->home), "%s/fakehome", t->base);
    (void)snprintf(t->root, sizeof(t->root), "%s/root", t->home);
    (void)snprintf(t->sys, sizeof(t->sys), "%s/sys", t->base);
    (void)snprintf(t->env_home, sizeof(t->env_home), "HOME=%s", t->home);
    return mkdir(t->home, 0700) == 0 && mkdir(t->root, 0700) == 0 &&
           mkdir(t->sys, 0700) == 0;
}

/* Emit main.c under the case's flags; the matching probe records. */
static bool ssp_emit(const struct ssp_tree *t, const struct ssp_case *k,
                     size_t *hits)
{
    char out[PATH_MAX + 32], message[4096];
    const char *argv[20] = {"env",      "-u",   "HOME",  t->env_home,
                            SSP_SENSOR, "emit", "--root", t->root,
                            "--source", "main.c", "--out", out, "--",
                            "-isystem", t->sys};
    size_t n = 15, len = 0;
    uint8_t *m = NULL;
    struct ssp_seen seen = {.want = k->want, .unbound = k->unbound};
    bool timed_out = false, ok;
    int rc;
    (void)snprintf(out, sizeof(out), "%s/main.zsm", t->base);
    for (size_t f = 0; f < 3 && k->flags[f] != NULL; f++)
        argv[n++] = k->flags[f];
    argv[n] = NULL;
    rc = zcl_spawn_capture_merged_observed(argv, message, sizeof(message),
                                           60000, &timed_out);
    ok = !timed_out && rc == 0 && ssp_read(out, &m, &len) &&
         vcs_semantic_absent_v1_each(m, len, ssp_absent_cb, &seen);
    if (!ok)
        printf("  probe %s: rc=%d: %s\n", k->name, rc, message);
    free(m);
    *hits = seen.hits;
    return ok;
}

static int ssp_t_case(const struct ssp_case *k)
{
    int failures = 0;
    struct ssp_tree t = {0};
    size_t hits = 0;
    printf("semantic_sensor: the scan records a probe written %s", k->name);
    TEST_CASE("") {
        ASSERT(ssp_layout(&t));
        ASSERT(ssp_write(t.root, "main.c", k->main));
        if (k->sys_header != NULL)
            ASSERT(ssp_write(t.sys, "probe.h", k->sys_header));
        ASSERT(ssp_emit(&t, k, &hits));
        if (hits == 0)
            printf("  probe %s: no %s record naming \"%s\"\n", k->name,
                   k->unbound ? "unbound" : "lookup", k->want);
        ASSERT(hits > 0);
    } TEST_END
    if (t.base[0] != '\0')
        (void)test_rm_rf_recursive(t.base);
    return failures;
}

int semantic_sensor_probe_tests(void)
{
    int failures = 0;
    for (size_t k = 0; k < sizeof(k_ssp_cases) / sizeof(k_ssp_cases[0]); k++)
        failures += ssp_t_case(&k_ssp_cases[k]);
    return failures;
}
