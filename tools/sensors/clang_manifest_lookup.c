/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Include lookups, stat-confirmed negative probes and __has_include replay shared by both semantic manifest producers. */
/* realpath() is declared only under _DEFAULT_SOURCE on glibc without the
 * fortify inline; set it before the first header pulls in <features.h>. */
#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#include "clang_manifest_core.h"

#include "base/safe_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define CM_PRESENT_MAX 64

/* Probe slots of one lookup, in the order the front end searches them:
 * quoted: the includer's directory, then the quote dirs, then the angled
 * dirs; angled: the angled dirs only. */
struct cm_probe {
    uint8_t form;
    char includer_dir[PATH_MAX];
    const char *name;
    uint32_t hit_slot;
    uint32_t present[CM_PRESENT_MAX];
    size_t npresent;
    char found_real[PATH_MAX];
    bool found;
};

static uint32_t cm_slot_count(const struct cm_core *c, uint8_t form)
{
    size_t n = c->angled.n;
    if (form == VCS_SEMANTIC_FORM_V1_QUOTED)
        n += 1 + c->quote.n;
    return (uint32_t)n;
}

static const char *cm_slot_dir(const struct cm_core *c,
                               const struct cm_probe *p, uint32_t slot)
{
    if (p->form == VCS_SEMANTIC_FORM_V1_QUOTED) {
        if (slot == 0)
            return p->includer_dir;
        slot -= 1;
        if (slot < c->quote.n)
            return c->quote.items[slot].raw;
        slot -= (uint32_t)c->quote.n;
    }
    return slot < c->angled.n ? c->angled.items[slot].raw : NULL;
}

static void cm_dir_of(const char *opened, char out[PATH_MAX])
{
    const char *slash = strrchr(opened, '/');
    if (slash == NULL) {
        (void)snprintf(out, PATH_MAX, ".");
        return;
    }
    if (slash == opened) {
        (void)snprintf(out, PATH_MAX, "/");
        return;
    }
    (void)snprintf(out, PATH_MAX, "%.*s", (int)(slash - opened), opened);
}

/* Walk the slots. With want_real set, stop at the slot whose regular file
 * is that exact file (the compiler's reported hit) and record any earlier
 * regular file that the compiler skipped; without it, stop at the first
 * regular file (a replay of the search). Misses are stat-confirmed. */
static void cm_probe_run(const struct cm_core *c, struct cm_probe *p,
                         const char *want_real)
{
    uint32_t count = cm_slot_count(c, p->form);
    p->hit_slot = count;
    p->npresent = 0;
    p->found = false;
    for (uint32_t s = 0; s < count; s++) {
        char cand[PATH_MAX * 2];
        struct stat sb;
        (void)snprintf(cand, sizeof(cand), "%s/%s", cm_slot_dir(c, p, s),
                       p->name);
        if (stat(cand, &sb) != 0 || !S_ISREG(sb.st_mode))
            continue;
        if (realpath(cand, p->found_real) == NULL)
            continue;
        if (want_real == NULL || strcmp(p->found_real, want_real) == 0) {
            p->hit_slot = s;
            p->found = true;
            return;
        }
        if (p->npresent < CM_PRESENT_MAX)
            p->present[p->npresent++] = s;
    }
}

static bool cm_emit_lookup(struct cm_core *c, const struct cm_file *includer,
                           const struct cm_probe *p, uint8_t kind,
                           const char *hit_path, uint8_t evidence)
{
    struct vcs_semantic_record_v1 rec = {0};
    bool ok;
    vcs_semantic_record_v1_cstr(&rec, includer->path);
    vcs_semantic_record_v1_cstr(&rec, p->name);
    vcs_semantic_record_v1_u8(&rec, p->form);
    vcs_semantic_record_v1_u8(&rec, kind);
    vcs_semantic_record_v1_cstr(&rec, hit_path);
    vcs_semantic_record_v1_u32(&rec, p->hit_slot);
    vcs_semantic_record_v1_u8(&rec, evidence);
    vcs_semantic_record_v1_u32(&rec, (uint32_t)p->npresent);
    for (size_t k = 0; k < p->npresent; k++)
        vcs_semantic_record_v1_u32(&rec, p->present[k]);
    ok = cm_add(c, VCS_SEMANTIC_SECTION_V1_LOOKUPS, &rec);
    vcs_semantic_record_v1_free(&rec);
    if (ok && c->facts)
        ok = cm_emit_probe(c, includer, p->name, p->form, kind,
                           evidence != VCS_SEMANTIC_MISS_V1_NONE, p->hit_slot,
                           p->present, p->npresent);
    return ok;
}

bool cm_lookup_directive(struct cm_core *c, const struct cm_file *includer,
                         const char *spelled, uint8_t form, uint8_t kind,
                         bool computed, const struct cm_file *hit)
{
    struct cm_probe p = {.form = form, .name = spelled};
    uint8_t evidence = VCS_SEMANTIC_MISS_V1_DERIVED_STAT;
    if (hit == NULL)
        return cm_fail(c, "include of %s resolved to no read file", includer->path);
    cm_dir_of(includer->opened, p.includer_dir);
    if (kind == VCS_SEMANTIC_LOOKUP_V1_INCLUDE_NEXT || computed ||
        spelled[0] == '/') {
        p.hit_slot = cm_slot_count(c, p.form);
        evidence = VCS_SEMANTIC_MISS_V1_NONE;
        kind = kind == VCS_SEMANTIC_LOOKUP_V1_INCLUDE_NEXT
                   ? kind : VCS_SEMANTIC_LOOKUP_V1_INCLUDE;
    } else {
        cm_probe_run(c, &p, hit->real);
        if (!p.found) {
            p.npresent = 0;
            evidence = VCS_SEMANTIC_MISS_V1_NONE;
        }
    }
    return cm_emit_lookup(c, includer, &p, kind, hit->path, evidence);
}

/* ---- __has_include: text scan + search replay -------------------------------- */

static bool cm_ident_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

static size_t cm_skip_space(const char *s, size_t i, size_t n)
{
    while (i < n && (s[i] == ' ' || s[i] == '\t'))
        i++;
    return i;
}

/* The quoted or angled operand starting at s[j]; copied to name. */
static bool cm_include_operand(const char *s, size_t j, size_t n,
                               uint8_t *form, char name[PATH_MAX])
{
    size_t end;
    char close;
    if (j >= n || (s[j] != '"' && s[j] != '<'))
        return false;
    close = s[j] == '"' ? '"' : '>';
    *form = s[j] == '"' ? VCS_SEMANTIC_FORM_V1_QUOTED : VCS_SEMANTIC_FORM_V1_ANGLED;
    for (end = j + 1; end < n && s[end] != close && s[end] != '\n'; end++)
        ;
    if (end >= n || s[end] != close || end - j - 1 == 0 || end - j - 1 >= PATH_MAX)
        return false;
    memcpy(name, s + j + 1, end - j - 1);
    name[end - j - 1] = '\0';
    return true;
}

/* Parse `__has_include ( "x" )` or `( <x> )` at s[i]; the operand is copied
 * to name. Returns false for anything else (a macro operand, _next). */
static bool cm_has_include_at(const char *s, size_t i, size_t n, uint8_t *form,
                              char name[PATH_MAX])
{
    static const char kw[] = "__has_include";
    size_t kl = sizeof(kw) - 1, j;
    if (n - i < kl || memcmp(s + i, kw, kl) != 0)
        return false;
    if ((i > 0 && cm_ident_char(s[i - 1])) || (i + kl < n && cm_ident_char(s[i + kl])))
        return false;
    j = cm_skip_space(s, i + kl, n);
    if (j >= n || s[j] != '(')
        return false;
    return cm_include_operand(s, cm_skip_space(s, j + 1, n), n, form, name);
}

static bool cm_has_include_one(struct cm_core *c, const struct cm_file *f,
                               uint8_t form, const char *name)
{
    struct cm_probe p = {.form = form, .name = name};
    char *hit_path = NULL;
    bool ok;
    cm_dir_of(f->opened, p.includer_dir);
    cm_probe_run(c, &p, NULL);
    if (p.found && !cm_norm_path(c, p.found_real, &hit_path))
        return false;
    ok = cm_emit_lookup(c, f, &p, VCS_SEMANTIC_LOOKUP_V1_HAS_INCLUDE,
                        p.found ? hit_path : "",
                        VCS_SEMANTIC_MISS_V1_REPLAYED_STAT);
    free(hit_path);
    return ok;
}

bool cm_scan_has_include(struct cm_core *c)
{
    for (size_t k = 0; k < c->nfiles; k++) {
        const struct cm_file *f = &c->files[k];
        const char *s = f->contents;
        if (f->origin == VCS_SEMANTIC_ORIGIN_V1_SYSTEM)
            continue;
        for (size_t i = 0; i + 13 <= f->size; i++) {
            char name[PATH_MAX];
            uint8_t form;
            if (s[i] != '_' || !cm_has_include_at(s, i, f->size, &form, name))
                continue;
            if (!cm_has_include_one(c, f, form, name))
                return false;
        }
    }
    return true;
}
