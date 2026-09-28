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

#define CM_PRESENT_MAX 256

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

/* Walk the slots from `first` on; every slot before it is one the search
 * never looks at (an include_next start), recorded present: unclaimed.
 * With want_real set, stop at the slot whose regular file is that exact
 * file (the compiler's reported hit) and record any earlier regular file
 * that the compiler skipped; without it, stop at the first regular file (a
 * replay of the search). Misses are stat-confirmed. False when the slots
 * before `first` do not fit the present list. */
static bool cm_probe_run(const struct cm_core *c, struct cm_probe *p,
                         const char *want_real, uint32_t first)
{
    uint32_t count = cm_slot_count(c, p->form);
    p->hit_slot = count;
    p->npresent = 0;
    p->found = false;
    if (first > CM_PRESENT_MAX)
        return false;
    for (uint32_t s = 0; s < first && s < count; s++)
        p->present[p->npresent++] = s;
    for (uint32_t s = first; s < count; s++) {
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
            return true;
        }
        if (p->npresent < CM_PRESENT_MAX)
            p->present[p->npresent++] = s;
    }
    return true;
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

/* How a directive entered its hit, for a __has_include_next the hit makes
 * (cm_cond_next): the combined search index (quote dirs, then angled dirs)
 * of the slot the replay found it in; CM_ENTRY_NORMAL when found in its
 * includer's own dir (the front end then records no search slot, and
 * include_next searches as include does); CM_ENTRY_UNKNOWN when the
 * directive makes no claim (include_next, computed, absolute, or a replay
 * that did not find the compiler's file). */
static uint32_t cm_entry_of(const struct cm_core *c, const struct cm_probe *p,
                            uint8_t evidence)
{
    if (evidence == VCS_SEMANTIC_MISS_V1_NONE || !p->found)
        return CM_ENTRY_UNKNOWN;
    if (p->form != VCS_SEMANTIC_FORM_V1_QUOTED)
        return (uint32_t)c->quote.n + p->hit_slot;
    return p->hit_slot == 0 ? CM_ENTRY_NORMAL : p->hit_slot - 1;
}

static bool cm_entry_add(struct cm_core *c, const struct cm_file *f,
                         uint32_t slot)
{
    for (size_t k = 0; k < c->nentries; k++)
        if (c->entries[k].file == f && c->entries[k].slot == slot)
            return true;
    if (!cm_grow((void **)&c->entries, &c->capentries, c->nentries,
                 sizeof(*c->entries)))
        return cm_fail(c, "out of memory");
    c->entries[c->nentries++] = (struct cm_entry){.file = f, .slot = slot};
    return true;
}

bool cm_lookup_directive(struct cm_core *c, const struct cm_file *includer,
                         const char *spelled, uint8_t form, uint8_t kind,
                         bool computed, const struct cm_file *hit)
{
    struct cm_probe p = {.form = form, .name = spelled};
    uint8_t evidence = VCS_SEMANTIC_MISS_V1_DERIVED_STAT;
    if (hit == NULL)
        return cm_fail(c, "include of \"%s\" in %s resolved to no read file",
                       spelled, includer->path);
    cm_dir_of(includer->opened, p.includer_dir);
    if (kind == VCS_SEMANTIC_LOOKUP_V1_INCLUDE_NEXT || computed ||
        spelled[0] == '/') {
        p.hit_slot = cm_slot_count(c, p.form);
        evidence = VCS_SEMANTIC_MISS_V1_NONE;
        kind = kind == VCS_SEMANTIC_LOOKUP_V1_INCLUDE_NEXT
                   ? kind : VCS_SEMANTIC_LOOKUP_V1_INCLUDE;
    } else if (!cm_probe_run(c, &p, hit->real, 0) || !p.found) {
        p.npresent = 0;
        p.found = false;
        evidence = VCS_SEMANTIC_MISS_V1_NONE;
    }
    return cm_entry_add(c, hit, cm_entry_of(c, &p, evidence)) &&
           cm_emit_lookup(c, includer, &p, kind, hit->path, evidence);
}

/* ---- conditional lookups: text scan + search replay --------------------------- */

/* __has_include and its relatives ask whether a name resolves, and #embed
 * reads a resource, without an include directive the front end reports. The
 * scan finds every one in every file the TU reads, a system header's too
 * (its angled searches start at the repo's -I dirs), and records each as a
 * lookup; a word only an #ifdef-like directive or a plain defined test
 * reads is none (cm_defined_operand). A
 * __has_include whose operand is a literal "x" or <x>, or an object-like
 * repo macro every definition of which is one plain string literal (the
 * front end's recorded expansion at that offset names the definition;
 * facts manifests only, since only they keep expansions), is
 * replayed: the producer runs the search itself and stat-confirms every
 * probe (REPLAYED_STAT). A __has_include_next is replayed the same way from
 * the slot after each one its file was entered through (cm_entry_add), or,
 * in a system header, from the start and after each search dir that could
 * hold it (cm_cond_next_system); the slots before that start are recorded
 * present, with no claim. The sensor refuses every -X pass-through, -Wp,,
 * front-end plugins and MSVC compatibility,
 * whose -D, -std or trigraph rules the scan cannot read. Every other
 * spelling - another macro operand, __has_embed, the GNU __has_include__
 * words, #embed, an include_next start no slot names, an occurrence a
 * line continuation or trigraph runs through, an operand holding a
 * comment opener, a probe word in any file's #define body (a system
 * header's too) or in a -D value - is recorded with no negative claim
 * (MISS_V1_NONE), as include_next is: a warm session then recreates the TU,
 * and the facts consumer treats it as reachable by every created or deleted
 * path (and #embed or __has_embed by every changed path). The scan reads
 * the text as translation phases 1 to 3 do, with the trigraph and digit
 * separator rules the front end measured for the TU (cm_measure_lang,
 * cm_splice, cm_code_mask): a word in a comment or a
 * character or string literal is no lookup, but one in a skipped group
 * counts too, which costs warm reuse and precision, never truth.
 * Contract: docs/work/SEMANTIC_MANIFEST.md, "Warm session". */

/* The longest spelled name an unbound record keeps. */
#define CM_COND_NAME_MAX 200

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

/* A file's text as translation phases 1 and 2 leave it: each trigraph
 * replaced (when the TU's mode has them) and every backslash-newline (clang
 * also takes spaces between the two) removed. splices holds, strictly
 * increasing, each offset of the spliced text that one was removed before,
 * or that holds a trigraph's replacement: an offset the front end reports
 * past one no longer names the same byte here. */
struct cm_spliced {
    const char *s;
    size_t n;
    char *owned;
    size_t *splices;
    size_t nsplices, cap;
};

/* The end of a continuation starting at the backslash s[i], or 0. */
static size_t cm_continuation_end(const char *s, size_t i, size_t n)
{
    size_t j = cm_skip_space(s, i + 1, n);
    if (j < n && s[j] == '\n')
        return j + 1;
    if (j + 1 < n && s[j] == '\r' && s[j + 1] == '\n')
        return j + 2;
    return 0;
}

/* The character the trigraph at s[i] stands for, else 0. */
static char cm_trigraph(const char *s, size_t i, size_t n)
{
    static const char from[] = "=/'()!<>-", to[] = "#\\^[]|{}~";
    const char *p;
    if (n - i < 3 || s[i] != '?' || s[i + 1] != '?' || s[i + 2] == '\0')
        return 0;
    p = strchr(from, s[i + 2]);
    return p == NULL ? 0 : to[p - from];
}

static bool cm_any_trigraph(const char *s, size_t n)
{
    for (size_t i = 0; i + 2 < n; i++)
        if (cm_trigraph(s, i, n) != 0)
            return true;
    return false;
}

static bool cm_splice_mark(struct cm_spliced *t, size_t w)
{
    if (!cm_grow((void **)&t->splices, &t->cap, t->nsplices,
                 sizeof(*t->splices)))
        return false;
    if (t->nsplices == 0 || t->splices[t->nsplices - 1] != w)
        t->splices[t->nsplices++] = w;
    return true;
}

static bool cm_splice(const char *s, size_t n, bool trigraphs,
                      struct cm_spliced *t)
{
    size_t w = 0;
    memset(t, 0, sizeof(*t));
    t->s = s;
    t->n = n;
    trigraphs = trigraphs && cm_any_trigraph(s, n);
    if (!trigraphs && memchr(s, '\\', n) == NULL)
        return true;
    t->owned = zcl_malloc(n + 1, "clang_manifest.spliced");
    if (t->owned == NULL)
        return false;
    for (size_t i = 0; i < n;) {
        char tri = trigraphs ? cm_trigraph(s, i, n) : 0;
        char ch = tri != 0 ? tri : s[i];
        size_t width = tri != 0 ? 3 : 1;
        size_t end = ch == '\\' ? cm_continuation_end(s, i + width - 1, n) : 0;
        if ((end != 0 || tri != 0) && !cm_splice_mark(t, w))
            return false;
        if (end != 0) {
            i = end;
            continue;
        }
        t->owned[w++] = ch;
        i += width;
    }
    t->owned[w] = '\0';
    t->s = t->owned;
    t->n = w;
    return true;
}

static void cm_spliced_free(struct cm_spliced *t)
{
    free(t->owned);
    free(t->splices);
}

/* Did a continuation run through the occurrence [start, end)? */
static bool cm_spliced_inside(const struct cm_spliced *t, size_t start,
                              size_t end)
{
    for (size_t k = 0; k < t->nsplices; k++) {
        if (t->splices[k] > start && t->splices[k] < end)
            return true;
    }
    return false;
}

/* The quoted or angled operand starting at s[j]; copied to name. *close is
 * the offset of its closing character. */
static bool cm_include_operand(const char *s, size_t j, size_t n,
                               uint8_t *form, char name[PATH_MAX],
                               size_t *close)
{
    size_t end;
    char want;
    if (j >= n || (s[j] != '"' && s[j] != '<'))
        return false;
    want = s[j] == '"' ? '"' : '>';
    *form = s[j] == '"' ? VCS_SEMANTIC_FORM_V1_QUOTED : VCS_SEMANTIC_FORM_V1_ANGLED;
    for (end = j + 1; end < n && s[end] != want && s[end] != '\n'; end++)
        ;
    if (end >= n || s[end] != want || end - j - 1 == 0 || end - j - 1 >= PATH_MAX)
        return false;
    memcpy(name, s + j + 1, end - j - 1);
    name[end - j - 1] = '\0';
    *close = end;
    return true;
}

/* What the scan can replay of a conditional-lookup word. */
enum cm_cond_kind {
    CM_COND_OTHER, /* recorded unbound */
    CM_COND_PLAIN, /* __has_include */
    CM_COND_NEXT,  /* __has_include_next */
};

/* The length of the conditional-lookup word at s[i], else 0; *kind: what
 * the scan can replay of it. */
static size_t cm_cond_word(const char *s, size_t i, size_t n,
                           enum cm_cond_kind *kind)
{
    static const char *const words[] = {
        "__has_include_next__", "__has_include_next", "__has_include__",
        "__has_include", "__has_embed",
    };
    if (s[i] != '_' || (i > 0 && cm_ident_char(s[i - 1])))
        return 0;
    for (size_t w = 0; w < sizeof(words) / sizeof(words[0]); w++) {
        size_t len = strlen(words[w]);
        if (n - i >= len && memcmp(s + i, words[w], len) == 0 &&
            (i + len == n || !cm_ident_char(s[i + len]))) {
            *kind = strcmp(words[w], "__has_include") == 0 ? CM_COND_PLAIN
                    : strcmp(words[w], "__has_include_next") == 0
                        ? CM_COND_NEXT
                        : CM_COND_OTHER;
            return len;
        }
    }
    return 0;
}

/* Skip spaces, tabs and block comments: what may sit between a directive's
 * '#' and its name. */
static size_t cm_skip_directive_space(const char *s, size_t i, size_t n)
{
    for (;;) {
        i = cm_skip_space(s, i, n);
        if (n - i < 2 || s[i] != '/' || s[i + 1] != '*')
            return i;
        for (i += 2; n - i >= 2 && (s[i] != '*' || s[i + 1] != '/'); i++)
            ;
        if (n - i < 2)
            return n;
        i += 2;
    }
}

/* An #embed (or %:embed) directive's name at s[i]: the offset just past
 * "embed", else 0. */
static size_t cm_embed_at(const char *s, size_t i, size_t n)
{
    size_t j;
    if (s[i] == '#')
        j = i + 1;
    else if (s[i] == '%' && n - i >= 2 && s[i + 1] == ':')
        j = i + 2;
    else
        return 0;
    j = cm_skip_directive_space(s, j, n);
    if (n - j < 5 || memcmp(s + j, "embed", 5) != 0 ||
        (j + 5 < n && cm_ident_char(s[j + 5])))
        return 0;
    return j + 5;
}

/* A __has_include(...) with a literal operand, closed on its own spelling:
 * form and name; *end is just past the ')'. An operand holding a comment
 * opener is none: which name the front end reads there is left unbound. */
static bool cm_literal_at(const char *s, size_t i, size_t n, uint8_t *form,
                          char name[PATH_MAX], size_t *end)
{
    size_t j = cm_skip_space(s, i, n), close;
    if (j >= n || s[j] != '(')
        return false;
    if (!cm_include_operand(s, cm_skip_space(s, j + 1, n), n, form, name,
                            &close) ||
        strstr(name, "/*") != NULL || strstr(name, "//") != NULL)
        return false;
    j = cm_skip_space(s, close + 1, n);
    if (j >= n || s[j] != ')')
        return false;
    *end = j + 1;
    return true;
}

/* The form of an unbound occurrence s[i, end): angled when its operand's
 * first quote or angle is '<', else quoted. */
static uint8_t cm_cond_form(const char *s, size_t i, size_t end)
{
    size_t open = i;
    while (open < end && s[open] != '"' && s[open] != '<')
        open++;
    return open < end && s[open] == '<' ? VCS_SEMANTIC_FORM_V1_ANGLED
                                        : VCS_SEMANTIC_FORM_V1_QUOTED;
}

/* An unbound occurrence's spelled name: from s[i] through the first ')' of
 * its line, else to the line's end, trailing blanks dropped, with prefix in
 * front; and its form. */
static void cm_cond_name(const char *prefix, const char *s, size_t i,
                         size_t n, char name[PATH_MAX], uint8_t *form)
{
    size_t end = i;
    while (end < n && s[end] != '\n' && s[end] != ')')
        end++;
    if (end < n && s[end] == ')')
        end++;
    while (end > i && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
                       s[end - 1] == '\r'))
        end--;
    if (end - i > CM_COND_NAME_MAX)
        end = i + CM_COND_NAME_MAX;
    (void)snprintf(name, PATH_MAX, "%s%.*s", prefix, (int)(end - i), s + i);
    *form = cm_cond_form(s, i, end);
}

/* A lookup the scan found and cannot replay: no hit, no negative claim. */
static bool cm_cond_unbound(struct cm_core *c, const struct cm_file *f,
                            const char *name, uint8_t form)
{
    struct cm_probe p = {.form = form, .name = name};
    cm_dir_of(f->opened, p.includer_dir);
    p.hit_slot = cm_slot_count(c, form);
    return cm_emit_lookup(c, f, &p, VCS_SEMANTIC_LOOKUP_V1_HAS_INCLUDE, "",
                          VCS_SEMANTIC_MISS_V1_NONE);
}

/* Replay the search for `name` from slot `first` of form's numbering;
 * *fits false (nothing emitted) when the slots before it do not fit. */
static bool cm_has_include_from(struct cm_core *c, const struct cm_file *f,
                                uint8_t form, const char *name,
                                uint32_t first, bool *fits)
{
    struct cm_probe p = {.form = form, .name = name};
    char *hit_path = NULL;
    bool ok;
    cm_dir_of(f->opened, p.includer_dir);
    *fits = cm_probe_run(c, &p, NULL, first);
    if (!*fits)
        return true;
    if (p.found && !cm_norm_path(c, p.found_real, &hit_path))
        return false;
    ok = cm_emit_lookup(c, f, &p, VCS_SEMANTIC_LOOKUP_V1_HAS_INCLUDE,
                        p.found ? hit_path : "",
                        VCS_SEMANTIC_MISS_V1_REPLAYED_STAT);
    free(hit_path);
    return ok;
}

static bool cm_has_include_one(struct cm_core *c, const struct cm_file *f,
                               uint8_t form, const char *name)
{
    bool fits;
    return cm_has_include_from(c, f, form, name, 0, &fits);
}

/* One entry of f for __has_include_next(name): the main file, or a file
 * found in its includer's own dir, searches as __has_include does (the
 * front end warns and starts over); a file found in combined slot e
 * searches from e + 1, which is quoted slot e + 2 (slot 0 is the
 * includer's dir, which include_next never searches). */
static bool cm_next_entry(struct cm_core *c, const struct cm_file *f,
                          uint8_t form, const char *name, const char *raw,
                          uint8_t raw_form, uint32_t entry)
{
    bool fits = false, ok;
    if (entry == CM_ENTRY_NORMAL)
        return cm_has_include_one(c, f, form, name);
    ok = entry == CM_ENTRY_UNKNOWN ||
         cm_has_include_from(c, f, VCS_SEMANTIC_FORM_V1_QUOTED, name,
                             entry + 2, &fits);
    return ok && (fits || cm_cond_unbound(c, f, raw, raw_form));
}

/* Could f have been found in the search dir `raw`: its raw spelling opens
 * f's opened name, or its realpath holds f's realpath? */
static bool cm_dir_holds(const char *raw, const struct cm_file *f)
{
    char real[PATH_MAX];
    size_t n = strlen(raw);
    while (n > 1 && raw[n - 1] == '/')
        n--;
    if (strncmp(f->opened, raw, n) == 0 && f->opened[n] == '/')
        return true;
    if (realpath(raw, real) == NULL)
        return false;
    n = strlen(real);
    return n > 1 && strncmp(f->real, real, n) == 0 && f->real[n] == '/';
}

/* __has_include_next(name) in a system file. Its entries come from other
 * system files too, which record no lookups, so every way it could have
 * been entered is replayed: found relative or by absolute path (searches
 * as __has_include does), and found in each search dir that could hold it
 * (searches the dirs after it). Unbound when no search dir holds it. */
static bool cm_cond_next_system(struct cm_core *c, const struct cm_file *f,
                                uint8_t form, const char *name,
                                const char *raw, uint8_t raw_form)
{
    size_t nq = c->quote.n, all = nq + c->angled.n;
    bool any = false, ok = cm_has_include_one(c, f, form, name);
    for (size_t e = 0; ok && e < all; e++) {
        const char *dir = e < nq ? c->quote.items[e].raw
                                 : c->angled.items[e - nq].raw;
        bool fits = false;
        if (!cm_dir_holds(dir, f))
            continue;
        any = true;
        ok = cm_has_include_from(c, f, VCS_SEMANTIC_FORM_V1_QUOTED, name,
                                 (uint32_t)e + 2, &fits) &&
             (fits || cm_cond_unbound(c, f, raw, raw_form));
    }
    return ok && (any || cm_cond_unbound(c, f, raw, raw_form));
}

/* __has_include_next(name) in f, once per way f was entered; unbound when
 * f was entered in a way that names no search slot. */
static bool cm_cond_next(struct cm_core *c, const struct cm_file *f,
                         uint8_t form, const char *name, const char *raw,
                         uint8_t raw_form)
{
    bool any = false, ok = true;
    if (f->origin == VCS_SEMANTIC_ORIGIN_V1_MAIN)
        return cm_has_include_one(c, f, form, name);
    if (f->origin == VCS_SEMANTIC_ORIGIN_V1_SYSTEM)
        return cm_cond_next_system(c, f, form, name, raw, raw_form);
    for (size_t k = 0; ok && k < c->nentries; k++) {
        if (c->entries[k].file != f)
            continue;
        any = true;
        ok = cm_next_entry(c, f, form, name, raw, raw_form,
                           c->entries[k].slot);
    }
    return ok && (any || cm_cond_unbound(c, f, raw, raw_form));
}

/* ---- a macro operand: the literals its expansion there can give ------------- */

#define CM_MACRO_NAMES_MAX 8

struct cm_names {
    char v[CM_MACRO_NAMES_MAX][PATH_MAX];
    size_t n;
};

/* The identifier operand of "( IDENT )" at s[j]: [*a, *b). */
static bool cm_ident_operand(const char *s, size_t j, size_t n, size_t *a,
                             size_t *b)
{
    j = cm_skip_space(s, j, n);
    if (j >= n || s[j] != '(')
        return false;
    *a = cm_skip_space(s, j + 1, n);
    if (*a >= n || !cm_ident_char(s[*a]) || (s[*a] >= '0' && s[*a] <= '9'))
        return false;
    for (*b = *a; *b < n && cm_ident_char(s[*b]); (*b)++)
        ;
    j = cm_skip_space(s, *b, n);
    return j < n && s[j] == ')';
}

/* A macro body that is one plain string literal: its contents. */
static bool cm_string_body(const char *body, char name[PATH_MAX])
{
    size_t n = strlen(body);
    if (n < 3 || n - 2 >= PATH_MAX || body[0] != '"' || body[n - 1] != '"' ||
        memchr(body + 1, '"', n - 2) != NULL ||
        memchr(body + 1, '\\', n - 2) != NULL)
        return false;
    memcpy(name, body + 1, n - 2);
    name[n - 2] = '\0';
    return true;
}

static bool cm_names_add(struct cm_names *out, const char *name)
{
    for (size_t k = 0; k < out->n; k++)
        if (strcmp(out->v[k], name) == 0)
            return true;
    if (out->n == CM_MACRO_NAMES_MAX)
        return false;
    (void)snprintf(out->v[out->n++], PATH_MAX, "%s", name);
    return true;
}

/* Every repo definition of `ident` whose id is def_id is an object-like
 * macro whose body is one plain string literal; add each. */
static bool cm_def_literals(const struct cm_core *c, const char *ident,
                            const char *def_id, struct cm_names *out)
{
    bool matched = false;
    for (size_t k = 0; k < c->nmacros; k++) {
        const struct cm_macro *m = &c->macros[k];
        char name[PATH_MAX], *id;
        bool same;
        if (strcmp(m->name, ident) != 0)
            continue;
        id = cm_id('m', m->path, m->name, 0);
        same = id != NULL && strcmp(id, def_id) == 0;
        free(id);
        if (!same)
            continue;
        matched = true;
        if (m->function_like || !cm_string_body(m->body, name) ||
            !cm_names_add(out, name))
            return false;
    }
    return matched;
}

/* The literals the macro operand spelled at f's offset [a, b) expands to:
 * every expansion the front end recorded there names a repo definition,
 * and every such definition is a plain string literal. False (unbound)
 * otherwise: no expansion there (a skipped group, or offsets a line
 * continuation moved), a predefined or builtin macro, or another body. */
static bool cm_macro_names(const struct cm_core *c, const struct cm_file *f,
                           const struct cm_spliced *t, size_t a, size_t b,
                           struct cm_names *out)
{
    char ident[CM_COND_NAME_MAX + 1];
    bool any = false;
    if (t->nsplices != 0 || b - a > CM_COND_NAME_MAX)
        return false;
    memcpy(ident, t->s + a, b - a);
    ident[b - a] = '\0';
    for (size_t k = 0; k < c->nexps; k++) {
        const struct cm_expansion *e = &c->exps[k];
        if (e->file != f || e->offset != a || strcmp(e->name, ident) != 0)
            continue;
        any = true;
        if (e->def_id == NULL || !cm_def_literals(c, ident, e->def_id, out))
            return false;
    }
    return any;
}

/* A __has_include or __has_include_next whose operand is a literal, or a
 * macro that expands to literals: replay each; false in *done otherwise. */
static bool cm_cond_resolve(struct cm_core *c, const struct cm_file *f,
                            const struct cm_spliced *t, size_t i, size_t len,
                            enum cm_cond_kind kind, bool *done)
{
    struct cm_names names = {0};
    char raw[PATH_MAX];
    uint8_t form = VCS_SEMANTIC_FORM_V1_QUOTED, raw_form;
    size_t end, a, b;
    bool ok = true;
    *done = true;
    if (cm_literal_at(t->s, i + len, t->n, &form, names.v[0], &end) &&
        !cm_spliced_inside(t, i, end))
        names.n = 1;
    else if (!cm_ident_operand(t->s, i + len, t->n, &a, &b) ||
             !cm_macro_names(c, f, t, a, b, &names))
        return *done = false, true;
    cm_cond_name("", t->s, i, t->n, raw, &raw_form);
    for (size_t k = 0; ok && k < names.n; k++)
        ok = kind == CM_COND_NEXT
                 ? cm_cond_next(c, f, form, names.v[k], raw, raw_form)
                 : cm_has_include_one(c, f, form, names.v[k]);
    return ok;
}

/* The end of the identifier at s[j], bounded by n; j when none starts. */
static size_t cm_word_end(const char *s, size_t j, size_t n)
{
    while (j < n && cm_ident_char(s[j]))
        j++;
    return j;
}

static bool cm_word_is(const char *s, size_t a, size_t b, const char *w)
{
    return b - a == strlen(w) && memcmp(s + a, w, b - a) == 0;
}

/* The directive on the line holding s[a]: its name [*name, *end), found
 * after only blanks, a '#' or "%:", and blanks. False on any other line. */
static bool cm_line_directive(const char *s, size_t a, size_t *name,
                              size_t *end)
{
    size_t j = a;
    while (j > 0 && s[j - 1] != '\n')
        j--;
    j = cm_skip_space(s, j, a);
    if (j < a && s[j] == '#')
        j++;
    else if (a - j >= 2 && s[j] == '%' && s[j + 1] == ':')
        j += 2;
    else
        return false;
    *name = cm_skip_space(s, j, a);
    *end = cm_word_end(s, *name, a);
    return true;
}

/* The end of the defined test "defined W" or "defined ( W )" at s[j],
 * bounded by a; 0 when s[j] starts none. */
static size_t cm_defined_test_end(const char *s, size_t j, size_t a)
{
    size_t e = cm_word_end(s, j, a), w;
    bool paren;
    if (!cm_word_is(s, j, e, "defined"))
        return 0;
    j = cm_skip_space(s, e, a);
    paren = j < a && s[j] == '(';
    w = cm_skip_space(s, paren ? j + 1 : j, a);
    e = cm_word_end(s, w, a);
    if (e == w)
        return 0;
    j = cm_skip_space(s, e, a);
    if (!paren)
        return j;
    return j < a && s[j] == ')' ? j + 1 : 0;
}

/* Is [j, a) only defined tests and the operators ! && ||? Any other
 * identifier could be a macro whose expansion reaches the "defined" at
 * s[a] (a paste there makes it another identifier), and no other token
 * is modelled. */
static bool cm_only_defined_tests(const char *s, size_t j, size_t a)
{
    while (j < a) {
        size_t e;
        if (s[j] == ' ' || s[j] == '\t' || s[j] == '!' || s[j] == '&' ||
            s[j] == '|') {
            j++;
            continue;
        }
        e = cm_defined_test_end(s, j, a);
        if (e == 0)
            return false;
        j = e;
    }
    return true;
}

/* The identifier [*a, *b) before the word at s[i], across blanks and at
 * most one '(' (*paren); *a == *b when none. */
static void cm_word_before(const char *s, size_t i, size_t *a, size_t *b,
                           bool *paren)
{
    size_t j = i;
    *paren = false;
    while (j > 0 && (s[j - 1] == ' ' || s[j - 1] == '\t'))
        j--;
    if (j > 0 && s[j - 1] == '(') {
        *paren = true;
        for (j--; j > 0 && (s[j - 1] == ' ' || s[j - 1] == '\t'); j--)
            ;
    }
    for (*b = j; j > 0 && cm_ident_char(s[j - 1]); j--)
        ;
    *a = j;
}

static bool cm_ifdef_word(const char *s, size_t a, size_t b)
{
    return cm_word_is(s, a, b, "ifdef") || cm_word_is(s, a, b, "ifndef") ||
           cm_word_is(s, a, b, "elifdef") || cm_word_is(s, a, b, "elifndef");
}

/* Is the word at s[i] only tested for being defined? Either the operand
 * of an #ifdef-like directive (the word right after the '#': a macro may
 * be named ifdef), or of a "defined W" or "defined ( W" on an #if or
 * #elif line after nothing but other defined tests. Blanks only between;
 * a comment there leaves it a lookup. The glibc headers guard each
 * __has_include with #ifdef __has_include. */
static bool cm_defined_operand(const char *s, size_t i)
{
    size_t a, b, name, end;
    bool paren;
    cm_word_before(s, i, &a, &b, &paren);
    if (a == b || !cm_line_directive(s, a, &name, &end))
        return false;
    if (cm_word_is(s, a, b, "defined"))
        return (cm_word_is(s, name, end, "if") ||
                cm_word_is(s, name, end, "elif")) &&
               cm_only_defined_tests(s, end, a);
    return !paren && name == a && cm_ifdef_word(s, a, b);
}

/* The conditional lookup at t[i], if any: replayed or recorded unbound. */
static bool cm_cond_at(struct cm_core *c, const struct cm_file *f,
                       const struct cm_spliced *t, size_t i)
{
    char name[PATH_MAX];
    uint8_t form;
    enum cm_cond_kind kind = CM_COND_OTHER;
    bool done = false;
    size_t len = cm_cond_word(t->s, i, t->n, &kind), end;
    if (len != 0 && cm_defined_operand(t->s, i))
        return true;
    if (len != 0 && kind != CM_COND_OTHER &&
        !cm_cond_resolve(c, f, t, i, len, kind, &done))
        return false;
    if (done)
        return true;
    if (len != 0) {
        cm_cond_name("", t->s, i, t->n, name, &form);
        return cm_cond_unbound(c, f, name, form);
    }
    end = cm_embed_at(t->s, i, t->n);
    if (end == 0)
        return true;
    cm_cond_name("#embed ", t->s, cm_skip_space(t->s, end, t->n), t->n, name,
                 &form);
    return cm_cond_unbound(c, f, name, form);
}

/* ---- where a conditional lookup can start: code, not comments or literals -- */

/* A word after which '<' opens a header name: an include-like directive's,
 * or a conditional-lookup word's (past its '('). */
static bool cm_hdr_word(const char *s, size_t a, size_t b)
{
    static const char *const words[] = {
        "include",       "include_next",       "import",
        "embed",         "__has_include",      "__has_include_next",
        "__has_include__", "__has_include_next__", "__has_embed",
    };
    for (size_t k = 0; k < sizeof(words) / sizeof(words[0]); k++)
        if (strlen(words[k]) == b - a && memcmp(s + a, words[k], b - a) == 0)
            return true;
    return false;
}

static bool cm_blank(char ch)
{
    return ch == ' ' || ch == '\t';
}

/* The end of the header name the '<' at s[i] opens: just past its '>'; 0
 * when a '"' or the line's end comes before the '>' (then it is a
 * punctuator and lexing goes on after it). */
static size_t cm_hdr_close(const char *s, size_t i, size_t n)
{
    for (size_t j = i + 1; j < n && s[j] != '\n' && s[j] != '"'; j++)
        if (s[j] == '>')
            return j + 1;
    return 0;
}

/* The end of the line or block comment at s[i]: a line comment leaves its
 * newline; an unterminated block comment runs to the end. */
static size_t cm_comment_end(const char *s, size_t i, size_t n)
{
    const char *end;
    if (s[i + 1] == '/') {
        end = memchr(s + i, '\n', n - i);
        return end == NULL ? n : (size_t)(end - s);
    }
    for (size_t j = i + 2; j + 1 < n; j++)
        if (s[j] == '*' && s[j + 1] == '/')
            return j + 2;
    return n;
}

/* The end of the character or string literal at s[i]: past its closing
 * quote, or at the raw newline that leaves it unterminated. */
static size_t cm_literal_end(const char *s, size_t i, size_t n)
{
    size_t j = i + 1;
    while (j < n && s[j] != s[i] && s[j] != '\n')
        j += s[j] == '\\' && j + 1 < n ? 2 : 1;
    return j < n && s[j] == s[i] ? j + 1 : j;
}

static bool cm_digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

/* The phase-3 lexer over one file's spliced text. */
struct cm_lexer {
    const char *s;
    size_t n;
    bool separators; /* C23 on: a pp-number takes digit separators */
    int hdr;         /* 1 after a header-name word, 2 after its '(' too */
};

/* The end of the pp-number at s[i]: digits, letters, '_', '.', an exponent
 * sign, and (C23 on) a digit separator followed by a digit or nondigit.
 * Before C23 a '\'' there opens a character literal. */
static size_t cm_ppnum_end(const struct cm_lexer *L, size_t i)
{
    const char *s = L->s;
    size_t j = i + 1, n = L->n;
    while (j < n) {
        char c = s[j], p = s[j - 1];
        if ((c == '+' || c == '-') &&
            (p == 'e' || p == 'E' || p == 'p' || p == 'P'))
            j++;
        else if (c == '\'' && L->separators && j + 1 < n &&
                 cm_ident_char(s[j + 1]))
            j += 2;
        else if (cm_ident_char(c) || c == '.')
            j++;
        else
            break;
    }
    return j;
}

/* The end of the comment or the character or string literal at s[i], or 0
 * when none starts there. */
static size_t cm_lex_hidden(const char *s, size_t i, size_t n)
{
    char ch = s[i], nx = i + 1 < n ? s[i + 1] : '\0';
    if (ch == '/' && (nx == '/' || nx == '*'))
        return cm_comment_end(s, i, n);
    if (ch == '"' || ch == '\'')
        return cm_literal_end(s, i, n);
    return 0;
}

/* The end of the token or gap at s[i], as translation phase 3 lexes the
 * spliced text; *code false for a comment or a literal. An identifier ends
 * at its last identifier byte, so an encoding prefix's literal (u8"x") is
 * the next token. A '<' is a header name's only where one may start. */
static size_t cm_lex_one(const struct cm_lexer *L, size_t i, bool *code)
{
    const char *s = L->s;
    size_t n = L->n, j = cm_lex_hidden(L->s, i, L->n);
    char ch = s[i], nx = i + 1 < n ? s[i + 1] : '\0';
    *code = j == 0;
    if (j != 0)
        return j;
    if (cm_digit(ch) || (ch == '.' && cm_digit(nx)))
        return cm_ppnum_end(L, i);
    if (cm_ident_char(ch)) {
        for (j = i + 1; j < n && cm_ident_char(s[j]); j++)
            ;
        return j;
    }
    if (ch == '<' && L->hdr != 0 && (j = cm_hdr_close(s, i, n)) != 0)
        return j;
    return i + 1;
}

/* Where a header name may start after the token [i, end): past a
 * header-name word and its '(', across blanks and comments (each only
 * whitespace to phase 4), and nowhere else. */
static int cm_hdr_after(const struct cm_lexer *L, size_t i, size_t end,
                        bool code)
{
    const char *s = L->s;
    if (!code)
        return s[i] == '/' ? L->hdr : 0;
    if (end - i == 1 && cm_blank(s[i]))
        return L->hdr;
    if (cm_ident_char(s[i]) && !cm_digit(s[i]))
        return cm_hdr_word(s, i, end) ? 1 : 0;
    return L->hdr == 1 && end - i == 1 && s[i] == '(' ? 2 : 0;
}

/* An #define (or %:define) directive's '#' at s[i]. */
static bool cm_define_at(const char *s, size_t i, size_t n)
{
    size_t j;
    if (s[i] == '#')
        j = i + 1;
    else if (s[i] == '%' && n - i >= 2 && s[i + 1] == ':')
        j = i + 2;
    else
        return false;
    j = cm_skip_directive_space(s, j, n);
    return n - j >= 6 && memcmp(s + j, "define", 6) == 0 &&
           (j + 6 == n || !cm_ident_char(s[j + 6]));
}

/* code[i] 2 across each #define directive, from its '#' to the newline
 * that ends it (one inside a comment does not): a conditional-lookup word
 * there is evaluated where the macro expands. */
static void cm_mark_defines(const struct cm_spliced *t, uint8_t *code)
{
    const char *s = t->s;
    size_t n = t->n;
    for (size_t i = 0; i < n;) {
        size_t j = i, end;
        while (j < n && s[j] != '\n' && (cm_blank(s[j]) || code[j] == 0))
            j++;
        for (end = j; end < n && (s[end] != '\n' || code[end] == 0); end++)
            ;
        if (j < n && cm_define_at(s, j, n))
            for (size_t k = j; k < end; k++)
                code[k] = code[k] != 0 ? 2 : 0;
        i = end + 1;
    }
}

/* code[i]: 0 when s[i] lies inside a comment or a character or string
 * literal, 2 inside a #define directive's code, else 1. A conditional-lookup
 * word or directive can only start in code; a word in a comment or a
 * literal is none (a header name's text is code, and never opens a
 * comment). The lexer is phase 3's over the spliced text, with a header
 * name recognized after an include-like directive word or a
 * conditional-lookup word. */
static uint8_t *cm_code_mask(const struct cm_spliced *t, bool separators)
{
    struct cm_lexer L = {.s = t->s, .n = t->n, .separators = separators};
    uint8_t *code = zcl_malloc(t->n + 1, "clang_manifest.code_mask");
    if (code == NULL)
        return NULL;
    for (size_t i = 0; i < t->n;) {
        bool is_code;
        size_t end = cm_lex_one(&L, i, &is_code);
        memset(code + i, is_code ? 1 : 0, end - i);
        L.hdr = cm_hdr_after(&L, i, end, is_code);
        i = end;
    }
    cm_mark_defines(t, code);
    return code;
}

/* ---- what the scan reads: the TU's language, its files, its -D values ------ */

/* A conditional-lookup word at s[i] in a macro's body (a #define's, or a
 * -D value's): evaluated wherever the macro expands, and quoted names then
 * search that file's directory, so the scan cannot replay it: recorded
 * unbound, on f. */
static bool cm_body_at(struct cm_core *c, const struct cm_file *f,
                       const char *s, size_t i, size_t n)
{
    enum cm_cond_kind kind = CM_COND_OTHER;
    char name[PATH_MAX];
    uint8_t form;
    if (cm_cond_word(s, i, n, &kind) == 0)
        return true;
    cm_cond_name("", s, i, n, name, &form);
    return cm_cond_unbound(c, f, name, form);
}

/* The value of the -D (or --define-macro) option at argv[*k], or NULL;
 * *k moves past a separate value. */
static const char *cm_define_value(const char *const *argv, size_t argc,
                                   size_t *k)
{
    const char *a = argv[*k];
    if (strcmp(a, "-D") == 0 || strcmp(a, "--define-macro") == 0)
        return *k + 1 < argc ? argv[++*k] : NULL;
    if (strncmp(a, "-D", 2) == 0)
        return a + 2;
    if (strncmp(a, "--define-macro=", 15) == 0)
        return a + 15;
    return NULL;
}

/* Every conditional-lookup word in a -D value, recorded unbound on the
 * main file: the macro it defines is text no file holds. */
static bool cm_scan_argv(struct cm_core *c, const char *const *argv,
                         size_t argc)
{
    const struct cm_file *main = NULL;
    for (size_t k = 0; main == NULL && k < c->nfiles; k++)
        if (c->files[k].origin == VCS_SEMANTIC_ORIGIN_V1_MAIN)
            main = &c->files[k];
    for (size_t k = 0; k < argc; k++) {
        const char *v = cm_define_value(argv, argc, &k);
        size_t n = v == NULL ? 0 : strlen(v);
        for (size_t i = 0; i < n; i++)
            if (!(main != NULL ? cm_body_at(c, main, v, i, n)
                               : cm_fail(c, "no main file for a -D value")))
                return false;
    }
    return true;
}

/* One file, repo or system: a system header's own conditionals search the
 * -I dirs too (an angled search starts there), so a repo dir gaining a
 * name can flip them. */
static bool cm_scan_file(struct cm_core *c, const struct cm_file *f,
                         struct cm_lang lang)
{
    bool ok;
    struct cm_spliced t;
    uint8_t *code = NULL;
    ok = cm_splice(f->contents, f->size, lang.trigraphs, &t);
    if (!ok)
        (void)cm_fail(c, "cannot splice %s", f->path);
    else if ((code = cm_code_mask(&t, lang.separators)) == NULL)
        ok = cm_fail(c, "out of memory");
    for (size_t i = 0; ok && i < t.n; i++) {
        char ch = t.s[i];
        if (ch != '_' && ch != '#' && ch != '%')
            continue;
        if (code[i] == 2)
            ok = cm_body_at(c, f, t.s, i, t.n);
        else if (code[i] == 1)
            ok = cm_cond_at(c, f, &t, i);
    }
    free(code);
    cm_spliced_free(&t);
    return ok;
}

bool cm_scan_has_include(struct cm_core *c, const char *const *argv,
                         size_t argc, struct cm_lang lang)
{
    if (!cm_scan_argv(c, argv, argc))
        return false;
    for (size_t k = 0; k < c->nfiles; k++)
        if (!cm_scan_file(c, &c->files[k], lang))
            return false;
    return true;
}
