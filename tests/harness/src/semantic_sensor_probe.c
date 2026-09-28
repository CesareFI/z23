/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: semantic_sensor checks that the conditional-lookup scan sees every probe the front end evaluates, a system header's own included, and that the sensor refuses the options it cannot read (-Wp, and MSVC compatibility).
 *
 * Each case emits one TU whose __has_include the scan could miss and
 * reads its LOOKUPS back: the probe must be there, replayed against its
 * search slots or recorded with no negative claim, so the facts consumer
 * widens on a created or deleted path. Each refusal case must fail the
 * emit with its reason. Part of the semantic_sensor group
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

/* What a matching record must be. */
enum ssp_want {
    SSP_ANY,     /* replayed or unbound */
    SSP_UNBOUND, /* no negative claim */
    SSP_BOUND,   /* replayed, and the TU records no unbound lookup at all */
};

struct ssp_case {
    const char *name;
    const char *main;
    const char *flags[3];
    const char *sys_header;  /* probe.h in an -isystem dir outside the root */
    const char *sys_header2; /* probe2.h there */
    bool repo_inc;           /* add -I <root>/inc, a repo dir */
    const char *want;        /* a substring of the probe's recorded name */
    enum ssp_want kind;
};

static const struct ssp_case k_ssp_cases[] = {
    {"a -D value's macro",
     "#if HAS(<opt.h>)\n" SSP_TAIL,
     {"-std=c23", "-DHAS(x)=__has_include(x)", NULL}, NULL, NULL, false,
     "__has_include", SSP_UNBOUND},
    {"a system header's macro body",
     "#include <probe.h>\n"
     "#if SYS_HAS(<opt.h>)\n" SSP_TAIL,
     {"-std=c23", NULL, NULL}, "#define SYS_HAS(x) __has_include(x)\n", NULL,
     false, "__has_include", SSP_UNBOUND},
    {"after an apostrophe that opens a character literal before C23",
     "#if 0\n"
     "int k = 1'a/*';\n"
     "#endif\n"
     "#if __has_include(\"opt.h\")\n" SSP_TAIL "/* end */\n",
     {"-std=c17", NULL, NULL}, NULL, NULL, false, "opt.h", SSP_ANY},
    {"a word a trigraph line splice joins",
     "#if __has_in?\?/\n"
     "clude(<opt.h>)\n" SSP_TAIL,
     {"-std=c17", NULL, NULL}, NULL, NULL, false, "opt.h", SSP_ANY},
    {"after a comment between the word and a header name holding /*",
     "#if __has_include(/**/<x/*y.h>)\n"
     "#endif\n"
     "#if __has_include(\"opt.h\")\n" SSP_TAIL "/* end */\n",
     {"-std=c23", NULL, NULL}, NULL, NULL, false, "opt.h", SSP_ANY},
    {"in a system header's own conditional, searching a repo -I dir first",
     "#include <probe.h>\n"
     "int f(void) { return 0; }\n",
     {"-std=c23", NULL, NULL}, "#if __has_include(<x.h>)\n#endif\n", NULL,
     true, "x.h", SSP_BOUND},
    {"in a system header behind #ifdef __has_include, as glibc guards it",
     "#include <probe.h>\n"
     "int f(void) { return 0; }\n",
     {"-std=c23", NULL, NULL},
     "#ifdef __has_include\n"
     "# if __has_include (\"x.h\")\n"
     "# endif\n"
     "#endif\n"
     "#if defined(__has_include) && defined __has_include_next\n"
     "#endif\n",
     NULL, true, "x.h", SSP_BOUND},
    {"as a __has_include_next in a system header another one includes",
     "#include <probe.h>\n"
     "int f(void) { return 0; }\n",
     {"-std=c23", NULL, NULL}, "#include <probe2.h>\n",
     "#if __has_include_next(<probe2.h>)\n#endif\n", true, "probe2.h",
     SSP_BOUND},
};

/* Options the scan cannot read: the emit must refuse with `why`. */
struct ssp_refusal {
    const char *name;
    const char *main;
    const char *flags[3];
    const char *why;
};

static const struct ssp_refusal k_ssp_refusals[] = {
    {"a -D value passed through -Wp,",
     "#if HAS(<opt.h>)\n" SSP_TAIL,
     {"-std=c23", "-Wp,-DHAS(x)=__has_include(x)", NULL},
     "indirect compiler options"},
    {"a -std passed through -Wp,",
     "#if __has_in?\?/\n"
     "clude(<opt.h>)\n" SSP_TAIL,
     {"-Wp,-std=c17", NULL, NULL}, "indirect compiler options"},
    {"-fms-compatibility, which turns trigraphs off",
     "// note ?\?/\n"
     "#if __has_include(<opt.h>)\n" SSP_TAIL,
     {"-std=c17", "-fms-compatibility", NULL}, "MSVC compatibility"},
    {"an MSVC target, which turns MSVC compatibility on",
     "// note ?\?/\n"
     "#if __has_include(<opt.h>)\n" SSP_TAIL,
     {"-std=c17", "--target=x86_64-pc-windows-msvc", NULL}, "MSVC target"},
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
    enum ssp_want kind;
    size_t hits;
    size_t unbound; /* every unbound record, matching or not */
};

static bool ssp_absent_cb(void *ctx, const char *dir, size_t dir_len,
                          const char *name, size_t name_len)
{
    struct ssp_seen *s = ctx;
    char spelled[PATH_MAX];
    bool match;
    (void)dir_len;
    (void)snprintf(spelled, sizeof(spelled), "%.*s", (int)name_len, name);
    match = strstr(spelled, s->want) != NULL;
    if (dir == NULL)
        s->unbound++;
    if (match && (s->kind == SSP_ANY ||
                  (s->kind == SSP_UNBOUND) == (dir == NULL)))
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
    char inc[PATH_MAX + 24];
    char sys[PATH_MAX + 8];
    char env_home[PATH_MAX + 16];
};

static bool ssp_layout(struct ssp_tree *t)
{
    if (test_mkdtemp(t->base, sizeof(t->base), "semsensor_probe") == NULL)
        return false;
    (void)snprintf(t->home, sizeof(t->home), "%s/fakehome", t->base);
    (void)snprintf(t->root, sizeof(t->root), "%s/root", t->home);
    (void)snprintf(t->inc, sizeof(t->inc), "%s/inc", t->root);
    (void)snprintf(t->sys, sizeof(t->sys), "%s/sys", t->base);
    (void)snprintf(t->env_home, sizeof(t->env_home), "HOME=%s", t->home);
    return mkdir(t->home, 0700) == 0 && mkdir(t->root, 0700) == 0 &&
           mkdir(t->inc, 0700) == 0 && mkdir(t->sys, 0700) == 0;
}

/* Run the sensor on main.c under flags (and -I <root>/inc when repo_inc);
 * out receives the manifest, message the merged output. */
static int ssp_run(const struct ssp_tree *t, const char *const flags[3],
                   bool repo_inc, const char *out, char *message,
                   size_t message_len)
{
    const char *argv[24] = {"env",      "-u",   "HOME",  t->env_home,
                            SSP_SENSOR, "emit", "--root", t->root,
                            "--source", "main.c", "--out", out, "--"};
    size_t n = 13;
    bool timed_out = false;
    int rc;
    if (repo_inc) {
        argv[n++] = "-I";
        argv[n++] = t->inc;
    }
    argv[n++] = "-isystem";
    argv[n++] = t->sys;
    for (size_t f = 0; f < 3 && flags[f] != NULL; f++)
        argv[n++] = flags[f];
    argv[n] = NULL;
    rc = zcl_spawn_capture_merged_observed(argv, message, message_len, 60000,
                                           &timed_out);
    return timed_out ? -1 : rc;
}

/* Emit main.c under the case's flags; the matching probe records. */
static bool ssp_emit(const struct ssp_tree *t, const struct ssp_case *k,
                     struct ssp_seen *seen)
{
    char out[PATH_MAX + 32], message[4096];
    size_t len = 0;
    uint8_t *m = NULL;
    int rc;
    bool ok;
    (void)snprintf(out, sizeof(out), "%s/main.zsm", t->base);
    rc = ssp_run(t, k->flags, k->repo_inc, out, message, sizeof(message));
    ok = rc == 0 && ssp_read(out, &m, &len) &&
         vcs_semantic_absent_v1_each(m, len, ssp_absent_cb, seen);
    if (!ok)
        printf("  probe %s: rc=%d: %s\n", k->name, rc, message);
    free(m);
    return ok;
}

static bool ssp_write_case(const struct ssp_tree *t, const struct ssp_case *k)
{
    return ssp_write(t->root, "main.c", k->main) &&
           (k->sys_header == NULL ||
            ssp_write(t->sys, "probe.h", k->sys_header)) &&
           (k->sys_header2 == NULL ||
            ssp_write(t->sys, "probe2.h", k->sys_header2));
}

static int ssp_t_case(const struct ssp_case *k)
{
    static const char *const kinds[] = {"lookup", "unbound", "replayed"};
    int failures = 0;
    struct ssp_tree t = {0};
    struct ssp_seen seen = {.want = k->want, .kind = k->kind};
    printf("semantic_sensor: the scan records a probe written %s", k->name);
    TEST_CASE("") {
        ASSERT(ssp_layout(&t));
        ASSERT(ssp_write_case(&t, k));
        ASSERT(ssp_emit(&t, k, &seen));
        if (seen.hits == 0)
            printf("  probe %s: no %s record naming \"%s\"\n", k->name,
                   kinds[k->kind], k->want);
        ASSERT(seen.hits > 0);
        if (k->kind == SSP_BOUND && seen.unbound != 0)
            printf("  probe %s: %zu unbound records\n", k->name, seen.unbound);
        ASSERT(k->kind != SSP_BOUND || seen.unbound == 0);
    } TEST_END
    if (t.base[0] != '\0')
        (void)test_rm_rf_recursive(t.base);
    return failures;
}

static int ssp_t_refusal(const struct ssp_refusal *k)
{
    int failures = 0;
    struct ssp_tree t = {0};
    char out[PATH_MAX + 32], message[4096] = "";
    int rc = 0;
    printf("semantic_sensor: the sensor refuses %s", k->name);
    TEST_CASE("") {
        ASSERT(ssp_layout(&t));
        ASSERT(ssp_write(t.root, "main.c", k->main));
        (void)snprintf(out, sizeof(out), "%s/main.zsm", t.base);
        rc = ssp_run(&t, k->flags, false, out, message, sizeof(message));
        if (rc == 0 || strstr(message, k->why) == NULL)
            printf("  refusal %s: rc=%d, want \"%s\": %s\n", k->name, rc,
                   k->why, message);
        ASSERT(rc > 0);
        ASSERT(strstr(message, k->why) != NULL);
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
    for (size_t k = 0; k < sizeof(k_ssp_refusals) / sizeof(k_ssp_refusals[0]);
         k++)
        failures += ssp_t_refusal(&k_ssp_refusals[k]);
    return failures;
}
