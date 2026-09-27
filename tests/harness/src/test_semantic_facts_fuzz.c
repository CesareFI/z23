/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * ACCEPTANCE BAR for the facts-narrowed change closure against a cold
 * compiler: a differential fuzzer. Each case is a small multi-TU C23
 * project before and after one edit, either generated from a seed
 * (semantic_fuzz_gen.c, deterministic from seed and profile) or one of the
 * fixed reproducers (semantic_fuzz_repro.c), every one of which runs every
 * time. For each case every TU is compiled (the clang of the LLVM the
 * sensor links, -std=c23 -O1 -ffunction-sections, the project's flags) and
 * sensed (build/bin/z23-clang-manifest emit --facts) on both sides, the
 * change set is planned in process from the facts directory exactly as
 * dev.change.plan {"facts":...} plans it, and two oracles judge the plan:
 *
 *   objects     every TU the plan leaves unaffected (or out of its
 *               universe) has a byte-identical object;
 *   symbols     in a changed object, every function and data object
 *               whose bytes changed, or whose relocations now address
 *               other content (a relocation into a section or a local
 *               symbol is resolved to the string, constant or object
 *               bytes it addresses; an addend that resolves to the same
 *               content does not count), is a seed of a narrowed plan by
 *               canonical id (or is covered by a fallback, or by a
 *               broadened TU after a header change), and so is every other
 *               symbol at its address.
 *
 * A false negative prints the case (seed, profile, mutation), the TU and
 * the function, and fails. The default run is a fixed list of 40 seeds;
 * ZCL_SEMANTIC_FUZZ_SEEDS=FIRST:COUNT[:PROFILE[:KIND]] runs a long range
 * instead (PROFILE all, no-ctr-line or gcc-deps; KIND forces one mutation
 * kind). The default seeds must yield narrowed plans that seed a changed
 * function, so the group cannot pass on fallbacks alone. Cases run in
 * forked children, SFZ_CASES_AT_ONCE at once, each handing its outcome
 * back over a pipe; reports print in case order. Self-skips,
 * visibly, where the sensor has not been built (`make clang-manifest`),
 * where no clang sits beside the libclang it links, or where the sensor is
 * not ELF. A reproducer marked known-RED names the fix it waits for and the
 * exact false-negative lines it reports until then: it holds only when it
 * fails with exactly those lines. Any other outcome fails the group: an
 * ERROR, another miss, or a PASS (the mark is stale and must go). A fixed
 * reproducer or default seed whose edit changes no file (NOOP) fails too;
 * only a ZCL_SEMANTIC_FUZZ_SEEDS range run may draw one.
 */

#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "test/test_core.h"

#include "test/semantic_fuzz.h"

#include "base/safe_alloc.h"

#include "platform/clock.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SFZ_SENSOR "build/bin/z23-clang-manifest"
/* Cases run SFZ_CASES_AT_ONCE at once, each compiling and sensing
 * SFZ_JOBS files at once: eight processes in all. */
#define SFZ_CASES_AT_ONCE 4
#define SFZ_JOBS 2
/* A case of at most eight TUs compiles and senses each twice, SFZ_JOBS
 * runs at once, every run bounded by its own deadline; a case whose
 * process has not reported after this is killed and is an ERROR. */
#define SFZ_CASE_BUDGET_S 240

enum sfz_profile { PROF_ALL, PROF_NO_CTR_LINE, PROF_GCC_DEPS, PROF_COUNT };

static const char *const k_profile_names[PROF_COUNT] = {"all", "no-ctr-line",
                                                     "gcc-deps"};

struct sfz_seed {
    uint64_t seed;
    enum sfz_profile profile;
    const char *kind; /* the mutation kind, NULL: drawn from the seed */
};

/* The default run: 40 (seed, profile, kind) cases covering every mutation
 * kind, most in the no-ctr-line profile where plans narrow, some whose
 * plans must fall back (flag, counter_c), and gcc depfiles for the probed
 * paths gcc omits. Chosen from a 4,900-case run at this rule set; each
 * passed there and most narrowed with seeds. */
static const struct sfz_seed k_default_seeds[] = {
    {20007, PROF_ALL, "hasinc"},
    {20015, PROF_ALL, "shadow"},
    {20029, PROF_ALL, "body_extern"},
    {20048, PROF_ALL, "counter_c"},
    {20055, PROF_ALL, "header_inline"},
    {20061, PROF_ALL, "flag"},
    {20122, PROF_ALL, "multi"},
    {20005, PROF_ALL, "macro_cond"},
    {20168, PROF_ALL, "macro_value"},
    {20259, PROF_ALL, "comment_ws"},
    {22402, PROF_GCC_DEPS, "shadow"},
    {22408, PROF_GCC_DEPS, "macro_new"},
    {22410, PROF_GCC_DEPS, "hasinc"},
    {22414, PROF_GCC_DEPS, "typedef"},
    {22422, PROF_GCC_DEPS, "hasinc"},
    {22426, PROF_GCC_DEPS, "header_const"},
    {22430, PROF_GCC_DEPS, "signature"},
    {22449, PROF_GCC_DEPS, "layout"},
    {23000, PROF_NO_CTR_LINE, "header_const"},
    {23003, PROF_NO_CTR_LINE, "layout"},
    {23004, PROF_NO_CTR_LINE, "macro_new"},
    {23005, PROF_NO_CTR_LINE, "body_static"},
    {23006, PROF_NO_CTR_LINE, "signature"},
    {23008, PROF_NO_CTR_LINE, "macro_value"},
    {23013, PROF_NO_CTR_LINE, "multi"},
    {23022, PROF_NO_CTR_LINE, "body_extern"},
    {23023, PROF_NO_CTR_LINE, "typedef"},
    {23027, PROF_NO_CTR_LINE, "body_static"},
    {23030, PROF_NO_CTR_LINE, "header_inline"},
    {23031, PROF_NO_CTR_LINE, "body_extern"},
    {23037, PROF_NO_CTR_LINE, "enum_value"},
    {23051, PROF_NO_CTR_LINE, "shadow"},
    {23053, PROF_NO_CTR_LINE, "comment_ws"},
    {23057, PROF_NO_CTR_LINE, "hasinc"},
    {23058, PROF_NO_CTR_LINE, "flag"},
    {23059, PROF_NO_CTR_LINE, "body_static"},
    {23061, PROF_NO_CTR_LINE, "body_extern"},
    {23062, PROF_NO_CTR_LINE, "multi"},
    {23073, PROF_NO_CTR_LINE, "body_static"},
    {23149, PROF_NO_CTR_LINE, "macro_cond"},
};

struct sfz_tally {
    size_t cases, pass, fail, noop, error, skipped, narrowed;
    size_t tus, predicted, changed, cfun, cobj, covered_seed, reloc;
    double plan_s;
};

struct sfz_group {
    struct sfz_env env;
    char scratch[PATH_MAX];
    struct sfz_tally t;
    bool kept; /* a failing case directory was left for inspection */
    bool noop_ok; /* a range run: an edit that changed nothing is no failure */
};

/* ---- discovery -------------------------------------------------------------- */

static bool executable(const char *path)
{
    return access(path, X_OK) == 0;
}

/* <prefix>/bin/clang for the first "<prefix>/lib" entry of the sensor's
 * runpath: the LLVM whose libclang the sensor parses with. */
static bool find_clang(const char *sensor, char *out, size_t n)
{
    char rp[PATH_MAX], *save = NULL;
    if (!sfz_elf_runpath(sensor, rp, sizeof(rp)))
        return false;
    for (char *d = strtok_r(rp, ":", &save); d != NULL;
         d = strtok_r(NULL, ":", &save)) {
        size_t len = strlen(d);
        if (len < 4 || strcmp(d + len - 4, "/lib") != 0)
            continue;
        if ((size_t)snprintf(out, n, "%.*s/bin/clang", (int)(len - 4), d) < n &&
            executable(out))
            return true;
    }
    return false;
}

static bool find_on_path(const char *name, char *out, size_t n)
{
    const char *path = getenv("PATH");
    char buf[4096], *save = NULL;
    if (path == NULL || (size_t)snprintf(buf, sizeof(buf), "%s", path) >= sizeof(buf))
        return false;
    for (char *d = strtok_r(buf, ":", &save); d != NULL;
         d = strtok_r(NULL, ":", &save))
        if ((size_t)snprintf(out, n, "%s/%s", d, name) < n && executable(out))
            return true;
    out[0] = '\0';
    return false;
}

/* The sensor, its clang and gcc; prints the SKIP line when absent. */
static bool discover(struct sfz_env *env)
{
    struct stat sb;
    memset(env, 0, sizeof(*env));
    env->jobs = SFZ_JOBS;
    if (stat(SFZ_SENSOR, &sb) != 0 || realpath(SFZ_SENSOR, env->sensor) == NULL) {
        printf("semantic_facts_fuzz: SKIP (needs %s from `make clang-manifest`)\n",
               SFZ_SENSOR);
        return false;
    }
    if (!find_clang(env->sensor, env->clang, sizeof(env->clang))) {
        printf("semantic_facts_fuzz: SKIP (no clang beside the libclang %s links, "
               "or it is not an ELF executable)\n", SFZ_SENSOR);
        return false;
    }
    (void)find_on_path("gcc", env->gcc, sizeof(env->gcc));
    return true;
}

/* ---- one case ------------------------------------------------------------------ */

static void tally(struct sfz_tally *t, const struct sfz_outcome *o)
{
    t->cases++;
    t->pass += o->status == SFZ_PASS;
    t->fail += o->status == SFZ_FAIL;
    t->noop += o->status == SFZ_NOOP;
    t->error += o->status == SFZ_ERROR;
    if (o->status != SFZ_PASS && o->status != SFZ_FAIL)
        return;
    t->narrowed += o->narrowed;
    t->tus += o->tus;
    t->predicted += o->predicted;
    t->changed += o->changed;
    t->cfun += o->cfun;
    t->cobj += o->cobj;
    t->covered_seed += o->covered_seed;
    t->reloc += o->reloc;
    t->plan_s += o->plan_s;
}

static const char *status_name(enum sfz_status s)
{
    static const char *const names[] = {"PASS", "FAIL", "NOOP", "ERROR"};
    return names[s];
}

static void report(const struct sfz_case *c, const struct sfz_outcome *o)
{
    printf("semantic_facts_fuzz: %s kind=%s %s narrowed=%s%s%s tus=%zu "
           "pred=%zu changed=%zu missed=%zu cfun=%zu cobj=%zu seeded=%zu reloc=%zu "
           "notcov=%zu seeds=%zu plan=%.2fs\n",
           c->label, c->kind, status_name(o->status), o->narrowed ? "yes" : "no",
           o->narrowed ? "" : " reason=", o->vreason, o->tus, o->predicted,
           o->changed, o->missed, o->cfun, o->cobj, o->covered_seed, o->reloc,
           o->notcov, o->seeds, o->plan_s);
    if (o->status != SFZ_PASS && o->status != SFZ_NOOP)
        printf("  detail: %s\n  case: %s\n%s", c->detail, c->dir, o->why);
}

static bool gcc_missing(struct sfz_group *g, const char *label)
{
    if (g->env.gcc[0] != '\0')
        return false;
    printf("semantic_facts_fuzz: %s SKIP (gcc-deps needs gcc on PATH)\n", label);
    g->t.skipped++;
    return true;
}

/* ---- the case pool: SFZ_CASES_AT_ONCE cases, each in its own process ---------- */

/* One case, prepared in the group's process and run in a child that hands
 * its outcome back over a pipe. */
struct sfz_item {
    struct sfz_case c;
    const char *known_red; /* a reproducer's known-RED mark */
    const char *known_red_why; /* ...and the exact lines it must report */
    bool run;              /* false: skipped, or failed before it could run */
    bool ok;               /* passed, or held as recorded (NOOP only in a
                              range run) */
    pid_t pid;
    int64_t deadline;      /* monotonic ns: killed and an ERROR after it */
    int fd;
    struct sfz_outcome o;
};

static bool write_all(int fd, const void *buf, size_t n)
{
    const uint8_t *p = buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0 && errno == EINTR)
            continue;
        if (w <= 0)
            return false;
        p += w;
        n -= (size_t)w;
    }
    return true;
}

/* Read n bytes from fd by `deadline` (monotonic ns): 1 read, 0 closed or
 * failed, -1 out of time. */
static int read_by(int fd, void *buf, size_t n, int64_t deadline)
{
    uint8_t *p = buf;
    while (n > 0) {
        int64_t left = (deadline - clock_now_monotonic_ns()) / 1000000;
        struct pollfd pfd = {.fd = fd, .events = POLLIN};
        int ready = left > 0 ? poll(&pfd, 1, left > INT_MAX ? INT_MAX : (int)left) : 0;
        ssize_t r;
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready == 0)
            return -1;
        r = ready > 0 ? read(fd, p, n) : -1;
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            return 0;
        p += r;
        n -= (size_t)r;
    }
    return 1;
}

static void item_start(const struct sfz_env *env, struct sfz_item *it)
{
    int p[2];
    it->pid = -1;
    it->fd = -1;
    if (pipe(p) != 0) {
        fprintf(stderr, "sfz: pipe for %s: %s\n", it->c.label, strerror(errno));
        return;
    }
    (void)fflush(stdout);
    (void)fflush(stderr);
    it->pid = fork();
    if (it->pid == 0) {
        (void)close(p[0]);
        (void)sfz_run_case(env, &it->c, &it->o);
        _exit(write_all(p[1], &it->o, sizeof(it->o)) ? 0 : 1);
    }
    (void)close(p[1]);
    if (it->pid < 0) {
        fprintf(stderr, "sfz: fork for %s: %s\n", it->c.label, strerror(errno));
        (void)close(p[0]);
        return;
    }
    it->fd = p[0];
    it->deadline = clock_now_monotonic_ns() + (int64_t)SFZ_CASE_BUDGET_S * 1000000000;
}

/* True when `why` is exactly the lines of `want`, each reported with the
 * two-space indent sfz_why() gives a false-negative line. */
static bool why_is(const char *why, const char *want)
{
    while (*want != '\0') {
        const char *eol = strchr(want, '\n');
        size_t len = eol != NULL ? (size_t)(eol - want) + 1 : strlen(want);
        if (strncmp(why, "  ", 2) != 0 || strncmp(why + 2, want, len) != 0)
            return false;
        why += 2 + len;
        want += len;
    }
    return *why == '\0';
}

/* A known-RED reproducer holds only when it fails with exactly its
 * recorded lines: an ERROR, a NOOP, another miss or a PASS (a stale mark)
 * each fail the group. */
static bool known_red_holds(const struct sfz_item *it)
{
    bool holds = it->o.status == SFZ_FAIL && it->known_red_why != NULL &&
                 why_is(it->o.why, it->known_red_why);
    if (holds)
        printf("semantic_facts_fuzz: %s KNOWN-RED as recorded, expected until %s\n",
               it->c.label, it->known_red);
    else if (it->o.status == SFZ_PASS)
        printf("semantic_facts_fuzz: %s now passes: its known-RED mark is stale, "
               "remove it\n", it->c.label);
    else
        printf("semantic_facts_fuzz: %s KNOWN-RED MISMATCH: expected FAIL "
               "reporting exactly:\n%s", it->c.label,
               it->known_red_why != NULL ? it->known_red_why : "(nothing recorded)\n");
    return holds;
}

/* Whether the finished item holds: a PASS, a known-RED reproducer failing
 * exactly as recorded, or a NOOP in a range run. */
static void judge_item(struct sfz_group *g, struct sfz_item *it)
{
    if (it->known_red != NULL)
        it->ok = known_red_holds(it);
    else
        it->ok = it->o.status == SFZ_PASS ||
                 (it->o.status == SFZ_NOOP && g->noop_ok);
    if (it->o.status == SFZ_NOOP && !it->ok)
        printf("semantic_facts_fuzz: %s NOOP fails: a fixed reproducer or default "
               "seed must change a file; only a range run may draw a no-op\n",
               it->c.label);
    /* a case that failed as recorded leaves nothing to inspect */
    if (it->known_red != NULL && it->ok)
        (void)test_rm_rf_recursive(it->c.dir);
    else
        g->kept = g->kept || it->o.status == SFZ_FAIL || it->o.status == SFZ_ERROR;
}

static void item_finish(struct sfz_group *g, struct sfz_item *it)
{
    int st = 0;
    int got = it->fd >= 0 ? read_by(it->fd, &it->o, sizeof(it->o), it->deadline) : 0;
    if (it->fd >= 0)
        (void)close(it->fd);
    /* out of time: its compile and sensor runs die with it (PDEATHSIG) */
    if (got < 0 && it->pid > 0)
        (void)kill(it->pid, SIGKILL);
    if (it->pid > 0 && waitpid(it->pid, &st, 0) != it->pid)
        fprintf(stderr, "sfz: waitpid for %s: %s\n", it->c.label, strerror(errno));
    if (got != 1) {
        memset(&it->o, 0, sizeof(it->o));
        it->o.status = SFZ_ERROR;
        if (got < 0)
            (void)snprintf(it->o.why, sizeof(it->o.why),
                           "the case process ran past its %d s budget and was "
                           "killed\n", SFZ_CASE_BUDGET_S);
        else
            (void)snprintf(it->o.why, sizeof(it->o.why),
                           "the case process did not report (status %d)\n", st);
    }
    tally(&g->t, &it->o);
    report(&it->c, &it->o);
    judge_item(g, it);
}

/* Run every runnable item, at most SFZ_CASES_AT_ONCE at once, reporting
 * them in order; the number that did not pass. */
static size_t run_items(struct sfz_group *g, struct sfz_item *v, size_t n)
{
    size_t ring[SFZ_CASES_AT_ONCE], head = 0, live = 0, bad = 0;
    for (size_t k = 0; k < n; k++) {
        if (!v[k].run)
            continue;
        if (live == SFZ_CASES_AT_ONCE) {
            item_finish(g, &v[ring[head]]);
            head = (head + 1) % SFZ_CASES_AT_ONCE;
            live--;
        }
        item_start(&g->env, &v[k]);
        ring[(head + live++) % SFZ_CASES_AT_ONCE] = k;
    }
    for (; live > 0; live--, head = (head + 1) % SFZ_CASES_AT_ONCE)
        item_finish(g, &v[ring[head]]);
    for (size_t k = 0; k < n; k++)
        bad += !v[k].ok;
    return bad;
}

/* ---- the fixed reproducers ------------------------------------------------------ */

static void prep_repro(struct sfz_group *g, const struct sfz_repro *r,
                       struct sfz_item *it)
{
    struct sfz_case *c = &it->c;
    c->gcc_deps = r->gcc_deps;
    (void)snprintf(c->label, sizeof(c->label), "%s", r->name);
    (void)snprintf(c->dir, sizeof(c->dir), "%s/%s", g->scratch, r->name);
    (void)snprintf(c->kind, sizeof(c->kind), "%s", r->kind);
    (void)snprintf(c->detail, sizeof(c->detail), "%s", r->detail);
    it->known_red = r->known_red;
    it->known_red_why = r->known_red_why;
    it->ok = true;
    if (r->gcc_deps && gcc_missing(g, r->name))
        return;
    it->ok = sfz_write_repro(r, c->dir);
    it->run = it->ok;
    if (!it->ok)
        printf("semantic_facts_fuzz: %s ERROR: cannot write the reproducer\n",
               r->name);
}

static int sfz_t_repros(struct sfz_group *g)
{
    int failures = 0;
    struct sfz_item *v = zcl_calloc(k_sfz_nrepros, sizeof(*v), "sfz.repros");
    TEST_CASE("semantic_facts_fuzz: every fixed reproducer plans safely") {
        ASSERT(v != NULL);
        for (size_t k = 0; k < k_sfz_nrepros; k++)
            prep_repro(g, &k_sfz_repros[k], &v[k]);
        ASSERT_EQ(run_items(g, v, k_sfz_nrepros), (size_t)0);
    } TEST_END
    free(v);
    return failures;
}

/* ---- the seeds ------------------------------------------------------------------- */

static unsigned profile_bits(enum sfz_profile p)
{
    return p == PROF_NO_CTR_LINE ? SFZ_NO_CTR | SFZ_NO_LINE : 0u;
}

static void prep_seed(struct sfz_group *g, struct sfz_seed s, struct sfz_item *it)
{
    struct sfz_case *c = &it->c;
    struct sfz_meta meta;
    c->gcc_deps = s.profile == PROF_GCC_DEPS;
    (void)snprintf(c->label, sizeof(c->label), "seed=%llu profile=%s",
                   (unsigned long long)s.seed, k_profile_names[s.profile]);
    (void)snprintf(c->dir, sizeof(c->dir), "%s/seed_%llu_%s", g->scratch,
                   (unsigned long long)s.seed, k_profile_names[s.profile]);
    it->ok = true;
    if (c->gcc_deps && gcc_missing(g, c->label))
        return;
    it->ok = sfz_generate(s.seed, profile_bits(s.profile), s.kind, c->dir, &meta);
    it->run = it->ok;
    if (!it->ok) {
        printf("semantic_facts_fuzz: %s ERROR: the generator failed\n", c->label);
        return;
    }
    (void)snprintf(c->kind, sizeof(c->kind), "%s", meta.kind);
    (void)snprintf(c->detail, sizeof(c->detail), "%s", meta.detail);
}

/* ZCL_SEMANTIC_FUZZ_SEEDS=FIRST:COUNT[:PROFILE[:KIND]]: COUNT consecutive
 * seeds from FIRST, the kind drawn unless KIND names one. */
struct sfz_range {
    uint64_t first;
    size_t count;
    enum sfz_profile profile;
    char kind[32];
};

/* False when unset; *bad when set but malformed. */
static bool seeds_override(struct sfz_range *r, bool *bad)
{
    const char *v = getenv("ZCL_SEMANTIC_FUZZ_SEEDS");
    char prof[32] = "all";
    unsigned long long f = 0, n = 0;
    int got;
    memset(r, 0, sizeof(*r));
    *bad = false;
    if (v == NULL || v[0] == '\0')
        return false;
    got = sscanf(v, "%llu:%llu:%31[^:]:%31s", &f, &n, prof, r->kind);
    r->first = f;
    r->count = (size_t)n;
    r->profile = PROF_COUNT;
    for (int p = 0; p < PROF_COUNT; p++)
        if (strcmp(prof, k_profile_names[p]) == 0)
            r->profile = (enum sfz_profile)p;
    *bad = got < 2 || n == 0 || r->profile == PROF_COUNT ||
           (got == 4 && !sfz_kind_known(r->kind));
    return true;
}

static int sfz_t_seeds(struct sfz_group *g)
{
    int failures = 0;
    size_t n = sizeof(k_default_seeds) / sizeof(k_default_seeds[0]);
    struct sfz_range r;
    bool bad_env = false, custom = seeds_override(&r, &bad_env);
    size_t narrowed0 = g->t.narrowed, seeded0 = g->t.covered_seed;
    struct sfz_item *v = NULL;
    TEST_CASE("semantic_facts_fuzz: generated seeds plan safely") {
        /* FIRST:COUNT[:all|no-ctr-line|gcc-deps[:KIND]] */
        ASSERT(!bad_env);
        n = custom ? r.count : n;
        g->noop_ok = custom;
        v = zcl_calloc(n, sizeof(*v), "sfz.seeds");
        ASSERT(v != NULL);
        for (size_t k = 0; k < n; k++)
            prep_seed(g, custom ? (struct sfz_seed){r.first + k, r.profile,
                                                    r.kind[0] ? r.kind : NULL}
                                : k_default_seeds[k],
                      &v[k]);
        ASSERT_EQ(run_items(g, v, n), (size_t)0);
        /* the default seeds must exercise narrowed plans that seed a
         * changed function, not only fallbacks */
        ASSERT(custom || g->t.narrowed > narrowed0);
        ASSERT(custom || g->t.covered_seed > seeded0);
    } TEST_END
    free(v);
    return failures;
}

int test_semantic_facts_fuzz(void)
{
    int failures = 0;
    struct sfz_group *g = zcl_calloc(1, sizeof(*g), "sfz.group");
    int64_t t0 = clock_now_monotonic_ns();
    if (g == NULL)
        return 1;
    if (!discover(&g->env)) {
        free(g);
        return 0;
    }
    TEST_CASE("semantic_facts_fuzz: scratch") {
        ASSERT(test_mkdtemp(g->scratch, sizeof(g->scratch), "semfacts_fuzz") != NULL);
    } TEST_END
    if (failures == 0) {
        failures += sfz_t_repros(g);
        failures += sfz_t_seeds(g);
    }
    printf("semantic_facts_fuzz: %zu cases (%zu fixed) in %.1f s: %zu pass, %zu "
           "fail, %zu noop, %zu error, %zu skipped; %zu narrowed verdicts; %zu "
           "TUs, %zu predicted, %zu changed; %zu changed functions, %zu changed "
           "data objects, %zu seeded, "
           "%zu relocation-only; planning %.1f s\n",
           g->t.cases, k_sfz_nrepros,
           (double)(clock_now_monotonic_ns() - t0) / 1e9, g->t.pass, g->t.fail,
           g->t.noop, g->t.error, g->t.skipped, g->t.narrowed, g->t.tus,
           g->t.predicted, g->t.changed, g->t.cfun, g->t.cobj, g->t.covered_seed,
           g->t.reloc, g->t.plan_s);
    if (failures == 0 && !g->kept)
        (void)test_rm_rf_recursive(g->scratch);
    free(g);
    return failures;
}
