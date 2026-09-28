/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * z23-sem-replay report: the per-commit table, totals, totals by change
 * kind, the fallback reasons ranked by what precision would save, and the
 * sensor's cost against the compile time the facts plan saved. Reads
 * <state>/run/<NN>_<commit>/result.tsv, tasks.tsv and compile_cost.tsv;
 * writes Markdown to stdout. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sem_replay.h"
#include "sem_replay_step.h"

#define SR_COLS 64

struct row {
    char *line;
    char *f[SR_COLS];
    size_t n;
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

static bool load_runs(const char *state, struct table *t, struct sr_strv *dirs)
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
        if (sr_read_file(file, &text, &len) && !table_add(t, text))
            fprintf(stderr, "sem-replay: malformed %s\n", file);
        free(text);
    }
    return true;
}

static void print_commits(const struct table *t)
{
    printf("## Per commit\n\n");
    printf("| # | commit | kind | files c/h/other | make | changed | plain | facts (mode) | FN | "
           "groups plain / facts / obligations | verdict | extraction CPU s | compile CPU saved vs make s |\n");
    printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < t->n; i++) {
        const struct row *r = &t->rows[i];
        bool narrowed = strcmp(col(t, r, "narrowed"), "1") == 0;
        printf("| %s | %s | %s | %s/%s/%s | %s | %s | %s | %s (%s) | %s | %s / %s / %s | %s | %.1f | %.1f |\n",
               col(t, r, "idx"), col(t, r, "commit"), col(t, r, "kind"), col(t, r, "c"),
               col(t, r, "h"), col(t, r, "other"), col(t, r, "make"), col(t, r, "changed"),
               col(t, r, "plain"), col(t, r, "facts"), col(t, r, "facts_mode"), col(t, r, "fn"),
               col(t, r, "groups_plain"), col(t, r, "groups_facts"), col(t, r, "obl_facts"),
               narrowed ? "narrowed" : col(t, r, "reason"), num(t, r, "sense_cpu"),
               num(t, r, "saved_make"));
    }
    printf("\n");
}

struct agg {
    char name[128];
    size_t commits, narrowed;
    double make, changed, plain, facts, fn, gplain, gfacts, obl, sense, saved_make,
        saved_plain, cpu_facts, cpu_changed, build_cpu;
};

static void agg_add(struct agg *a, const struct table *t, const struct row *r)
{
    a->commits++;
    a->narrowed += strcmp(col(t, r, "narrowed"), "1") == 0;
    a->make += num(t, r, "make");
    a->changed += num(t, r, "changed");
    a->plain += num(t, r, "plain");
    a->facts += num(t, r, "facts");
    a->fn += num(t, r, "fn");
    a->gplain += num(t, r, "groups_plain");
    a->gfacts += num(t, r, "groups_facts");
    a->obl += num(t, r, "obl_facts");
    a->sense += num(t, r, "sense_cpu");
    a->saved_make += num(t, r, "saved_make");
    a->saved_plain += num(t, r, "saved_plain");
    a->cpu_facts += num(t, r, "cpu_facts");
    a->cpu_changed += num(t, r, "cpu_changed");
    a->build_cpu += num(t, r, "build_cpu");
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
    printf("| %s | %zu | %zu | %.0f | %.0f | %.0f | %.0f | %.0f | %.0f / %.0f / %.0f | %.1f | %.1f | %.1f |\n",
           a->name, a->commits, a->narrowed, a->make, a->plain, a->facts, a->changed, a->fn,
           a->gplain, a->gfacts, a->obl, a->sense, a->saved_make, a->saved_plain);
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
    printf("| %s | commits | narrowed | make | plain | facts | changed | FN | groups plain / facts / obligations | "
           "extraction CPU s | saved vs make s | saved vs plain s |\n", key);
    printf("|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < n; i++)
        print_agg_row(&v[i]);
    print_agg_row(&total);
    printf("\n");
}

/* Fallback reasons: what a precise plan could still remove, in TUs and
 * CPU seconds (facts set minus changed objects) and in test groups
 * (the facts groups minus the obligations a narrowed plan would give). */
static void print_fallbacks(const struct table *t)
{
    struct agg v[64];
    size_t n = 0;
    for (size_t i = 0; i < t->n; i++) {
        const struct row *r = &t->rows[i];
        if (strcmp(col(t, r, "narrowed"), "1") == 0)
            continue;
        struct agg *a = agg_find(v, &n, 64, col(t, r, "reason"));
        if (a)
            agg_add(a, t, r);
    }
    printf("## Fallback reasons (facts verdict not narrowed)\n\n");
    printf("| reason | commits | facts TUs | changed TUs | TUs precision could drop | CPU s precision could drop | groups (facts run) |\n");
    printf("|---|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < n; i++)
        printf("| %s | %zu | %.0f | %.0f | %.0f | %.1f | %.0f |\n", v[i].name, v[i].commits,
               v[i].facts, v[i].changed, v[i].facts - v[i].changed,
               v[i].cpu_facts - v[i].cpu_changed, v[i].gfacts);
    printf("\n");
}

/* The same commits planned with only their C text. */
static void print_c_variant(const struct table *t)
{
    double facts = 0, fn = 0, gp = 0, gf = 0, obl = 0, saved = 0;
    printf("## C-only plans (the commit's .c, .h, .def and .inc files only)\n\n");
    printf("| # | commit | separate plan | C files | facts (mode) | changed | FN | groups plain / facts / obligations | verdict |\n");
    printf("|---|---|---|---|---|---|---|---|---|\n");
    for (size_t i = 0; i < t->n; i++) {
        const struct row *r = &t->rows[i];
        bool narrowed = strcmp(col(t, r, "c_narrowed"), "1") == 0;
        printf("| %s | %s | %s | %s | %s (%s) | %s | %s | %s / %s / %s | %s |\n",
               col(t, r, "idx"), col(t, r, "commit"), col(t, r, "c_run"), col(t, r, "c_files"),
               col(t, r, "c_facts"), col(t, r, "c_mode"), col(t, r, "changed"),
               col(t, r, "c_fn"), col(t, r, "c_groups_plain"), col(t, r, "c_groups_facts"),
               col(t, r, "c_obl_facts"), narrowed ? "narrowed" : col(t, r, "c_reason"));
        facts += num(t, r, "c_facts");
        fn += num(t, r, "c_fn");
        gp += num(t, r, "c_groups_plain");
        gf += num(t, r, "c_groups_facts");
        obl += num(t, r, "c_obl_facts");
        saved += num(t, r, "c_saved_make");
    }
    printf("| **total** | | | | %.0f | | %.0f | %.0f / %.0f / %.0f | compile CPU saved vs make %.1f s |\n\n",
           facts, fn, gp, gf, obl, saved);
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
    if (!load_runs(cfg->state, &t, &dirs))
        return 1;
    printf("# Semantic facts replay\n\n%zu commits replayed.\n\n", t.n);
    print_commits(&t);
    print_groups(&t, "kind", "By change kind");
    print_groups(&t, "facts_mode", "By facts compile-set mode");
    print_fallbacks(&t);
    print_c_variant(&t);
    print_costs(cfg->state, &dirs);
    for (size_t i = 0; i < t.n; i++)
        free(t.rows[i].line);
    free(t.rows);
    free(t.head_line);
    sr_strv_free(&dirs);
    return 0;
}
