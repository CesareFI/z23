/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * z23-sem-replay: one replayed commit. For commit C with parent P:
 *
 *   1. check out P, make every test-fast object, snapshot the objects and
 *      sense the TUs whose depfile names a changed file (before side);
 *   2. check out C, make again (the measured incremental build), snapshot,
 *      and sense the same bound at C (after side);
 *   3. plan the change with dev.change.plan, plain and with the facts;
 *   4. compare: what make recompiled, which object bytes changed, the plain
 *      compile set, the facts compile set, and every changed object the
 *      facts plan left out (a false negative).
 *
 * The step writes run/<NN>_<C>/result.tsv and the sets behind it. */
#include "sem_replay_step.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sem_replay_build.h"
#include "sem_replay_change.h"
#include "sem_replay_plan.h"

#define SR_FACTS_REL "build/sem-replay/facts"

/* ── small helpers ────────────────────────────────────────────────────── */

static char **steal_argv(struct sr_strv *s)
{
    if (!sr_strv_pushn(s, "", 0))
        return NULL;
    free(s->v[s->n - 1]);
    s->v[s->n - 1] = NULL;
    char **v = s->v;
    memset(s, 0, sizeof(*s));
    return v;
}

static const char *basename_of(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

/* Words after the compiler words (the leading words without a '-'). */
static size_t first_flag(const struct sr_strv *flags)
{
    size_t i = 0;
    while (i < flags->n && flags->v[i][0] != '-')
        i++;
    return i;
}

static bool set_union(const struct sr_strv *a, const struct sr_strv *b,
                      struct sr_strv *out)
{
    bool ok = true;
    for (size_t i = 0; ok && i < a->n; i++)
        ok = sr_strv_push(out, a->v[i]);
    for (size_t i = 0; ok && i < b->n; i++)
        ok = sr_strv_push(out, b->v[i]);
    sr_strv_sort_unique(out);
    return ok;
}

/* out = a ∩ b, or a \ b when minus. */
static bool set_filter(const struct sr_strv *a, const struct sr_strv *b,
                       bool minus, struct sr_strv *out)
{
    bool ok = true;
    for (size_t i = 0; ok && i < a->n; i++)
        if (sr_strv_has(b, a->v[i]) != minus)
            ok = sr_strv_push(out, a->v[i]);
    sr_strv_sort_unique(out);
    return ok;
}

static bool snap_tus(const struct sr_snap *s, struct sr_strv *out)
{
    bool ok = true;
    for (size_t i = 0; ok && i < s->n; i++)
        ok = sr_strv_push(out, s->v[i].tu);
    sr_strv_sort_unique(out);
    return ok;
}

/* ── make ─────────────────────────────────────────────────────────────── */

static bool fast_target(const struct sr_cfg *cfg, const char *log, char out[SR_PATH])
{
    char *argv[] = {"make", "-s", "--no-print-directory", "--eval",
                    "zsr-print: ; @echo ZSR_TARGET=$(TEST_PARALLEL_FAST_CANDIDATE)",
                    "zsr-print", NULL};
    char *text = NULL;
    size_t len = 0;
    int rc = sr_capture(argv, cfg->repo, log, &text, &len);
    const char *hit = text ? strstr(text, "ZSR_TARGET=") : NULL;
    bool ok = rc == 0 && hit != NULL;
    if (ok) {
        snprintf(out, SR_PATH, "%s", hit + 8);
        out[strcspn(out, "\r\n")] = '\0';
        ok = out[0] != '\0';
    }
    if (!ok)
        fprintf(stderr, "sem-replay: cannot name the test-fast binary (make %d)\n", rc);
    free(text);
    return ok;
}

bool sr_make_objects(const struct sr_cfg *cfg, const char *log,
                     const struct sr_strv *what_if, struct sr_cost *cost)
{
    char rsp[SR_PATH], jobs[16];
    if (!fast_target(cfg, log, rsp))
        return false;
    snprintf(jobs, sizeof(jobs), "-j%d", cfg->jobs);
    struct sr_strv cmd = {0};
    bool ok = sr_strv_push(&cmd, "make") && sr_strv_push(&cmd, "--no-print-directory") &&
              sr_strv_push(&cmd, "-k") && sr_strv_push(&cmd, jobs);
    for (size_t i = 0; ok && what_if && i < what_if->n; i++)
        ok = sr_strv_push(&cmd, "-W") && sr_strv_push(&cmd, what_if->v[i]);
    ok = ok && sr_strv_push(&cmd, rsp);
    char **argv = ok ? steal_argv(&cmd) : NULL;
    char *env[] = {"ZCC_DISABLE=1", NULL};
    int rc = argv ? sr_run(argv, cfg->repo, log, env, cost) : -1;
    for (size_t i = 0; argv && argv[i]; i++)
        free(argv[i]);
    free(argv);
    sr_strv_free(&cmd);
    if (rc != 0)
        fprintf(stderr, "sem-replay: make exited %d (see %s)\n", rc, log);
    return rc == 0;
}

/* ── sensing and timed compiles ───────────────────────────────────────── */

static char **sensor_argv(const struct sr_cfg *cfg, const char *tu,
                          const char *side, const struct sr_strv *flags)
{
    char out[SR_PATH];
    snprintf(out, sizeof(out), "%s/%s/%s.%s.zsm", cfg->repo, SR_FACTS_REL, tu, side);
    if (!sr_mkparent(out))
        return NULL;
    struct sr_strv a = {0};
    bool ok = sr_strv_push(&a, cfg->sensor) && sr_strv_push(&a, "emit") &&
              sr_strv_push(&a, "--root") && sr_strv_push(&a, ".") &&
              sr_strv_push(&a, "--source") && sr_strv_push(&a, tu) &&
              sr_strv_push(&a, "--out") && sr_strv_push(&a, out) &&
              sr_strv_push(&a, "--facts") && sr_strv_push(&a, "--");
    for (size_t i = first_flag(flags); ok && i < flags->n; i++)
        ok = sr_strv_push(&a, flags->v[i]);
    char **v = ok ? steal_argv(&a) : NULL;
    sr_strv_free(&a);
    return v;
}

static char **compile_argv(const struct sr_strv *flags, const char *tu,
                           const char *obj)
{
    char dep[SR_PATH];
    snprintf(dep, sizeof(dep), "%s.d", obj);
    size_t i = flags->n > 0 && strcmp(basename_of(flags->v[0]), "zcc") == 0 ? 1 : 0;
    struct sr_strv a = {0};
    bool ok = true;
    for (; ok && i < flags->n; i++)
        ok = sr_strv_push(&a, flags->v[i]);
    ok = ok && sr_strv_push(&a, "-c") && sr_strv_push(&a, tu) &&
         sr_strv_push(&a, "-o") && sr_strv_push(&a, obj) &&
         sr_strv_push(&a, "-MMD") && sr_strv_push(&a, "-MP") &&
         sr_strv_push(&a, "-MF") && sr_strv_push(&a, dep);
    char **v = ok ? steal_argv(&a) : NULL;
    sr_strv_free(&a);
    return v;
}

struct batch {
    struct sr_task *t;
    char **side; /* "before", "after" or "compile" per task */
    char **obj;  /* compile output per task, or NULL */
    size_t n, cap;
};

static bool batch_add(struct batch *b, const char *tu, char **argv,
                      const char *side, const char *obj, const char *log)
{
    if (argv == NULL)
        return false;
    if (b->n == b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 256;
        struct sr_task *t = realloc(b->t, cap * sizeof(*t));
        char **s = t ? realloc(b->side, cap * sizeof(*s)) : NULL;
        char **o = s ? realloc(b->obj, cap * sizeof(*o)) : NULL;
        b->t = t ? t : b->t;
        b->side = s ? s : b->side;
        b->obj = o ? o : b->obj;
        if (o == NULL) {
            fprintf(stderr, "sem-replay: out of memory growing a batch\n");
            return false;
        }
        b->cap = cap;
    }
    b->t[b->n] = (struct sr_task){.tu = tu, .argv = argv, .log = log, .rc = -1};
    b->side[b->n] = (char *)side;
    b->obj[b->n] = obj ? strdup(obj) : NULL;
    b->n++;
    return true;
}

static void batch_free(struct batch *b)
{
    for (size_t i = 0; i < b->n; i++) {
        sr_task_free(&b->t[i]);
        free(b->obj[i]);
    }
    free(b->t);
    free(b->side);
    free(b->obj);
    memset(b, 0, sizeof(*b));
}

static bool add_sensing(const struct sr_cfg *cfg, struct batch *b,
                        const struct sr_strv *tus, const struct sr_argv_map *m,
                        const char *side, const char *log)
{
    for (size_t i = 0; i < tus->n; i++) {
        const struct sr_strv *flags = sr_argv_find(m, tus->v[i]);
        if (flags == NULL) {
            fprintf(stderr, "sem-replay: no compile argv for %s (%s)\n", tus->v[i], side);
            continue; /* the planner sees no manifest and falls back */
        }
        if (!batch_add(b, tus->v[i], sensor_argv(cfg, tus->v[i], side, flags),
                       side, NULL, log))
            return false;
    }
    return true;
}

static bool add_compiles(struct batch *b, const struct sr_strv *tus,
                         const struct sr_argv_map *m, const char *tmpdir,
                         const char *log)
{
    for (size_t i = 0; i < tus->n; i++) {
        const struct sr_strv *flags = sr_argv_find(m, tus->v[i]);
        char obj[SR_PATH];
        if (flags == NULL)
            continue;
        snprintf(obj, sizeof(obj), "%s/%zu.o", tmpdir, i);
        if (!batch_add(b, tus->v[i], compile_argv(flags, tus->v[i], obj),
                       "compile", obj, log))
            return false;
    }
    return true;
}

/* ── per-commit state ─────────────────────────────────────────────────── */

struct commit_run {
    char parent[64];
    char dir[SR_PATH];
    char log[SR_PATH];
    struct sr_change ch;
    struct sr_snap snap_p, snap_c;
    struct sr_strv tus_c, bound_p, bound_c;
    struct sr_strv make_set, changed, removed, plain, facts, fn, plain_fn;
    struct sr_plan plain_plan, facts_plan;
    const char *facts_mode;
    struct sr_cost build_c;
    size_t sense_n, sense_fail;
    double sense_cpu, sense_wall;
    size_t repro_rebuilt, repro_mismatch, cold_checked, cold_mismatch;
    double cpu_make, cpu_facts, cpu_changed, saved_make, saved_plain;
    size_t cost_missing;
    bool build_failed;
};

static void commit_run_free(struct commit_run *r)
{
    sr_change_free(&r->ch);
    sr_snap_free(&r->snap_p);
    sr_snap_free(&r->snap_c);
    struct sr_strv *sets[] = {&r->tus_c, &r->bound_p, &r->bound_c, &r->make_set,
                              &r->changed, &r->removed, &r->plain, &r->facts,
                              &r->fn, &r->plain_fn};
    for (size_t i = 0; i < sizeof(sets) / sizeof(sets[0]); i++)
        sr_strv_free(sets[i]);
    sr_plan_free(&r->plain_plan);
    sr_plan_free(&r->facts_plan);
}

/* ── compile cost table ───────────────────────────────────────────────── */

struct cost_row {
    char *tu;
    double cpu;
};

struct cost_table {
    struct cost_row *v;
    size_t n, cap;
};

static int cmp_cost(const void *a, const void *b)
{
    return strcmp(((const struct cost_row *)a)->tu, ((const struct cost_row *)b)->tu);
}

static bool cost_push(struct cost_table *t, const char *tu, double cpu)
{
    if (t->n == t->cap) {
        size_t cap = t->cap ? t->cap * 2 : 1024;
        struct cost_row *v = realloc(t->v, cap * sizeof(*v));
        if (v == NULL) {
            fprintf(stderr, "sem-replay: out of memory in the cost table\n");
            return false;
        }
        t->v = v;
        t->cap = cap;
    }
    t->v[t->n].tu = strdup(tu);
    t->v[t->n].cpu = cpu;
    return t->v[t->n++].tu != NULL;
}

static bool cost_load(struct cost_table *t, const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[SR_PATH + 128], tu[SR_PATH];
    double cpu = 0;
    bool ok = true;
    while (fp != NULL && ok && fgets(line, sizeof(line), fp) != NULL)
        if (sscanf(line, "%4095s\t%lf", tu, &cpu) == 2)
            ok = cost_push(t, tu, cpu);
    if (fp != NULL)
        fclose(fp);
    if (t->n > 1)
        qsort(t->v, t->n, sizeof(*t->v), cmp_cost);
    return ok;
}

static const struct cost_row *cost_find(const struct cost_table *t, const char *tu)
{
    struct cost_row key = {.tu = (char *)tu};
    return t->n ? bsearch(&key, t->v, t->n, sizeof(*t->v), cmp_cost) : NULL;
}

static void cost_free(struct cost_table *t)
{
    for (size_t i = 0; i < t->n; i++)
        free(t->v[i].tu);
    free(t->v);
    memset(t, 0, sizeof(*t));
}

/* Sum of the known compile CPU over s; missing counts TUs without a row. */
static double cost_sum(const struct cost_table *t, const struct sr_strv *s,
                       size_t *missing)
{
    double sum = 0;
    for (size_t i = 0; i < s->n; i++) {
        const struct cost_row *row = cost_find(t, s->v[i]);
        if (row)
            sum += row->cpu;
        else if (missing)
            (*missing)++;
    }
    return sum;
}

/* ── the before side ──────────────────────────────────────────────────── */

static void path_in(char *out, size_t n, const struct sr_cfg *cfg, const char *name)
{
    snprintf(out, n, "%s/%s", cfg->state, name);
}

static void repro_compare(const struct sr_snap *prev, const struct sr_snap *now,
                          size_t *rebuilt, size_t *mismatch, FILE *log)
{
    if (strcmp(prev->epoch, now->epoch) != 0)
        return;
    for (size_t i = 0; i < now->n; i++) {
        const struct sr_obj *o = &now->v[i], *p = sr_snap_find(prev, o->tu);
        if (p == NULL || (p->ino == o->ino && p->mtime_ns == o->mtime_ns))
            continue;
        (*rebuilt)++;
        if (memcmp(p->hash, o->hash, 32) != 0) {
            (*mismatch)++;
            if (log)
                fprintf(log, "repro-mismatch\t%s\n", o->tu);
        }
    }
}

static bool sense_batch(const struct sr_cfg *cfg, struct commit_run *r,
                        struct batch *b)
{
    if (b->n == 0)
        return true;
    bool ok = sr_pool_run(b->t, b->n, cfg->jobs, cfg->repo);
    char path[SR_PATH];
    snprintf(path, sizeof(path), "%s/tasks.tsv", r->dir);
    FILE *fp = fopen(path, "a");
    for (size_t i = 0; i < b->n; i++) {
        bool sense = b->obj[i] == NULL;
        if (fp)
            fprintf(fp, "%s\t%s\t%.4f\t%.4f\t%d\n", b->side[i], b->t[i].tu,
                    b->t[i].wall_s, b->t[i].cpu_s, b->t[i].rc);
        if (!sense)
            continue;
        r->sense_n++;
        r->sense_fail += b->t[i].rc != 0;
        r->sense_cpu += b->t[i].cpu_s;
        r->sense_wall += b->t[i].wall_s;
    }
    if (fp)
        fclose(fp);
    return ok;
}

static bool sense_side(const struct sr_cfg *cfg, struct commit_run *r,
                       const struct sr_strv *tus, const char *epoch,
                       const char *side)
{
    struct sr_argv_map m;
    struct batch b = {0};
    bool ok = sr_make_argv(cfg->repo, epoch, tus, r->log, &m) &&
              add_sensing(cfg, &b, tus, &m, side, r->log) && sense_batch(cfg, r, &b);
    batch_free(&b);
    sr_argv_free(&m);
    return ok;
}

static bool save_before_sources(const struct sr_cfg *cfg, struct commit_run *r)
{
    bool ok = true;
    for (size_t i = 0; ok && i < r->ch.files.n; i++) {
        const char *f = r->ch.files.v[i];
        char dest[SR_PATH];
        if (sr_strv_has(&r->ch.added, f))
            continue;
        snprintf(dest, sizeof(dest), "%s/%s/%s.before", cfg->repo, SR_FACTS_REL, f);
        ok = sr_git_blob(cfg->repo, r->parent, f, dest);
    }
    return ok;
}

static bool reset_facts(const struct sr_cfg *cfg)
{
    char dir[SR_PATH];
    snprintf(dir, sizeof(dir), "%s/%s", cfg->repo, SR_FACTS_REL);
    return sr_rmtree(dir) && sr_mkdirs(dir);
}

static bool before_side(const struct sr_cfg *cfg, struct commit_run *r)
{
    char last_snap[SR_PATH], last_commit[SR_PATH], *lc = NULL;
    size_t lcn = 0, missing = 0;
    struct sr_snap prev = {0};
    path_in(last_snap, sizeof(last_snap), cfg, "last.snap");
    path_in(last_commit, sizeof(last_commit), cfg, "last.commit");
    bool have_prev = sr_snap_load(&prev, last_snap) && sr_read_file(last_commit, &lc, &lcn);
    bool ok = sr_git_checkout(cfg->repo, r->parent, r->log) &&
              sr_make_objects(cfg, r->log, NULL, NULL) &&
              sr_snap_take(cfg->repo, have_prev ? &prev : NULL, &r->snap_p);
    if (ok && have_prev && strncmp(lc, r->parent, strlen(r->parent)) == 0)
        repro_compare(&prev, &r->snap_p, &r->repro_rebuilt, &r->repro_mismatch, stderr);
    free(lc);
    sr_snap_free(&prev);
    ok = ok && sr_deps_hits(cfg->repo, &r->snap_p, &r->ch.files, &r->bound_p, &missing) &&
         reset_facts(cfg) && sense_side(cfg, r, &r->bound_p, r->snap_p.epoch, "before") &&
         save_before_sources(cfg, r);
    if (missing)
        fprintf(stderr, "sem-replay: %zu objects at %s have no depfile\n", missing, r->parent);
    return ok;
}

/* ── the after side ───────────────────────────────────────────────────── */

static bool compare_snaps(struct commit_run *r)
{
    bool epoch_moved = strcmp(r->snap_p.epoch, r->snap_c.epoch) != 0;
    bool ok = true;
    for (size_t i = 0; ok && i < r->snap_c.n; i++) {
        const struct sr_obj *o = &r->snap_c.v[i], *p = sr_snap_find(&r->snap_p, o->tu);
        bool rebuilt = epoch_moved || p == NULL || p->ino != o->ino || p->mtime_ns != o->mtime_ns;
        if (rebuilt)
            ok = sr_strv_push(&r->make_set, o->tu);
        if (ok && (p == NULL || memcmp(p->hash, o->hash, 32) != 0))
            ok = sr_strv_push(&r->changed, o->tu);
    }
    for (size_t i = 0; ok && i < r->snap_p.n; i++)
        if (sr_snap_find(&r->snap_c, r->snap_p.v[i].tu) == NULL)
            ok = sr_strv_push(&r->removed, r->snap_p.v[i].tu);
    if (epoch_moved)
        r->ch.kind = "build-system";
    return ok;
}

static void record_compiles(const struct sr_cfg *cfg, struct commit_run *r,
                            const struct batch *b)
{
    char path[SR_PATH];
    path_in(path, sizeof(path), cfg, "compile_cost.tsv");
    FILE *fp = fopen(path, "a");
    for (size_t i = 0; i < b->n; i++) {
        uint8_t h[32];
        if (b->obj[i] == NULL || b->t[i].rc != 0)
            continue;
        const struct sr_obj *o = sr_snap_find(&r->snap_c, b->t[i].tu);
        bool same = o && sr_hash_file(b->obj[i], h) && memcmp(h, o->hash, 32) == 0;
        r->cold_checked += o != NULL;
        r->cold_mismatch += o != NULL && !same;
        if (o && !same)
            fprintf(stderr, "sem-replay: cold compile differs from make's object: %s\n",
                    b->t[i].tu);
        if (fp)
            fprintf(fp, "%s\t%.4f\t%.4f\t%d\n", b->t[i].tu, b->t[i].cpu_s,
                    b->t[i].wall_s, same ? 1 : 0);
        (void)unlink(b->obj[i]);
    }
    if (fp)
        fclose(fp);
}

static bool after_batch(const struct sr_cfg *cfg, struct commit_run *r,
                        const struct sr_strv *timed)
{
    struct sr_strv all = {0};
    struct sr_argv_map m = {0};
    struct batch b = {0};
    char tmp[SR_PATH];
    snprintf(tmp, sizeof(tmp), "%s/tmp-obj", cfg->state);
    bool ok = sr_mkdirs(tmp) && set_union(&r->plain, timed, &all) &&
              sr_make_argv(cfg->repo, r->snap_c.epoch, &all, r->log, &m) &&
              add_sensing(cfg, &b, &r->plain, &m, "after", r->log) &&
              add_compiles(&b, timed, &m, tmp, r->log) && sense_batch(cfg, r, &b);
    if (ok)
        record_compiles(cfg, r, &b);
    batch_free(&b);
    sr_argv_free(&m);
    sr_strv_free(&all);
    return ok;
}

/* TUs that read a changed file only at C need a before manifest too. */
static bool extra_before(const struct sr_cfg *cfg, struct commit_run *r)
{
    struct sr_strv only_c = {0}, tus_p = {0}, extra = {0};
    bool ok = set_filter(&r->bound_c, &r->bound_p, true, &only_c) &&
              snap_tus(&r->snap_p, &tus_p) && set_filter(&only_c, &tus_p, false, &extra);
    if (ok && extra.n > 0) {
        fprintf(stderr, "sem-replay: %zu TUs read a changed file only after; sensing their before side\n",
                extra.n);
        ok = sr_git_checkout(cfg->repo, r->parent, r->log) &&
             sense_side(cfg, r, &extra, r->snap_p.epoch, "before") &&
             sr_git_checkout(cfg->repo, cfg->commit, r->log);
    }
    sr_strv_free(&only_c);
    sr_strv_free(&tus_p);
    sr_strv_free(&extra);
    return ok;
}

static bool after_side(const struct sr_cfg *cfg, struct commit_run *r,
                       const struct cost_table *costs)
{
    size_t missing = 0;
    struct sr_strv bounds = {0}, want = {0}, timed = {0};
    bool ok = sr_git_checkout(cfg->repo, cfg->commit, r->log);
    r->build_failed = ok && !sr_make_objects(cfg, r->log, NULL, &r->build_c);
    ok = ok && sr_snap_take(cfg->repo, &r->snap_p, &r->snap_c) &&
         snap_tus(&r->snap_c, &r->tus_c) &&
         sr_deps_hits(cfg->repo, &r->snap_c, &r->ch.files, &r->bound_c, &missing) &&
         compare_snaps(r) && set_union(&r->bound_p, &r->bound_c, &bounds) &&
         set_filter(&bounds, &r->tus_c, false, &r->plain) &&
         set_union(&r->make_set, &r->plain, &want);
    for (size_t i = 0; ok && i < want.n; i++)
        if (cost_find(costs, want.v[i]) == NULL)
            ok = sr_strv_push(&timed, want.v[i]);
    ok = ok && after_batch(cfg, r, &timed) && extra_before(cfg, r);
    sr_strv_free(&bounds);
    sr_strv_free(&want);
    sr_strv_free(&timed);
    return ok;
}

/* ── plans and the comparison ─────────────────────────────────────────── */

static bool plan_both(const struct sr_cfg *cfg, struct commit_run *r)
{
    char plain_raw[SR_PATH], facts_raw[SR_PATH];
    snprintf(plain_raw, sizeof(plain_raw), "%s/plan-plain", r->dir);
    snprintf(facts_raw, sizeof(facts_raw), "%s/plan-facts", r->dir);
    bool a = sr_plan_run(cfg->planner, cfg->repo, &r->ch.files, NULL, plain_raw,
                         &r->plain_plan);
    bool b = sr_plan_run(cfg->planner, cfg->repo, &r->ch.files, SR_FACTS_REL,
                         facts_raw, &r->facts_plan);
    if (!a || !b)
        fprintf(stderr, "sem-replay: planner failed (plain %d, facts %d): %s%s\n", a, b,
                r->plain_plan.error, r->facts_plan.error);
    return true; /* a planner failure is a recorded outcome, not a stop */
}

/* The facts plan's compile set: the affected TUs of a complete universe;
 * every TU when the plan widens to the whole catalog; else the plain set
 * the plan fell back to. */
static bool facts_compile_set(struct commit_run *r)
{
    const struct sr_plan *f = &r->facts_plan;
    if (!f->ok) {
        r->facts_mode = "planner-error";
        return set_union(&r->plain, &r->plain, &r->facts);
    }
    if (f->uni_applied && f->uni_complete) {
        r->facts_mode = "precise";
        return set_filter(&f->tus_affected, &r->tus_c, false, &r->facts);
    }
    if (f->obl_plain_universal || f->closure_universal) {
        r->facts_mode = "universal";
        return set_union(&r->tus_c, &r->tus_c, &r->facts);
    }
    r->facts_mode = "fallback";
    return set_union(&r->plain, &r->plain, &r->facts);
}

static bool compare_sets(const struct cost_table *costs, struct commit_run *r)
{
    struct sr_strv make_minus = {0}, plain_minus = {0};
    bool ok = facts_compile_set(r) &&
              set_filter(&r->changed, &r->facts, true, &r->fn) &&
              set_filter(&r->changed, &r->plain, true, &r->plain_fn) &&
              set_filter(&r->make_set, &r->facts, true, &make_minus) &&
              set_filter(&r->plain, &r->facts, true, &plain_minus);
    r->cpu_make = cost_sum(costs, &r->make_set, &r->cost_missing);
    r->cpu_facts = cost_sum(costs, &r->facts, NULL);
    r->cpu_changed = cost_sum(costs, &r->changed, NULL);
    r->saved_make = cost_sum(costs, &make_minus, NULL);
    r->saved_plain = cost_sum(costs, &plain_minus, NULL);
    sr_strv_free(&make_minus);
    sr_strv_free(&plain_minus);
    return ok;
}

static void write_set(FILE *fp, const char *name, const struct sr_strv *s)
{
    for (size_t i = 0; i < s->n; i++)
        fprintf(fp, "%s\t%s\n", name, s->v[i]);
}

static bool write_sets(const struct commit_run *r)
{
    char path[SR_PATH];
    snprintf(path, sizeof(path), "%s/sets.tsv", r->dir);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "sem-replay: write %s: %s\n", path, strerror(errno));
        return false;
    }
    write_set(fp, "file", &r->ch.files);
    write_set(fp, "make", &r->make_set);
    write_set(fp, "changed", &r->changed);
    write_set(fp, "removed", &r->removed);
    write_set(fp, "plain", &r->plain);
    write_set(fp, "facts", &r->facts);
    write_set(fp, "fn", &r->fn);
    write_set(fp, "plain_fn", &r->plain_fn);
    write_set(fp, "tu", &r->facts_plan.tu_rows);
    write_set(fp, "plain_group", &r->plain_plan.groups);
    write_set(fp, "facts_group", &r->facts_plan.groups);
    write_set(fp, "obligation_group", &r->facts_plan.obl_groups);
    return fclose(fp) == 0;
}

const char *const sr_result_header =
    "idx\tcommit\tkind\tfiles\tc\th\tother\tmake\tchanged\tremoved\tplain\tfacts\t"
    "facts_mode\tfn\tplain_fn\tgroups_plain\tgroups_facts\tobl_plain\tobl_facts\t"
    "narrowed\treason\tobl_reason\tuni_complete\tuni_reason\tuni_total\t"
    "uni_affected\tbuild_wall\tbuild_cpu\tsense_n\tsense_fail\tsense_cpu\t"
    "sense_wall\tcpu_make\tcpu_facts\tcpu_changed\tsaved_make\tsaved_plain\tcost_missing\t"
    "repro_rebuilt\trepro_mismatch\tcold_checked\tcold_mismatch\tbuild_failed\n";

static const char *or_dash(const char *s)
{
    return s && s[0] ? s : "-";
}

static bool write_result(const struct sr_cfg *cfg, const struct commit_run *r)
{
    char path[SR_PATH];
    const struct sr_plan *p = &r->plain_plan, *f = &r->facts_plan;
    snprintf(path, sizeof(path), "%s/result.tsv", r->dir);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "sem-replay: write %s: %s\n", path, strerror(errno));
        return false;
    }
    fputs(sr_result_header, fp);
    fprintf(fp, "%d\t%.10s\t%s\t%zu\t%zu\t%zu\t%zu\t", cfg->index, cfg->commit,
            r->ch.kind, r->ch.files.n, r->ch.n_c, r->ch.n_h, r->ch.n_other);
    fprintf(fp, "%zu\t%zu\t%zu\t%zu\t%zu\t%s\t%zu\t%zu\t", r->make_set.n, r->changed.n,
            r->removed.n, r->plain.n, r->facts.n, r->facts_mode, r->fn.n, r->plain_fn.n);
    fprintf(fp, "%ld\t%ld\t%ld\t%ld\t%d\t%s\t%s\t%d\t%s\t%ld\t%ld\t", p->groups_total,
            f->groups_total, f->obl_plain, f->obl_facts, f->narrowed, or_dash(f->reason),
            or_dash(f->obl_reason), f->uni_complete, or_dash(f->uni_reason), f->uni_total,
            f->uni_affected);
    fprintf(fp, "%.3f\t%.3f\t%zu\t%zu\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%zu\t",
            r->build_c.wall_s, r->build_c.cpu_s, r->sense_n, r->sense_fail, r->sense_cpu,
            r->sense_wall, r->cpu_make, r->cpu_facts, r->cpu_changed, r->saved_make, r->saved_plain,
            r->cost_missing);
    fprintf(fp, "%zu\t%zu\t%zu\t%zu\t%d\n", r->repro_rebuilt, r->repro_mismatch,
            r->cold_checked, r->cold_mismatch, r->build_failed);
    return fclose(fp) == 0;
}

static void report_false_negatives(const struct sr_cfg *cfg, const struct commit_run *r)
{
    char path[SR_PATH], keep[SR_PATH], facts[SR_PATH];
    path_in(path, sizeof(path), cfg, "FALSE_NEGATIVES.tsv");
    FILE *fp = fopen(path, "a");
    for (size_t i = 0; i < r->fn.n; i++) {
        fprintf(stderr, "FALSE NEGATIVE: commit %s TU %s (facts %s, reason %s, obligations %s)\n",
                cfg->commit, r->fn.v[i], r->facts_mode, or_dash(r->facts_plan.reason),
                or_dash(r->facts_plan.obl_reason));
        if (fp)
            fprintf(fp, "%s\t%s\t%s\t%s\t%s\n", cfg->commit, r->fn.v[i], r->facts_mode,
                    or_dash(r->facts_plan.reason), or_dash(r->facts_plan.obl_reason));
    }
    if (fp)
        fclose(fp);
    snprintf(facts, sizeof(facts), "%s/%s", cfg->repo, SR_FACTS_REL);
    snprintf(keep, sizeof(keep), "%s/facts", r->dir);
    if (rename(facts, keep) != 0)
        fprintf(stderr, "sem-replay: keep %s: %s\n", facts, strerror(errno));
}

static bool save_last(const struct sr_cfg *cfg, const struct commit_run *r)
{
    char snap[SR_PATH], commit[SR_PATH];
    path_in(snap, sizeof(snap), cfg, "last.snap");
    path_in(commit, sizeof(commit), cfg, "last.commit");
    return sr_snap_save(&r->snap_c, snap) &&
           sr_write_file(commit, cfg->commit, strlen(cfg->commit));
}

static bool step_dirs(const struct sr_cfg *cfg, struct commit_run *r)
{
    snprintf(r->dir, sizeof(r->dir), "%s/run/%03d_%.10s", cfg->state, cfg->index,
             cfg->commit);
    snprintf(r->log, sizeof(r->log), "%s/step.log", r->dir);
    return sr_mkdirs(r->dir);
}

int sr_step(const struct sr_cfg *cfg)
{
    struct commit_run r = {0};
    struct cost_table costs = {0};
    char cost_path[SR_PATH];
    path_in(cost_path, sizeof(cost_path), cfg, "compile_cost.tsv");
    bool ok = step_dirs(cfg, &r) && sr_git_parent(cfg->repo, cfg->commit, r.parent) &&
              sr_change_load(cfg->repo, r.parent, cfg->commit, &r.ch) &&
              cost_load(&costs, cost_path) && before_side(cfg, &r) &&
              after_side(cfg, &r, &costs);
    cost_free(&costs); /* after_side appended the TUs it timed */
    ok = ok && cost_load(&costs, cost_path) && plan_both(cfg, &r) &&
         compare_sets(&costs, &r) && write_sets(&r) && write_result(cfg, &r) &&
         save_last(cfg, &r);
    int rc = ok ? SR_STEP_OK : SR_STEP_FAILED;
    if (ok && r.fn.n > 0 && !r.build_failed) {
        report_false_negatives(cfg, &r);
        rc = SR_STEP_FALSE_NEGATIVE;
    }
    fprintf(stderr, "sem-replay: step %d %s %s: make %zu changed %zu plain %zu facts %zu (%s) fn %zu\n",
            cfg->index, cfg->commit, ok ? r.ch.kind : "FAILED", r.make_set.n, r.changed.n,
            r.plain.n, r.facts.n, r.facts_mode ? r.facts_mode : "-", r.fn.n);
    cost_free(&costs);
    commit_run_free(&r);
    return rc;
}

/* ── reproducibility: rebuild every object and compare its bytes ─────── */

int sr_repro(const struct sr_cfg *cfg, const char *label)
{
    struct sr_snap a = {0}, b = {0};
    struct sr_strv tus = {0};
    size_t rebuilt = 0, mismatch = 0;
    char log[SR_PATH], out[SR_PATH];
    snprintf(log, sizeof(log), "%s/repro-%s.log", cfg->state, label);
    snprintf(out, sizeof(out), "%s/repro-%s.tsv", cfg->state, label);
    bool ok = sr_snap_take(cfg->repo, NULL, &a) && snap_tus(&a, &tus) &&
              sr_make_objects(cfg, log, &tus, NULL) && sr_snap_take(cfg->repo, &a, &b);
    FILE *fp = ok ? fopen(out, "w") : NULL;
    if (ok)
        repro_compare(&a, &b, &rebuilt, &mismatch, fp);
    if (fp) {
        fprintf(fp, "objects\t%zu\nrebuilt\t%zu\nmismatch\t%zu\n", b.n, rebuilt, mismatch);
        fclose(fp);
    }
    fprintf(stderr, "sem-replay: repro %s: %zu objects, %zu rebuilt, %zu differ\n", label,
            b.n, rebuilt, mismatch);
    sr_snap_free(&a);
    sr_snap_free(&b);
    sr_strv_free(&tus);
    if (!ok)
        return SR_STEP_FAILED;
    return mismatch == 0 && rebuilt == b.n ? SR_STEP_OK : SR_STEP_FAILED;
}
