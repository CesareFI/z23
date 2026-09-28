/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * z23-sem-replay report: the per-commit table, totals, totals by change
 * kind, the fallback reasons ranked by what precision would save, and the
 * sensor's cost against the compile CPU of each plan's set. Reads
 * <state>/run/<NN>_<commit>/result.tsv, sets.tsv and tasks.tsv, and prices
 * each TU from catalog_cost.tsv, then compile_cost.tsv; writes Markdown to
 * stdout. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sem_replay.h"
#include "sem_replay_step.h"


/* ── pricing: every compile set in CPU seconds ────────────────────────── */

/* The sets a commit's sets.tsv names that the report prices. */
enum { P_MAKE, P_CHANGED, P_PLAIN, P_FACTS, P_CFACTS, P_N };
static const char *const price_set[P_N] = {"make", "changed", "plain", "facts", "c_facts"};

struct price_row {
    char *tu;
    double cpu;
    size_t n;
};

struct prices {
    struct price_row *v;
    size_t n, cap;
    double mean; /* over the catalog, else over the timed compiles */
};

static int cmp_price(const void *a, const void *b)
{
    return strcmp(((const struct price_row *)a)->tu, ((const struct price_row *)b)->tu);
}

static bool price_push(struct prices *p, const char *tu, double cpu)
{
    if (p->n == p->cap) {
        size_t cap = p->cap ? p->cap * 2 : 4096;
        struct price_row *v = realloc(p->v, cap * sizeof(*v));
        if (v == NULL)
            return false;
        p->v = v;
        p->cap = cap;
    }
    p->v[p->n] = (struct price_row){.tu = strdup(tu), .cpu = cpu, .n = 1};
    return p->v[p->n++].tu != NULL;
}

/* Rows "TU<TAB>cpu..." averaged per TU, sorted for lookup. */
static bool prices_load(struct prices *p, const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[SR_PATH + 128], tu[SR_PATH];
    double cpu = 0, sum = 0;
    bool ok = true;
    while (fp != NULL && ok && fgets(line, sizeof(line), fp) != NULL)
        if (sscanf(line, "%4095s\t%lf", tu, &cpu) == 2)
            ok = price_push(p, tu, cpu);
    if (fp)
        fclose(fp);
    if (p->n > 1)
        qsort(p->v, p->n, sizeof(*p->v), cmp_price);
    size_t w = 0;
    for (size_t i = 0; i < p->n; i++) {
        sum += p->v[i].cpu;
        if (w > 0 && strcmp(p->v[w - 1].tu, p->v[i].tu) == 0) {
            p->v[w - 1].cpu += p->v[i].cpu;
            p->v[w - 1].n++;
            free(p->v[i].tu);
            continue;
        }
        p->v[w++] = p->v[i];
    }
    p->mean = p->n ? sum / (double)p->n : 0;
    p->n = w;
    for (size_t i = 0; i < p->n; i++)
        p->v[i].cpu /= (double)p->v[i].n;
    return ok;
}

static const struct price_row *price_find(const struct prices *p, const char *tu)
{
    struct price_row key = {.tu = (char *)tu};
    return p->n ? bsearch(&key, p->v, p->n, sizeof(*p->v), cmp_price) : NULL;
}

static void prices_free(struct prices *p)
{
    for (size_t i = 0; i < p->n; i++)
        free(p->v[i].tu);
    free(p->v);
    memset(p, 0, sizeof(*p));
}

/* The catalog's cold compile of the TU, else its timed compiles in the
 * replay, else the mean. */
struct pricing {
    struct prices catalog, timed;
    double mean;
    size_t guessed; /* lookups that fell back to the mean */
};

static double price_of(struct pricing *pr, const char *tu)
{
    const struct price_row *r = price_find(&pr->catalog, tu);
    if (r == NULL)
        r = price_find(&pr->timed, tu);
    pr->guessed += r == NULL;
    return r ? r->cpu : pr->mean;
}

static bool pricing_load(struct pricing *pr, const char *state)
{
    char path[SR_PATH];
    snprintf(path, sizeof(path), "%s/catalog_cost.tsv", state);
    bool ok = prices_load(&pr->catalog, path);
    snprintf(path, sizeof(path), "%s/compile_cost.tsv", state);
    ok = ok && prices_load(&pr->timed, path);
    pr->mean = pr->catalog.n ? pr->catalog.mean : pr->timed.mean;
    return ok;
}

static void price_sets(struct pricing *pr, const char *file, double out[P_N])
{
    FILE *fp = fopen(file, "r");
    char line[SR_PATH + 64], set[32], tu[SR_PATH];
    memset(out, 0, P_N * sizeof(double));
    while (fp && fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%31s\t%4095s", set, tu) != 2)
            continue;
        for (int k = 0; k < P_N; k++)
            if (strcmp(set, price_set[k]) == 0)
                out[k] += price_of(pr, tu);
    }
    if (fp)
        fclose(fp);
}

#define SR_COLS 64

struct row {
    char *line;
    char *f[SR_COLS];
    size_t n;
    double p[P_N]; /* priced compile sets, CPU s */
};

struct table {
    char *head_line;
    char *h[SR_COLS];
    size_t nh;
    struct row *rows;
    size_t n, cap;
};

static size_t split_tabs(char *line, char **f)
{
    size_t n = 0;
    line[strcspn(line, "\n")] = '\0';
    for (char *p = line; p && n < SR_COLS; n++) {
        f[n] = p;
        p = strchr(p, '\t');
        if (p)
            *p++ = '\0';
    }
    return n;
}

static const char *col(const struct table *t, const struct row *r, const char *name)
{
    for (size_t i = 0; i < t->nh && i < r->n; i++)
        if (strcmp(t->h[i], name) == 0)
            return r->f[i];
    return "";
}

static double num(const struct table *t, const struct row *r, const char *name)
{
    return strtod(col(t, r, name), NULL);
}

static bool table_add(struct table *t, char *text)
{
    char *nl = strchr(text, '\n');
    if (nl == NULL)
        return false;
    *nl = '\0';
    if (t->head_line == NULL) {
        t->head_line = strdup(text);
        if (t->head_line == NULL)
            return false;
        t->nh = split_tabs(t->head_line, t->h);
    }
    if (t->n == t->cap) {
        size_t cap = t->cap ? t->cap * 2 : 64;
        struct row *v = realloc(t->rows, cap * sizeof(*v));
        if (v == NULL)
            return false;
        t->rows = v;
        t->cap = cap;
    }
    struct row *r = &t->rows[t->n];
    r->line = strdup(nl + 1);
    if (r->line == NULL)
        return false;
    r->n = split_tabs(r->line, r->f);
    t->n++;
    return true;
}

static int cmp_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static bool load_runs(const char *state, struct pricing *pr, struct table *t,
                      struct sr_strv *dirs)
{
    char path[SR_PATH];
    snprintf(path, sizeof(path), "%s/run", state);
    DIR *d = opendir(path);
    if (d == NULL) {
        fprintf(stderr, "sem-replay: no runs under %s\n", path);
        return false;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
        if (e->d_name[0] != '.')
            (void)sr_strv_push(dirs, e->d_name);
    closedir(d);
    if (dirs->n > 1)
        qsort(dirs->v, dirs->n, sizeof(char *), cmp_name);
    for (size_t i = 0; i < dirs->n; i++) {
        char file[SR_PATH], *text = NULL;
        size_t len = 0;
        snprintf(file, sizeof(file), "%s/%s/result.tsv", path, dirs->v[i]);
        size_t before = t->n;
        if (sr_read_file(file, &text, &len) && !table_add(t, text))
            fprintf(stderr, "sem-replay: malformed %s\n", file);
        free(text);
        if (t->n == before)
            continue;
        snprintf(file, sizeof(file), "%s/%s/sets.tsv", path, dirs->v[i]);
        price_sets(pr, file, t->rows[t->n - 1].p);
    }
    return true;
}

static bool is_one(const struct table *t, const struct row *r, const char *name)
{
    return strcmp(col(t, r, name), "1") == 0;
}

static void print_commits(const struct table *t)
{
    printf("## Per commit\n\n");
    printf("Compile counts are TUs; CPU is priced per TU (see Pricing). FN is changed objects "
           "the facts set left out through their source; +argv counts those whose compile argv "
           "also changed (the identity-stamped TU).\n\n");
    printf("| # | commit | kind | files c/h/other | make | changed | plain | facts (mode) | FN | "
           "groups plain / facts / obligations | verdict | extraction CPU s | "
           "compile CPU make / plain / facts / changed s |\n");
    printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < t->n; i++) {
        const struct row *r = &t->rows[i];
        printf("| %s | %s | %s | %s/%s/%s | %s | %s | %s | %s (%s) | %s +%s argv | %s / %s / %s | "
               "%s | %.1f | %.1f / %.1f / %.1f / %.1f |\n",
               col(t, r, "idx"), col(t, r, "commit"), col(t, r, "kind"), col(t, r, "c"),
               col(t, r, "h"), col(t, r, "other"), col(t, r, "make"), col(t, r, "changed"),
               col(t, r, "plain"), col(t, r, "facts"), col(t, r, "facts_mode"),
               col(t, r, "fn_source"), col(t, r, "fn_flags"), col(t, r, "groups_plain"),
               col(t, r, "groups_facts"), col(t, r, "obl_facts"),
               is_one(t, r, "narrowed") ? "narrowed" : col(t, r, "reason"),
               num(t, r, "sense_cpu"), r->p[P_MAKE], r->p[P_PLAIN], r->p[P_FACTS],
               r->p[P_CHANGED]);
    }
    printf("\n");
}

struct agg {
    char name[128];
    size_t commits, narrowed, precise;
    double make, changed, plain, facts, fn_source, fn_flags, gplain, gfacts, obl, sense;
    double p[P_N];
};

static void agg_add(struct agg *a, const struct table *t, const struct row *r)
{
    a->commits++;
    a->narrowed += is_one(t, r, "narrowed");
    a->precise += strcmp(col(t, r, "facts_mode"), "precise") == 0;
    a->make += num(t, r, "make");
    a->changed += num(t, r, "changed");
    a->plain += num(t, r, "plain");
    a->facts += num(t, r, "facts");
    a->fn_source += num(t, r, "fn_source");
    a->fn_flags += num(t, r, "fn_flags");
    a->gplain += num(t, r, "groups_plain");
    a->gfacts += num(t, r, "groups_facts");
    a->obl += num(t, r, "obl_facts");
    a->sense += num(t, r, "sense_cpu");
    for (int k = 0; k < P_N; k++)
        a->p[k] += r->p[k];
}

static struct agg *agg_find(struct agg *v, size_t *n, size_t cap, const char *name)
{
    for (size_t i = 0; i < *n; i++)
        if (strcmp(v[i].name, name) == 0)
            return &v[i];
    if (*n == cap)
        return NULL;
    memset(&v[*n], 0, sizeof(v[*n]));
    snprintf(v[*n].name, sizeof(v[*n].name), "%s", name);
    return &v[(*n)++];
}

static void print_agg_row(const struct agg *a)
{
    printf("| %s | %zu | %zu / %zu | %.0f | %.0f | %.0f | %.0f | %.0f +%.0f argv | "
           "%.0f / %.0f / %.0f | %.1f | %.1f / %.1f / %.1f / %.1f |\n",
           a->name, a->commits, a->narrowed, a->precise, a->make, a->plain, a->facts, a->changed,
           a->fn_source, a->fn_flags, a->gplain, a->gfacts, a->obl, a->sense, a->p[P_MAKE],
           a->p[P_PLAIN], a->p[P_FACTS], a->p[P_CHANGED]);
}

static void print_groups(const struct table *t, const char *key, const char *title)
{
    struct agg v[64];
    size_t n = 0;
    struct agg total = {0};
    snprintf(total.name, sizeof(total.name), "**total**");
    for (size_t i = 0; i < t->n; i++) {
        struct agg *a = agg_find(v, &n, 64, col(t, &t->rows[i], key));
        if (a)
            agg_add(a, t, &t->rows[i]);
        agg_add(&total, t, &t->rows[i]);
    }
    printf("## %s\n\n", title);
    printf("| %s | commits | narrowed / precise | make | plain | facts | changed | FN | "
           "groups plain / facts / obligations | extraction CPU s | "
           "compile CPU make / plain / facts / changed s |\n", key);
    printf("|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < n; i++)
        print_agg_row(&v[i]);
    print_agg_row(&total);
    printf("\n");
}

static int cmp_waste(const void *a, const void *b)
{
    const struct agg *x = a, *y = b;
    double wx = x->p[P_FACTS] - x->p[P_CHANGED], wy = y->p[P_FACTS] - y->p[P_CHANGED];
    return (wx < wy) - (wx > wy);
}

/* Fallback reasons, ranked by what a precise plan could still remove: the
 * facts set's CPU minus the changed objects' CPU, and the facts plan's test
 * groups minus the obligations the facts verdict named. */
static void print_fallbacks(const struct table *t, const char *reason_col, const char *title)
{
    struct agg v[64];
    size_t n = 0;
    for (size_t i = 0; i < t->n; i++) {
        const struct row *r = &t->rows[i];
        if (strcmp(col(t, r, "facts_mode"), "precise") == 0)
            continue;
        struct agg *a = agg_find(v, &n, 64, col(t, r, reason_col));
        if (a)
            agg_add(a, t, r);
    }
    qsort(v, n, sizeof(v[0]), cmp_waste);
    printf("## %s\n\n", title);
    printf("| rank | reason | commits | facts TUs | changed TUs | TUs precision could drop | "
           "CPU s precision could drop | groups facts / obligations |\n");
    printf("|---|---|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < n; i++)
        printf("| %zu | %s | %zu | %.0f | %.0f | %.0f | %.1f | %.0f / %.0f |\n", i + 1, v[i].name,
               v[i].commits, v[i].facts, v[i].changed, v[i].facts - v[i].changed,
               v[i].p[P_FACTS] - v[i].p[P_CHANGED], v[i].gfacts, v[i].obl);
    printf("\n");
}

/* The same commits planned with only their C text. */
static void print_c_variant(const struct table *t)
{
    double facts = 0, fns = 0, fna = 0, gp = 0, gf = 0, obl = 0, cpu = 0;
    size_t precise = 0;
    printf("## C-only plans (the commit's .c, .h, .def and .inc files only)\n\n");
    printf("| # | commit | separate plan | C files | facts (mode) | changed | FN | "
           "groups plain / facts / obligations | verdict | compile CPU facts s |\n");
    printf("|---|---|---|---|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < t->n; i++) {
        const struct row *r = &t->rows[i];
        double fn_argv = num(t, r, "c_fn") - num(t, r, "c_fn_source");
        printf("| %s | %s | %s | %s | %s (%s) | %s | %s +%.0f argv | %s / %s / %s | %s | %.1f |\n",
               col(t, r, "idx"), col(t, r, "commit"), col(t, r, "c_run"), col(t, r, "c_files"),
               col(t, r, "c_facts"), col(t, r, "c_mode"), col(t, r, "changed"),
               col(t, r, "c_fn_source"), fn_argv, col(t, r, "c_groups_plain"),
               col(t, r, "c_groups_facts"), col(t, r, "c_obl_facts"),
               is_one(t, r, "c_narrowed") ? "narrowed" : col(t, r, "c_reason"), r->p[P_CFACTS]);
        precise += strcmp(col(t, r, "c_mode"), "precise") == 0;
        facts += num(t, r, "c_facts");
        fns += num(t, r, "c_fn_source");
        fna += fn_argv;
        gp += num(t, r, "c_groups_plain");
        gf += num(t, r, "c_groups_facts");
        obl += num(t, r, "c_obl_facts");
        cpu += r->p[P_CFACTS];
    }
    printf("| **total** | | %zu precise | | %.0f | | %.0f +%.0f argv | %.0f / %.0f / %.0f | | %.1f |\n\n",
           precise, facts, fns, fna, gp, gf, obl, cpu);
}

static void print_pricing(const struct pricing *pr)
{
    printf("## Pricing\n\n");
    printf("A TU's compile CPU is its cold compile in the catalog (%zu TUs, mean %.3f s), "
           "else the mean of its timed compiles during the replay (%zu TUs, mean %.3f s), "
           "else the mean (%.3f s; %zu lookups).\n\n",
           pr->catalog.n, pr->catalog.mean, pr->timed.n, pr->timed.mean, pr->mean, pr->guessed);
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

struct samples {
    double *v;
    size_t n, cap;
};

static void sample_push(struct samples *s, double x)
{
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 1024;
        double *v = realloc(s->v, cap * sizeof(*v));
        if (v == NULL)
            return;
        s->v = v;
        s->cap = cap;
    }
    s->v[s->n++] = x;
}

static void sample_line(const char *name, struct samples *s)
{
    if (s->n == 0) {
        printf("| %s | 0 | - | - | - | - |\n", name);
        return;
    }
    qsort(s->v, s->n, sizeof(double), cmp_double);
    double sum = 0;
    for (size_t i = 0; i < s->n; i++)
        sum += s->v[i];
    printf("| %s | %zu | %.3f | %.3f | %.3f | %.1f |\n", name, s->n, sum / (double)s->n,
           s->v[s->n / 2], s->v[(s->n * 9) / 10], sum);
}

static void read_tasks(const char *path, struct samples *cpu, struct samples *wall,
                       const char *side)
{
    FILE *fp = fopen(path, "r");
    char line[SR_PATH + 128], s[32], tu[SR_PATH];
    double w = 0, c = 0;
    int rc = 0;
    while (fp && fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%31s\t%4095s\t%lf\t%lf\t%d", s, tu, &w, &c, &rc) != 5)
            continue;
        if ((strcmp(side, "compile") == 0) != (strcmp(s, "compile") == 0) || rc != 0)
            continue;
        sample_push(cpu, c);
        sample_push(wall, w);
    }
    if (fp)
        fclose(fp);
}

static void print_costs(const char *state, const struct sr_strv *dirs)
{
    struct samples scpu = {0}, swall = {0}, ccpu = {0}, cwall = {0};
    for (size_t i = 0; i < dirs->n; i++) {
        char path[SR_PATH];
        snprintf(path, sizeof(path), "%s/run/%s/tasks.tsv", state, dirs->v[i]);
        read_tasks(path, &scpu, &swall, "sense");
        read_tasks(path, &ccpu, &cwall, "compile");
    }
    printf("## Per-TU cost (seconds)\n\n");
    printf("| measure | samples | mean | p50 | p90 | sum |\n|---|---|---|---|---|---|\n");
    sample_line("sensor CPU per TU", &scpu);
    sample_line("sensor wall per TU", &swall);
    sample_line("compile CPU per TU (timed, same argv)", &ccpu);
    sample_line("compile wall per TU (timed, same argv)", &cwall);
    printf("\n");
    free(scpu.v);
    free(swall.v);
    free(ccpu.v);
    free(cwall.v);
}

int sr_report(const struct sr_cfg *cfg)
{
    struct table t = {0};
    struct sr_strv dirs = {0};
    struct pricing pr = {0};
    if (!pricing_load(&pr, cfg->state) || !load_runs(cfg->state, &pr, &t, &dirs))
        return 1;
    printf("# Semantic facts replay\n\n%zu commits replayed.\n\n", t.n);
    print_commits(&t);
    print_groups(&t, "kind", "By change kind");
    print_groups(&t, "facts_mode", "By facts compile-set mode");
    print_fallbacks(&t, "reason", "Fallback reasons (facts plan not precise), by verdict reason");
    print_fallbacks(&t, "uni_reason", "Fallback reasons, by compile-universe reason");
    print_c_variant(&t);
    print_costs(cfg->state, &dirs);
    print_pricing(&pr);
    prices_free(&pr.catalog);
    prices_free(&pr.timed);
    for (size_t i = 0; i < t.n; i++)
        free(t.rows[i].line);
    free(t.rows);
    free(t.head_line);
    sr_strv_free(&dirs);
    return 0;
}
