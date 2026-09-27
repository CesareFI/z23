/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Compiler-API-free binding of a warm semantic-sensor TU to the tree: file digests, absent include slots and main-file identity. */
#include "clang_manifest_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* A warm session reparses a TU whose preamble (the headers) clang reuses
 * after checking each header's size and modification time only. These
 * checks are what the session trusts instead, all against the last manifest
 * it accepted for the TU: the exact bytes of every file it read, and the
 * absence of every include slot it saw missing. They are the checks
 * dev.change.plan applies to an after manifest (tools/dev/
 * devloop_facts_bind.c), over the same reader enumeration. */

struct cm_bind {
    const char *root;
    char *why;
    size_t why_len;
    bool failed;
};

static bool cm_bind_fail(struct cm_bind *b, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

static bool cm_bind_fail(struct cm_bind *b, const char *fmt, ...)
{
    va_list ap;
    if (b->failed)
        return false;
    b->failed = true;
    va_start(ap, fmt);
    (void)vsnprintf(b->why, b->why_len, fmt, ap);
    va_end(ap);
    return false;
}

/* The host path of a manifest path (the inverse of cm_spell_real for what
 * FILES and IDENTITY carry): "." is root, "@sys/x" is "/x", anything else
 * is root-relative. Texts are (pointer, length) as the enumerators give
 * them. False when out is too small. */
static bool cm_host_path(const char *root, const char *p, size_t n, char *out,
                         size_t cap)
{
    int w;
    if (n >= 4 && memcmp(p, "@sys", 4) == 0)
        w = snprintf(out, cap, "/%.*s", (int)(n > 5 ? n - 5 : 0), p + 5);
    else if (n == 1 && p[0] == '.')
        w = snprintf(out, cap, "%s", root);
    else
        w = snprintf(out, cap, "%s/%.*s", root, (int)n, p);
    return w >= 0 && (size_t)w < cap;
}

static bool cm_bind_file_cb(void *ctx, const struct vcs_semantic_fields_v1 *f)
{
    struct cm_bind *b = ctx;
    char path[PATH_MAX * 2];
    uint8_t now[32];
    if (f->ntext < 1 || f->nnum < 1 || f->ndigest < 1)
        return cm_bind_fail(b, "a FILES record is malformed");
    if (f->num[0] == VCS_SEMANTIC_ORIGIN_V1_MAIN)
        return true;
    if (!cm_host_path(b->root, f->text[0], f->text_len[0], path,
                      sizeof(path)) ||
        !cm_stream_sha3(fopen(path, "rb"), now) ||
        memcmp(now, f->digest[0], 32) != 0)
        return cm_bind_fail(b, "file-changed %.*s", (int)f->text_len[0],
                            f->text[0]);
    return true;
}

/* ---- two manifests' FILES ---------------------------------------------------- */

/* One FILES record, as a view into its manifest's bytes. */
struct cm_fview {
    const char *path;
    size_t len;
    const uint8_t *digest;
    uint64_t origin;
};

struct cm_flist {
    struct cm_fview *v;
    size_t n, cap;
};

static bool cm_flist_cb(void *ctx, const struct vcs_semantic_fields_v1 *f)
{
    struct cm_flist *l = ctx;
    if (f->ntext < 1 || f->nnum < 1 || f->ndigest < 1 ||
        !cm_grow((void **)&l->v, &l->cap, l->n, sizeof(*l->v)))
        return false;
    l->v[l->n++] = (struct cm_fview){.path = f->text[0], .len = f->text_len[0],
                                     .digest = f->digest[0],
                                     .origin = f->num[0]};
    return true;
}

static bool cm_flist(const uint8_t *m, size_t n, struct cm_flist *l)
{
    return vcs_semantic_section_v1_each(m, n, VCS_SEMANTIC_SECTION_V1_FILES,
                                        cm_flist_cb, l);
}

/* The record of `want`'s path in l, or NULL. */
static const struct cm_fview *cm_flist_find(const struct cm_flist *l,
                                            const struct cm_fview *want)
{
    for (size_t k = 0; k < l->n; k++) {
        if (l->v[k].len == want->len &&
            memcmp(l->v[k].path, want->path, want->len) == 0)
            return &l->v[k];
    }
    return NULL;
}

static bool cm_flists_agree(const struct cm_flist *a, const struct cm_flist *b,
                            struct cm_bind *bind)
{
    for (size_t k = 0; k < b->n; k++) {
        const struct cm_fview *hit;
        if (b->v[k].origin == VCS_SEMANTIC_ORIGIN_V1_MAIN)
            continue;
        hit = cm_flist_find(a, &b->v[k]);
        if (hit != NULL && memcmp(hit->digest, b->v[k].digest, 32) != 0)
            return cm_bind_fail(bind, "preamble-file-differs %.*s",
                                (int)b->v[k].len, b->v[k].path);
    }
    return true;
}

bool cm_warm_files_agree(const uint8_t *prev, size_t prev_len,
                         const uint8_t *next, size_t next_len, char *why,
                         size_t why_len)
{
    struct cm_bind bind = {.why = why, .why_len = why_len};
    struct cm_flist a = {0}, b = {0};
    bool ok = cm_flist(prev, prev_len, &a) && cm_flist(next, next_len, &b);
    if (!ok)
        (void)cm_bind_fail(&bind, "a manifest's FILES are unreadable");
    ok = ok && cm_flists_agree(&a, &b, &bind);
    free(a.v);
    free(b.v);
    return ok;
}

struct cm_main_digest {
    uint8_t *out;
    bool found;
};

static bool cm_main_cb(void *ctx, const struct vcs_semantic_fields_v1 *f)
{
    struct cm_main_digest *d = ctx;
    if (f->nnum < 1 || f->ndigest < 1 ||
        f->num[0] != VCS_SEMANTIC_ORIGIN_V1_MAIN)
        return true;
    memcpy(d->out, f->digest[0], 32);
    d->found = true;
    return true;
}

bool cm_warm_main_digest(const uint8_t *m, size_t n, uint8_t out[32])
{
    struct cm_main_digest d = {.out = out};
    return vcs_semantic_section_v1_each(m, n, VCS_SEMANTIC_SECTION_V1_FILES,
                                        cm_main_cb, &d) &&
           d.found;
}

/* ---- lookups: the warm manifest's fresh probes against the accepted ones ---- */

/* One LOOKUPS record body and the length of its key: includer, spelled,
 * form and kind, the leading fields of the record. */
struct cm_lrec {
    const uint8_t *raw;
    size_t len, key_len;
    uint8_t evidence; /* VCS_SEMANTIC_MISS_V1_*: NONE makes no negative claim */
};

struct cm_lset {
    struct cm_lrec *v;
    size_t n, cap;
    bool bad;
};

static bool cm_text_end(const uint8_t *raw, size_t len, size_t *at)
{
    uint32_t n;
    if (len - *at < 4)
        return false;
    n = (uint32_t)raw[*at] | (uint32_t)raw[*at + 1] << 8 |
        (uint32_t)raw[*at + 2] << 16 | (uint32_t)raw[*at + 3] << 24;
    if (len - *at - 4 < n)
        return false;
    *at += 4 + (size_t)n;
    return true;
}

static bool cm_lset_cb(void *ctx, enum vcs_semantic_section_v1 section,
                       const struct vcs_semantic_fields_v1 *fields,
                       const uint8_t *raw, size_t raw_len)
{
    struct cm_lset *s = ctx;
    size_t at = 0, key;
    (void)fields;
    if (section != VCS_SEMANTIC_SECTION_V1_LOOKUPS)
        return true;
    /* includer, spelled, form, kind | hit, hit_slot, miss_evidence, ... */
    if (!cm_text_end(raw, raw_len, &at) || !cm_text_end(raw, raw_len, &at) ||
        raw_len - at < 2)
        return s->bad = true, false;
    key = at + 2;
    at = key;
    if (!cm_text_end(raw, raw_len, &at) || raw_len - at < 5 ||
        !cm_grow((void **)&s->v, &s->cap, s->n, sizeof(*s->v)))
        return s->bad = true, false;
    s->v[s->n++] = (struct cm_lrec){.raw = raw, .len = raw_len,
                                    .key_len = key, .evidence = raw[at + 4]};
    return true;
}

static bool cm_lset(const uint8_t *m, size_t n, struct cm_lset *s)
{
    return vcs_semantic_manifest_v1_each(m, n, cm_lset_cb, s) && !s->bad;
}

/* The spelled name of a LOOKUPS record, for a reason. */
static void cm_lrec_name(const struct cm_lrec *r, const char **name,
                         int *len)
{
    size_t at = 0, start;
    (void)cm_text_end(r->raw, r->len, &at);
    start = at;
    (void)cm_text_end(r->raw, r->len, &at);
    *name = (const char *)r->raw + start + 4;
    *len = (int)(at - start - 4);
}

/* A warm lookup is stale when the accepted manifest resolved the same
 * directive (same key) and none of its records for that key is this one:
 * the probes moved (a shadowing file now sits in a slot it saw absent, a
 * present slot went away) or the hit did. */
static bool cm_lrec_stale(const struct cm_lset *prev, const struct cm_lrec *r)
{
    bool keyed = false;
    for (size_t k = 0; k < prev->n; k++) {
        const struct cm_lrec *p = &prev->v[k];
        if (p->key_len != r->key_len || memcmp(p->raw, r->raw, r->key_len) != 0)
            continue;
        if (p->len == r->len && memcmp(p->raw, r->raw, r->len) == 0)
            return false;
        keyed = true;
    }
    return keyed;
}

static bool cm_lsets_agree(const struct cm_lset *prev, const struct cm_lset *next,
                           struct cm_bind *b)
{
    for (size_t k = 0; k < next->n; k++) {
        const struct cm_lrec *r = &next->v[k];
        const char *name;
        int len;
        if (!cm_lrec_stale(prev, r))
            continue;
        cm_lrec_name(r, &name, &len);
        return cm_bind_fail(b, "lookup-moved %.*s", len, name);
    }
    return true;
}

bool cm_warm_lookups_agree(const uint8_t *prev, size_t prev_len,
                           const uint8_t *next, size_t next_len, char *why,
                           size_t why_len)
{
    struct cm_bind b = {.why = why, .why_len = why_len};
    struct cm_lset a = {0}, c = {0};
    uint8_t ra[32], rc[32];
    bool ok = vcs_semantic_section_root_v1(prev, prev_len,
                                           VCS_SEMANTIC_SECTION_V1_IDENTITY,
                                           ra) &&
              vcs_semantic_section_root_v1(next, next_len,
                                           VCS_SEMANTIC_SECTION_V1_IDENTITY,
                                           rc);
    if (!ok)
        return cm_bind_fail(&b, "a manifest's IDENTITY is unreadable");
    if (memcmp(ra, rc, 32) != 0)
        return cm_bind_fail(&b, "identity-moved (the search list changed)");
    ok = cm_lset(prev, prev_len, &a) && cm_lset(next, next_len, &c);
    if (!ok)
        (void)cm_bind_fail(&b, "a manifest's LOOKUPS are unreadable");
    ok = ok && cm_lsets_agree(&a, &c, &b);
    free(a.v);
    free(c.v);
    return ok;
}

bool cm_warm_bound(const char *root, const uint8_t *m, size_t n, char *why,
                   size_t why_len)
{
    struct cm_bind b = {.root = root, .why = why, .why_len = why_len};
    struct cm_lset s = {0};
    bool ok = vcs_semantic_section_v1_each(m, n, VCS_SEMANTIC_SECTION_V1_FILES,
                                           cm_bind_file_cb, &b) &&
              cm_lset(m, n, &s);
    if (!ok)
        (void)cm_bind_fail(&b, "the previous manifest is unreadable");
    for (size_t k = 0; ok && k < s.n; k++) {
        const char *name;
        int len;
        if (s.v[k].evidence != VCS_SEMANTIC_MISS_V1_NONE)
            continue;
        cm_lrec_name(&s.v[k], &name, &len);
        ok = cm_bind_fail(&b, "lookup-unbound %.*s", len, name);
    }
    free(s.v);
    return ok;
}
