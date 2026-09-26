/* Copyright 2026 Rhett Creighton - Apache License 2.0
 *
 * purpose: HOT_FORK shape admission (see devloop_hotfork_shape.h).
 *
 * The baseline is never a previous candidate. It is the resident's own
 * inputs: the dev build's objects for the capsule's TU set in the epoch
 * named by build/dev-obj/.current-epoch, bound to the running image by
 * three facts — the epoch's completed build session names the action plan's
 * compiler identity, no object is newer than the running image, and every
 * global the objects define is defined by the running image.
 *
 * Shape, compared per capsule:
 *   ABI    defined GLOBAL/WEAK symbols (functions, objects, TLS, common)
 *   state  writable, thread-local and common objects of any binding, by
 *          name (a local static's ".N" suffix is dropped) and size;
 *          .data.rel.ro is read-only after relocation and is not state
 *   init   .preinit_array, .init_array, .fini_array, .ctors, .dtors
 * LOCAL function symbols are not shape: a static function has internal
 * linkage, holds no state, cannot be bound from outside its TU, and appears
 * or disappears with inlining decisions. Adding, renaming or removing one is
 * an implementation-only edit.
 *
 * The unity also carries the story adapter. A candidate-only symbol belongs
 * to the adapter only when its name is a token of the adapter text and of
 * no other file in the candidate's dependency closure; a symbol an owner
 * edit introduces is always a token of the owner (or a header) and is
 * therefore refused.
 *
 * Every read failure refuses: an unreadable or malformed object, a missing
 * symbol table, a missing session or object, an unreadable binding record.
 */
#include "devloop_hotfork_shape.h"

#include "hotfork_unity.h"

#include <stdio.h>
#include <string.h>

bool zcl_hotfork_shape_refused(const char *why)
{
    return why && strncmp(why, ZCL_HOTFORK_SHAPE_REASON,
                          sizeof(ZCL_HOTFORK_SHAPE_REASON) - 1) == 0;
}

static bool shape_refuse(char *why, size_t why_len, const char *reason,
                         const char *subject, const char *detail)
{
    if (why && why_len)
        (void)snprintf(why, why_len, "%s%s %s: %s; fallback=restart",
                       ZCL_HOTFORK_SHAPE_REASON, reason,
                       subject && subject[0] ? subject : "-", detail);
    return false;
}

#if defined(__linux__)

#include "base/hex.h"
#include "util/safe_alloc.h"
#include "sha3/sha3.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* Sibling rule: rr_overlay_object_safe() in devloop_restart_build.c refuses
 * the first three of these for restart overlays. A HOT_FORK candidate runs
 * its constructors at dlopen inside the resident's child, so the old-style
 * .ctors/.dtors spellings are refused as well. */
static const char *const k_shape_init_fini[] = {
    ".preinit_array", ".init_array", ".fini_array", ".ctors", ".dtors",
};

enum {
    SHAPE_SECTION_MAX = 64 << 20, /* bytes read from one ELF section */
    SHAPE_TEXT_MAX = 16 << 20,    /* bytes read from one closure file */
    SHAPE_TU_MAX = 1 + ZCL_HOTFORK_UNITY_SIBLING_MAX,
    SHAPE_DEPS_MAX = 4096,
};

enum shape_kind { SHAPE_GLOBAL, SHAPE_DATA, SHAPE_TLS };
enum shape_read { SHAPE_READ_OK, SHAPE_READ_UNREADABLE, SHAPE_READ_NO_SYMTAB };

struct shape_elf {
    int fd;
    uint64_t size;
    Elf64_Ehdr eh;
    Elf64_Shdr *sh;
    char *shstr;
    size_t shstr_len;
    Elf64_Sym *sym;
    size_t nsym;
    char *str;
    size_t str_len;
};

struct shape_sym {
    const char *name; /* borrowed from the owning shape_elf string table */
    size_t len;       /* compared length */
    uint64_t size;
    enum shape_kind kind;
};

struct shape_set {
    struct shape_sym *items;
    size_t count;
    size_t cap;
    char init_fini[64]; /* first init/fini section seen, "" when none */
};

struct shape_texts {
    bool adapter_tried;
    bool loaded;
    bool ok;
    char *files[SHAPE_DEPS_MAX];
    size_t count;
};

struct shape_run {
    const struct zcl_hotfork_shape *shape;
    const char *candidate;
    const char *depfile;
    struct shape_elf objects[SHAPE_TU_MAX];
    size_t object_count;
    struct shape_elf cand;
    struct shape_set base;
    struct shape_set next;
    char *deps_text;
    char *deps[SHAPE_DEPS_MAX];
    size_t dep_count;
    char closure[65];
    char story[ZCL_HOTFORK_UNITY_TU_MAX];
    char *adapter;
    char record[PATH_MAX];
    bool record_current;
    struct shape_texts texts;
};

/* ── small facts ─────────────────────────────────────────────────────── */

static void shape_hex_root(struct sha3_256_ctx *ctx, char out[65])
{
    unsigned char digest[SHA3_256_OUTPUT_SIZE];
    sha3_256_finalize(ctx, digest);
    zcl_hex_encode(digest, sizeof(digest), out);
}

static void shape_hash_text(struct sha3_256_ctx *ctx, const char *text)
{
    sha3_256_write(ctx, (const unsigned char *)text, strlen(text));
    sha3_256_write(ctx, (const unsigned char *)"\n", 1);
}

static void shape_hash_stat(struct sha3_256_ctx *ctx, const char *label,
                            const struct stat *st)
{
    char line[PATH_MAX + 160];
    int n = snprintf(line, sizeof(line), "%s=%llu:%llu:%lld:%lld.%09ld",
                     label, (unsigned long long)st->st_dev,
                     (unsigned long long)st->st_ino, (long long)st->st_size,
                     (long long)st->st_mtim.tv_sec, (long)st->st_mtim.tv_nsec);
    if (n > 0 && n < (int)sizeof(line))
        shape_hash_text(ctx, line);
}

static bool shape_hex64(const char *text)
{
    size_t n = 0;
    while (text[n] && ((text[n] >= '0' && text[n] <= '9') ||
                       (text[n] >= 'a' && text[n] <= 'f')))
        n++;
    return n == 64 && text[64] == 0;
}

static bool shape_newer(const struct stat *a, const struct stat *b)
{
    return a->st_mtim.tv_sec > b->st_mtim.tv_sec ||
           (a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
            a->st_mtim.tv_nsec > b->st_mtim.tv_nsec);
}

static bool shape_regular(const char *path, struct stat *st)
{
    return stat(path, st) == 0 && S_ISREG(st->st_mode);
}

/* Reads a small text file whole; NULL when absent, unreadable, too large or
 * holding a NUL byte (text searches must see every byte). */
static char *shape_slurp(const char *path, size_t max, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    char *buf = f ? zcl_malloc(max + 1, "HOT_FORK shape text") : NULL;
    size_t n = buf ? fread(buf, 1, max + 1, f) : 0;
    bool whole = buf && !ferror(f) && feof(f) && n <= max &&
                 !memchr(buf, 0, n);
    if (f)
        fclose(f);
    if (!whole) {
        free(buf);
        return NULL;
    }
    buf[n] = 0;
    if (len_out)
        *len_out = n;
    return buf;
}

/* ── the resident generation ─────────────────────────────────────────── */

static bool shape_unbound(struct zcl_hotfork_shape *shape, const char *reason,
                          const char *subject, const char *detail)
{
    return shape_refuse(shape->unbound, sizeof(shape->unbound), reason,
                        subject, detail);
}

static bool shape_read_epoch(struct zcl_hotfork_shape *shape)
{
    char path[PATH_MAX];
    char *text = snprintf(path, sizeof(path), "%s/build/dev-obj/.current-epoch",
                          shape->root) < (int)sizeof(path)
                     ? shape_slurp(path, 128, NULL) : NULL;
    bool ok = text && strlen(text) >= 64 &&
              (text[64] == 0 || strcmp(text + 64, "\n") == 0);
    if (ok) {
        memcpy(shape->epoch, text, 64);
        shape->epoch[64] = 0;
        ok = shape_hex64(shape->epoch);
    }
    free(text);
    return ok || shape_unbound(shape, "NO_BASELINE", "build/dev-obj/.current-epoch",
                               "no dev build epoch names the resident's objects");
}

static const char *shape_session_value(const char *text, const char *key,
                                       char out[65])
{
    size_t klen = strlen(key);
    for (const char *line = text; line && *line;) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        if (len == klen + 1 + 64 && strncmp(line, key, klen) == 0 &&
            line[klen] == '=') {
            memcpy(out, line + klen + 1, 64);
            out[64] = 0;
            return shape_hex64(out) ? out : NULL;
        }
        line = end ? end + 1 : NULL;
    }
    return NULL;
}

/* The epoch's completed build session: its compiler identity must be the
 * action plan's, and it is part of the generation. */
static bool shape_read_session(struct zcl_hotfork_shape *shape,
                               struct sha3_256_ctx *ctx)
{
    char path[PATH_MAX], epoch[65], compiler[65], flags[65];
    char *text = snprintf(path, sizeof(path),
                          "%s/build/dev-obj/epochs/%s/.build-session",
                          shape->root, shape->epoch) < (int)sizeof(path)
                     ? shape_slurp(path, 4096, NULL) : NULL;
    bool ok = text && strstr(text, "complete=1\n") &&
              shape_session_value(text, "epoch", epoch) &&
              strcmp(epoch, shape->epoch) == 0 &&
              shape_session_value(text, "compiler_id", compiler) &&
              shape_session_value(text, "flags_sha256", flags);
    free(text);
    if (!ok)
        return shape_unbound(shape, "NO_BASELINE", shape->epoch,
                             "the resident epoch has no completed build session");
    if (!shape->compiler_id || strcmp(compiler, shape->compiler_id) != 0)
        return shape_unbound(shape, "TOOLCHAIN_CHANGED", compiler,
                             "the resident objects were built by another "
                             "compiler than the action plan names");
    shape_hash_text(ctx, epoch);
    shape_hash_text(ctx, compiler);
    shape_hash_text(ctx, flags);
    return true;
}

static bool shape_object_path(const struct zcl_hotfork_shape *shape,
                              const char *tu, char *out, size_t cap)
{
    size_t n = strlen(tu);
    return n > 2 && strcmp(tu + n - 2, ".c") == 0 &&
           snprintf(out, cap, "%s/build/dev-obj/epochs/%s/%.*s.o", shape->root,
                    shape->epoch, (int)(n - 2), tu) < (int)cap;
}

/* TU `index` of the capsule: the owner first, then each sibling. */
static bool shape_tu_at(const struct zcl_hotfork_shape *shape, size_t index,
                        char *out, size_t cap)
{
    if (index == 0)
        return snprintf(out, cap, "%s", shape->source_tu) < (int)cap;
    return zcl_hotfork_tu_list_at(shape->sibling_tus, index - 1, out, cap);
}

static size_t shape_tu_count(const struct zcl_hotfork_shape *shape)
{
    return 1 + zcl_hotfork_tu_list_count(shape->sibling_tus);
}

static bool shape_bind_objects(struct zcl_hotfork_shape *shape,
                               const struct stat *resident,
                               struct sha3_256_ctx *ctx)
{
    size_t count = shape_tu_count(shape);
    for (size_t i = 0; i < count; i++) {
        char tu[ZCL_HOTFORK_UNITY_TU_MAX], path[PATH_MAX];
        struct stat st;
        if (!shape_tu_at(shape, i, tu, sizeof(tu)) ||
            !shape_object_path(shape, tu, path, sizeof(path)) ||
            !shape_regular(path, &st))
            return shape_unbound(shape, "NO_BASELINE", tu,
                                 "the resident build object for this TU is missing");
        if (shape_newer(&st, resident))
            return shape_unbound(shape, "RESIDENT_STALE", tu,
                                 "the resident build object is newer than the "
                                 "running image; restart the resident");
        shape_hash_stat(ctx, tu, &st);
    }
    return true;
}

static void shape_toolchain(struct zcl_hotfork_shape *shape)
{
    struct sha3_256_ctx ctx;
    sha3_256_init(&ctx);
    shape_hash_text(&ctx, "zcl.hotfork_shape_toolchain.v1");
    shape_hash_text(&ctx, shape->cc ? shape->cc : "");
    shape_hash_text(&ctx, shape->compiler_id ? shape->compiler_id : "");
    shape_hash_text(&ctx, shape->cflags ? shape->cflags : "");
    shape_hex_root(&ctx, shape->toolchain);
}

void zcl_hotfork_shape_begin(struct zcl_hotfork_shape *shape, const char *root,
                             const char *source_tu, const char *sibling_tus,
                             const char *adapter_id, const char *cc,
                             const char *compiler_id, const char *cflags)
{
    memset(shape, 0, sizeof(*shape));
    shape->root = root;
    shape->source_tu = source_tu;
    shape->sibling_tus = sibling_tus;
    shape->adapter_id = adapter_id;
    shape->cc = cc;
    shape->compiler_id = compiler_id;
    shape->cflags = cflags;
    (void)snprintf(shape->generation, sizeof(shape->generation), "unbound");
    shape_toolchain(shape);
    struct sha3_256_ctx ctx;
    struct stat resident;
    sha3_256_init(&ctx);
    shape_hash_text(&ctx, "zcl.hotfork_shape_generation.v1");
    if (!root || !source_tu) {
        (void)shape_unbound(shape, "NO_BASELINE", "-", "no capsule named");
        return;
    }
    if (!shape_regular("/proc/self/exe", &resident)) {
        (void)shape_unbound(shape, "RESIDENT_UNREADABLE", "/proc/self/exe",
                            "the running image cannot be identified");
        return;
    }
    shape_hash_stat(&ctx, "resident", &resident);
    if (shape_read_epoch(shape) && shape_read_session(shape, &ctx) &&
        shape_bind_objects(shape, &resident, &ctx))
        shape_hex_root(&ctx, shape->generation);
}

/* ── ELF64 reader ────────────────────────────────────────────────────── */

static unsigned char shape_host_data(void)
{
    const uint16_t probe = 1;
    return *(const unsigned char *)&probe ? ELFDATA2LSB : ELFDATA2MSB;
}

static void *shape_pread(int fd, uint64_t off, uint64_t len, uint64_t size)
{
    if (len == 0 || len > SHAPE_SECTION_MAX || off > size || len > size - off)
        return NULL;
    unsigned char *buf = zcl_malloc((size_t)len + 1, "HOT_FORK shape ELF section");
    size_t got = 0;
    while (buf && got < len) {
        ssize_t n = pread(fd, buf + got, (size_t)(len - got),
                          (off_t)(off + got));
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            free(buf);
            return NULL;
        }
        got += (size_t)n;
    }
    if (buf)
        buf[len] = 0;
    return buf;
}

static void shape_elf_close(struct shape_elf *e)
{
    if (e->fd >= 0)
        (void)close(e->fd);
    free(e->sh);
    free(e->shstr);
    free(e->sym);
    free(e->str);
    memset(e, 0, sizeof(*e));
    e->fd = -1;
}

static bool shape_ehdr_valid(const Elf64_Ehdr *eh, uint64_t size)
{
    return memcmp(eh->e_ident, ELFMAG, SELFMAG) == 0 &&
           eh->e_ident[EI_CLASS] == ELFCLASS64 &&
           eh->e_ident[EI_DATA] == shape_host_data() &&
           eh->e_ident[EI_VERSION] == EV_CURRENT &&
           eh->e_shentsize == sizeof(Elf64_Shdr) && eh->e_shoff <= size &&
           (uint64_t)eh->e_shnum * sizeof(Elf64_Shdr) <= size - eh->e_shoff &&
           (eh->e_shnum == 0 || eh->e_shstrndx < eh->e_shnum);
}

/* The one SHT_SYMTAB; NULL with *bad set when the table is malformed or
 * extended section indices are in use (not supported: refuse). */
static const Elf64_Shdr *shape_find_symtab(const struct shape_elf *e, bool *bad)
{
    const Elf64_Shdr *symtab = NULL;
    for (size_t i = 0; i < e->eh.e_shnum; i++) {
        if (e->sh[i].sh_type == SHT_SYMTAB_SHNDX ||
            (e->sh[i].sh_type == SHT_SYMTAB && symtab))
            *bad = true;
        if (e->sh[i].sh_type == SHT_SYMTAB)
            symtab = &e->sh[i];
    }
    return *bad ? NULL : symtab;
}

static bool shape_load_names(struct shape_elf *e)
{
    const Elf64_Shdr *names = &e->sh[e->eh.e_shstrndx];
    if (names->sh_type != SHT_STRTAB)
        return false;
    e->shstr = shape_pread(e->fd, names->sh_offset, names->sh_size, e->size);
    e->shstr_len = (size_t)names->sh_size;
    return e->shstr != NULL;
}

static enum shape_read shape_load_symtab(struct shape_elf *e, bool names)
{
    bool bad = false;
    const Elf64_Shdr *symtab = shape_find_symtab(e, &bad);
    if (bad)
        return SHAPE_READ_UNREADABLE;
    if (!symtab)
        return SHAPE_READ_NO_SYMTAB;
    if (symtab->sh_entsize != sizeof(Elf64_Sym) ||
        symtab->sh_size % sizeof(Elf64_Sym) != 0 ||
        symtab->sh_link >= e->eh.e_shnum ||
        e->sh[symtab->sh_link].sh_type != SHT_STRTAB)
        return SHAPE_READ_UNREADABLE;
    const Elf64_Shdr *strs = &e->sh[symtab->sh_link];
    e->sym = shape_pread(e->fd, symtab->sh_offset, symtab->sh_size, e->size);
    e->nsym = (size_t)(symtab->sh_size / sizeof(Elf64_Sym));
    e->str = shape_pread(e->fd, strs->sh_offset, strs->sh_size, e->size);
    e->str_len = (size_t)strs->sh_size;
    bool ok = e->sym && e->str && (!names || shape_load_names(e));
    return ok ? SHAPE_READ_OK : SHAPE_READ_UNREADABLE;
}

/* Opens one ELF64 file of the host byte order. `names` also loads section
 * names (relocatable objects; the resident image needs only symbols). */
static enum shape_read shape_elf_open(struct shape_elf *e, const char *path,
                                      bool names)
{
    struct stat st;
    memset(e, 0, sizeof(*e));
    e->fd = open(path, O_RDONLY | O_CLOEXEC);
    if (e->fd < 0 || fstat(e->fd, &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size < (off_t)sizeof(Elf64_Ehdr))
        return SHAPE_READ_UNREADABLE;
    e->size = (uint64_t)st.st_size;
    if (pread(e->fd, &e->eh, sizeof(e->eh), 0) != (ssize_t)sizeof(e->eh) ||
        !shape_ehdr_valid(&e->eh, e->size))
        return SHAPE_READ_UNREADABLE;
    if (e->eh.e_shnum == 0)
        return SHAPE_READ_NO_SYMTAB;
    e->sh = shape_pread(e->fd, e->eh.e_shoff,
                        (uint64_t)e->eh.e_shnum * sizeof(Elf64_Shdr), e->size);
    return e->sh ? shape_load_symtab(e, names) : SHAPE_READ_UNREADABLE;
}

static bool shape_open_refuse(enum shape_read got, const char *subject,
                              char *why, size_t why_len)
{
    if (got == SHAPE_READ_NO_SYMTAB)
        return shape_refuse(why, why_len, "SYMTAB_MISSING", subject,
                            "the ELF object has no symbol table");
    return shape_refuse(why, why_len, "OBJECT_UNREADABLE", subject,
                        "the ELF object cannot be read or parsed");
}

static const char *shape_sym_name(const struct shape_elf *e,
                                  const Elf64_Sym *s)
{
    return s->st_name < e->str_len ? e->str + s->st_name : NULL;
}

static const char *shape_section_name(const struct shape_elf *e,
                                      const Elf64_Shdr *sec)
{
    return e->shstr && sec->sh_name < e->shstr_len ? e->shstr + sec->sh_name
                                                   : NULL;
}

/* ── shape of one relocatable object ─────────────────────────────────── */

static bool shape_push(struct shape_set *set, const char *name, size_t len,
                       uint64_t size, enum shape_kind kind)
{
    if (set->count == set->cap) {
        size_t cap = set->cap ? set->cap * 2 : 256;
        struct shape_sym *grown =
            zcl_realloc(set->items, cap * sizeof(*grown), "HOT_FORK shape set");
        if (!grown)
            return false;
        set->items = grown;
        set->cap = cap;
    }
    set->items[set->count++] = (struct shape_sym){ name, len, size, kind };
    return true;
}

static bool shape_is_global(const Elf64_Sym *s)
{
    unsigned bind = ELF64_ST_BIND(s->st_info);
    unsigned type = ELF64_ST_TYPE(s->st_info);
    return (bind == STB_GLOBAL || bind == STB_WEAK || bind == STB_GNU_UNIQUE) &&
           s->st_shndx != SHN_UNDEF && type != STT_FILE &&
           type != STT_SECTION;
}

/* SHAPE_DATA / SHAPE_TLS for writable, thread-local or common objects;
 * SHAPE_GLOBAL (meaning "not state") otherwise. */
static enum shape_kind shape_state_kind(const struct shape_elf *e,
                                        const Elf64_Sym *s)
{
    unsigned type = ELF64_ST_TYPE(s->st_info);
    if (s->st_shndx == SHN_COMMON || type == STT_COMMON)
        return SHAPE_DATA;
    if ((type != STT_OBJECT && type != STT_TLS) ||
        s->st_shndx == SHN_UNDEF || s->st_shndx >= e->eh.e_shnum)
        return SHAPE_GLOBAL;
    const Elf64_Shdr *sec = &e->sh[s->st_shndx];
    if (sec->sh_flags & SHF_TLS)
        return SHAPE_TLS;
    if (!(sec->sh_flags & SHF_WRITE))
        return SHAPE_GLOBAL;
    const char *name = shape_section_name(e, sec);
    return name && strncmp(name, ".data.rel.ro", 12) == 0 ? SHAPE_GLOBAL
                                                         : SHAPE_DATA;
}

/* A local static's name without its compiler-numbered ".N" suffixes. */
static size_t shape_canonical_len(const char *name, bool local)
{
    size_t n = strlen(name);
    while (local) {
        size_t d = n;
        while (d > 0 && name[d - 1] >= '0' && name[d - 1] <= '9')
            d--;
        if (d == n || d < 2 || name[d - 1] != '.')
            break;
        n = d - 1;
    }
    return n;
}

static void shape_note_init_fini(const struct shape_elf *e,
                                 struct shape_set *set)
{
    for (size_t i = 0; i < e->eh.e_shnum && !set->init_fini[0]; i++) {
        const char *name = shape_section_name(e, &e->sh[i]);
        unsigned type = e->sh[i].sh_type;
        bool hit = type == SHT_INIT_ARRAY || type == SHT_FINI_ARRAY ||
                   type == SHT_PREINIT_ARRAY;
        for (size_t k = 0; !hit && name && k < sizeof(k_shape_init_fini) /
                                               sizeof(k_shape_init_fini[0]); k++)
            hit = strncmp(name, k_shape_init_fini[k],
                          strlen(k_shape_init_fini[k])) == 0;
        if (hit)
            (void)snprintf(set->init_fini, sizeof(set->init_fini), "%s",
                           name ? name : "(init/fini)");
    }
}

static bool shape_collect(const struct shape_elf *e, struct shape_set *set)
{
    for (size_t i = 1; i < e->nsym; i++) {
        const Elf64_Sym *s = &e->sym[i];
        const char *name = shape_sym_name(e, s);
        if (!name || !name[0])
            continue;
        enum shape_kind state = shape_state_kind(e, s);
        bool local = ELF64_ST_BIND(s->st_info) == STB_LOCAL;
        if (shape_is_global(s) &&
            !shape_push(set, name, strlen(name), s->st_size, SHAPE_GLOBAL))
            return false;
        if (state != SHAPE_GLOBAL &&
            !shape_push(set, name, shape_canonical_len(name, local),
                        s->st_size, state))
            return false;
    }
    shape_note_init_fini(e, set);
    return true;
}

static int shape_cmp(const void *pa, const void *pb)
{
    const struct shape_sym *a = pa, *b = pb;
    if (a->kind != b->kind)
        return a->kind < b->kind ? -1 : 1;
    int c = memcmp(a->name, b->name, a->len < b->len ? a->len : b->len);
    if (c != 0)
        return c;
    if (a->len != b->len)
        return a->len < b->len ? -1 : 1;
    if (a->kind == SHAPE_GLOBAL || a->size == b->size)
        return 0;
    return a->size < b->size ? -1 : 1;
}

/* ── baseline objects and the running image ──────────────────────────── */

static bool shape_load_baseline(struct shape_run *run, char *why,
                                size_t why_len)
{
    size_t count = shape_tu_count(run->shape);
    for (size_t i = 0; i < count; i++) {
        char tu[ZCL_HOTFORK_UNITY_TU_MAX], path[PATH_MAX];
        if (!shape_tu_at(run->shape, i, tu, sizeof(tu)) ||
            !shape_object_path(run->shape, tu, path, sizeof(path)))
            return shape_refuse(why, why_len, "NO_BASELINE", tu,
                                "the resident build object path does not resolve");
        enum shape_read got = shape_elf_open(&run->objects[i], path, true);
        run->object_count = i + 1;
        if (got != SHAPE_READ_OK || run->objects[i].eh.e_type != ET_REL)
            return shape_open_refuse(got == SHAPE_READ_OK ? SHAPE_READ_UNREADABLE
                                                          : got,
                                     tu, why, why_len);
        if (!shape_collect(&run->objects[i], &run->base))
            return shape_refuse(why, why_len, "OBJECT_UNREADABLE", tu,
                                "out of memory reading the object's shape");
    }
    qsort(run->base.items, run->base.count, sizeof(run->base.items[0]),
          shape_cmp);
    return true;
}

static uint64_t shape_fnv(const char *name, size_t len)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < len; i++)
        h = (h ^ (unsigned char)name[i]) * 1099511628211ull;
    return h;
}

struct shape_index {
    size_t *slots; /* 1 + index into set->items; 0 is empty */
    bool *seen;
    size_t mask;
};

static bool shape_index_build(const struct shape_set *set,
                              struct shape_index *ix)
{
    size_t cap = 16;
    while (cap < set->count * 2 + 1)
        cap *= 2;
    ix->slots = zcl_calloc(cap, sizeof(*ix->slots), "HOT_FORK shape index");
    ix->seen = zcl_calloc(set->count + 1, sizeof(*ix->seen), "HOT_FORK shape index");
    ix->mask = cap - 1;
    for (size_t i = 0; ix->slots && ix->seen && i < set->count; i++) {
        if (set->items[i].kind != SHAPE_GLOBAL)
            continue;
        size_t at = shape_fnv(set->items[i].name, set->items[i].len) & ix->mask;
        while (ix->slots[at])
            at = (at + 1) & ix->mask;
        ix->slots[at] = i + 1;
    }
    return ix->slots && ix->seen;
}

static void shape_index_mark(const struct shape_set *set,
                             struct shape_index *ix, const char *name)
{
    size_t len = strlen(name);
    size_t at = shape_fnv(name, len) & ix->mask;
    for (; ix->slots[at]; at = (at + 1) & ix->mask) {
        const struct shape_sym *s = &set->items[ix->slots[at] - 1];
        if (s->len == len && memcmp(s->name, name, len) == 0)
            ix->seen[ix->slots[at] - 1] = true;
    }
}

static void shape_mark_resident(const struct shape_elf *img,
                                const struct shape_set *set,
                                struct shape_index *ix)
{
    for (size_t i = 1; i < img->nsym; i++) {
        const char *name = shape_sym_name(img, &img->sym[i]);
        if (name && name[0] && shape_is_global(&img->sym[i]))
            shape_index_mark(set, ix, name);
    }
}

/* Every global the resident objects define is defined by the running image:
 * the objects are the image's inputs, not a later or foreign build. */
static bool shape_resident_defines(const struct shape_set *base, char *why,
                                   size_t why_len)
{
    struct shape_elf img;
    struct shape_index ix = {0};
    enum shape_read got = shape_elf_open(&img, "/proc/self/exe", false);
    bool ok = got == SHAPE_READ_OK && shape_index_build(base, &ix);
    if (ok)
        shape_mark_resident(&img, base, &ix);
    const struct shape_sym *missing = NULL;
    for (size_t i = 0; ok && !missing && i < base->count; i++)
        if (base->items[i].kind == SHAPE_GLOBAL && !ix.seen[i])
            missing = &base->items[i];
    shape_elf_close(&img);
    free(ix.slots);
    free(ix.seen);
    if (got != SHAPE_READ_OK)
        return got == SHAPE_READ_NO_SYMTAB
                   ? shape_refuse(why, why_len, "SYMTAB_MISSING", "/proc/self/exe",
                                  "the running image has no symbol table")
                   : shape_refuse(why, why_len, "RESIDENT_UNREADABLE",
                                  "/proc/self/exe",
                                  "the running image cannot be read");
    if (!ok)
        return shape_refuse(why, why_len, "RESIDENT_UNREADABLE", "-",
                            "out of memory indexing the resident objects");
    return !missing ||
           shape_refuse(why, why_len, "RESIDENT_MISMATCH", missing->name,
                        "defined by the resident build objects but not by the "
                        "running image; the objects are not its inputs");
}

/* ── dependency closure and the binding record ───────────────────────── */

static bool shape_depfile_index(struct shape_run *run, char *text,
                                const char *end)
{
    for (char *t = text; t < end && run->dep_count < SHAPE_DEPS_MAX;
         t += strlen(t) + 1)
        if (*t)
            run->deps[run->dep_count++] = t;
    return run->dep_count > 0 && run->dep_count < SHAPE_DEPS_MAX;
}

/* Splits a make depfile into its prerequisite paths, in place, one NUL
 * between paths; the target and its ':' are dropped, a backslash-newline is
 * a separator and a backslash-space is a space inside a path. */
static bool shape_depfile_split(struct shape_run *run)
{
    char *text = shape_slurp(run->depfile, 1 << 20, NULL);
    char *colon = text ? strchr(text, ':') : NULL;
    run->deps_text = text;
    if (!colon)
        return false;
    char *out = text;
    for (const char *in = colon + 1; *in; in++) {
        bool joined = in[0] == '\\' && (in[1] == '\n' || in[1] == '\r');
        bool escaped = in[0] == '\\' && in[1] == ' ';
        in += escaped;
        char c = joined || (!escaped && strchr(" \t\r\n", *in)) ? 0 : *in;
        if (c != 0 || (out > text && out[-1] != 0))
            *out++ = c;
    }
    *out = 0;
    return shape_depfile_index(run, text, out);
}

/* Every prerequisite after the first (the generated unity, a fresh temp
 * path on each compile) names the closure. */
static void shape_closure_root(struct shape_run *run)
{
    struct sha3_256_ctx ctx;
    sha3_256_init(&ctx);
    shape_hash_text(&ctx, "zcl.hotfork_shape_closure.v1");
    for (size_t i = 1; i < run->dep_count; i++)
        shape_hash_text(&ctx, run->deps[i]);
    shape_hex_root(&ctx, run->closure);
}

static bool shape_record_path(struct shape_run *run)
{
    size_t n = strlen(run->depfile);
    return n > 2 && strcmp(run->depfile + n - 2, ".d") == 0 &&
           snprintf(run->record, sizeof(run->record), "%.*s.shape",
                    (int)(n - 2), run->depfile) < (int)sizeof(run->record);
}

static bool shape_record_field(const char *text, const char *key,
                               char out[65])
{
    char want[32];
    (void)snprintf(want, sizeof(want), "\n%s=", key);
    const char *at = strstr(text, want);
    if (!at)
        return false;
    at += strlen(want);
    if (strnlen(at, 65) < 65 || at[64] != '\n')
        return false;
    memcpy(out, at, 64);
    out[64] = 0;
    return shape_hex64(out);
}

static bool shape_record_check(struct shape_run *run, char *why,
                               size_t why_len)
{
    if (!shape_depfile_split(run) || !shape_record_path(run))
        return shape_refuse(why, why_len, "CLOSURE_UNREADABLE", run->depfile,
                            "the candidate's dependency closure cannot be read");
    shape_closure_root(run);
    struct stat st;
    if (!shape_regular(run->record, &st))
        return true;
    char *text = shape_slurp(run->record, 4096, NULL);
    char generation[65], toolchain[65], closure[65];
    bool ok = text && strncmp(text, "zcl.hotfork_shape_record.v1\n", 28) == 0 &&
              shape_record_field(text, "generation", generation) &&
              shape_record_field(text, "toolchain", toolchain) &&
              shape_record_field(text, "closure", closure);
    free(text);
    if (!ok) {
        (void)unlink(run->record);
        return shape_refuse(why, why_len, "RECORD_UNREADABLE", run->record,
                            "the shape binding record is malformed and was "
                            "discarded");
    }
    if (strcmp(generation, run->shape->generation) != 0)
        return true;
    if (strcmp(toolchain, run->shape->toolchain) != 0)
        return shape_refuse(why, why_len, "TOOLCHAIN_CHANGED", toolchain,
                            "the baseline was bound under another compiler, "
                            "driver or flags than this candidate was built with");
    if (strcmp(closure, run->closure) != 0)
        return shape_refuse(why, why_len, "CLOSURE_CHANGED", closure,
                            "the baseline was bound to another dependency "
                            "closure than this candidate's");
    run->record_current = true;
    return true;
}

static bool shape_record_commit(struct shape_run *run, char *why,
                                size_t why_len)
{
    if (run->record_current)
        return true;
    char tmp[PATH_MAX + 32];
    FILE *f = snprintf(tmp, sizeof(tmp), "%s.%ld.tmp", run->record,
                       (long)getpid()) < (int)sizeof(tmp)
                  ? fopen(tmp, "wb") : NULL;
    bool ok = f && fprintf(f,
                           "zcl.hotfork_shape_record.v1\ngeneration=%s\n"
                           "toolchain=%s\nclosure=%s\n",
                           run->shape->generation, run->shape->toolchain,
                           run->closure) > 0;
    if (f && fclose(f) != 0)
        ok = false;
    ok = ok && rename(tmp, run->record) == 0;
    if (!ok) {
        if (f)
            (void)unlink(tmp);
        return shape_refuse(why, why_len, "RECORD_UNWRITABLE", run->record,
                            "the shape binding record could not be written");
    }
    return true;
}

/* ── adapter attribution ─────────────────────────────────────────────── */

static bool shape_is_ident(unsigned char c)
{
    return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

static bool shape_has_token(const char *text, const char *name, size_t len)
{
    for (const char *p = text; (p = strstr(p, name)) != NULL; p++) {
        /* strstr matched the NUL-terminated prefix; the canonical length may
         * be shorter than the stored name, so recheck the boundary at len. */
        if ((p == text || !shape_is_ident((unsigned char)p[-1])) &&
            !shape_is_ident((unsigned char)p[len]))
            return true;
    }
    return false;
}

/* True when [p, p+n) holds the identifier token name[0..len). */
static bool shape_span_has(const char *p, size_t n, const char *name,
                           size_t len)
{
    size_t i = 0;
    while (i < n) {
        size_t start = i;
        while (i < n && shape_is_ident((unsigned char)p[i]))
            i++;
        if (i - start == len && memcmp(p + start, name, len) == 0)
            return true;
        i += i == start;
    }
    return false;
}

static bool shape_is_adapter_path(const struct shape_run *run,
                                  const char *path, const char *story)
{
    size_t root_len = strlen(run->shape->root);
    return strcmp(path, story) == 0 ||
           (strncmp(path, run->shape->root, root_len) == 0 &&
            path[root_len] == '/' && strcmp(path + root_len + 1, story) == 0);
}

static char *shape_read_dep(const struct shape_run *run, const char *dep)
{
    char path[PATH_MAX];
    if (dep[0] == '/')
        return shape_slurp(dep, SHAPE_TEXT_MAX, NULL);
    return snprintf(path, sizeof(path), "%s/%s", run->shape->root, dep) <
                   (int)sizeof(path)
               ? shape_slurp(path, SHAPE_TEXT_MAX, NULL)
               : NULL;
}

static const char *shape_adapter(struct shape_run *run)
{
    char path[PATH_MAX];
    if (!run->texts.adapter_tried) {
        run->texts.adapter_tried = true;
        if (zcl_hotfork_story_path(run->shape->adapter_id, run->story,
                                   sizeof(run->story)) &&
            snprintf(path, sizeof(path), "%s/%s", run->shape->root,
                     run->story) < (int)sizeof(path))
            run->adapter = shape_slurp(path, SHAPE_TEXT_MAX, NULL);
    }
    return run->adapter;
}

static void shape_texts_load(struct shape_run *run)
{
    struct shape_texts *t = &run->texts;
    t->loaded = true;
    t->ok = shape_adapter(run) != NULL;
    for (size_t i = 1; t->ok && i < run->dep_count; i++) {
        if (shape_is_adapter_path(run, run->deps[i], run->story))
            continue;
        t->files[t->count] = shape_read_dep(run, run->deps[i]);
        t->ok = t->files[t->count++] != NULL;
    }
}

/* A candidate-only symbol is the story adapter's when its name is a token
 * of the adapter and of no other file the candidate compiled. */
static bool shape_adapter_owns(struct shape_run *run,
                               const struct shape_sym *s)
{
    char name[512];
    if (s->len >= sizeof(name))
        return false;
    memcpy(name, s->name, s->len);
    name[s->len] = 0;
    if (!run->texts.loaded)
        shape_texts_load(run);
    if (!run->texts.ok || !shape_has_token(run->adapter, name, s->len))
        return false;
    for (size_t i = 0; i < run->texts.count; i++)
        if (shape_has_token(run->texts.files[i], name, s->len))
            return false;
    return true;
}

/* A resident global the candidate deliberately does not rebuild: the story
 * adapter names it on a "hotfork-resident-bound:" line (a capsule core
 * macro compiles that definition out, so the candidate's references bind the
 * resident's copy by design). Only resident-side globals are ever excused. */
static bool shape_resident_bound(struct shape_run *run,
                                 const struct shape_sym *s)
{
    static const char marker[] = "hotfork-resident-bound:";
    const char *text = shape_adapter(run);
    for (const char *p = text; p && (p = strstr(p, marker)) != NULL;) {
        p += sizeof(marker) - 1;
        if (shape_span_has(p, strcspn(p, "\n*"), s->name, s->len))
            return true;
    }
    return false;
}

/* ── the comparison ──────────────────────────────────────────────────── */

struct shape_diff {
    const struct shape_sym *removed; /* resident global, candidate lacks */
    const struct shape_sym *added;   /* candidate global, resident lacks */
    const struct shape_sym *state;   /* writable state present on one side */
    bool state_in_resident;
};

static void shape_note(struct shape_run *run, struct shape_diff *d,
                       const struct shape_sym *s, bool resident_side)
{
    if (resident_side ? s->kind == SHAPE_GLOBAL && shape_resident_bound(run, s)
                      : shape_adapter_owns(run, s))
        return;
    if (s->kind == SHAPE_GLOBAL) {
        const struct shape_sym **slot = resident_side ? &d->removed : &d->added;
        if (!*slot)
            *slot = s;
        return;
    }
    if (!d->state) {
        d->state = s;
        d->state_in_resident = resident_side;
    }
}

static void shape_walk(struct shape_run *run, struct shape_diff *d)
{
    size_t i = 0, j = 0;
    const struct shape_set *a = &run->base, *b = &run->next;
    while (i < a->count || j < b->count) {
        int c = i == a->count ? 1
              : j == b->count ? -1
                              : shape_cmp(&a->items[i], &b->items[j]);
        if (c == 0) {
            i++;
            j++;
        } else if (c < 0) {
            shape_note(run, d, &a->items[i++], true);
        } else {
            shape_note(run, d, &b->items[j++], false);
        }
    }
}

static bool shape_report(const struct shape_diff *d, char *why, size_t why_len)
{
    char subject[512];
    const struct shape_sym *s = d->removed ? d->removed
                              : d->added   ? d->added
                                           : d->state;
    if (!s)
        return true;
    (void)snprintf(subject, sizeof(subject), "%.*s", (int)s->len, s->name);
    if (d->removed)
        return shape_refuse(why, why_len, "ABI_REMOVED", subject,
                            "exported by the resident build of the capsule but "
                            "not defined by the candidate; its callers would "
                            "bind the resident's stale copy");
    if (d->added)
        return shape_refuse(why, why_len, "ABI_ADDED", subject,
                            "exported by the candidate but not by the resident "
                            "build of the capsule");
    return shape_refuse(why, why_len, "STATE_CHANGED", subject,
                        d->state_in_resident
                            ? "writable state of the resident build is absent "
                              "or resized in the candidate"
                            : "the candidate adds or resizes writable state");
}

static bool shape_compare(struct shape_run *run, char *why, size_t why_len)
{
    enum shape_read got = shape_elf_open(&run->cand, run->candidate, true);
    if (got != SHAPE_READ_OK || run->cand.eh.e_type != ET_REL)
        return shape_open_refuse(got == SHAPE_READ_OK ? SHAPE_READ_UNREADABLE
                                                      : got,
                                 "candidate", why, why_len);
    if (!shape_collect(&run->cand, &run->next))
        return shape_refuse(why, why_len, "OBJECT_UNREADABLE", "candidate",
                            "out of memory reading the candidate's shape");
    if (run->next.init_fini[0])
        return shape_refuse(why, why_len, "INIT_FINI", run->next.init_fini,
                            "the candidate carries a constructor or destructor "
                            "section that would run at dlopen");
    qsort(run->next.items, run->next.count, sizeof(run->next.items[0]),
          shape_cmp);
    struct shape_diff diff = {0};
    shape_walk(run, &diff);
    return shape_report(&diff, why, why_len);
}

static void shape_run_free(struct shape_run *run)
{
    for (size_t i = 0; i < run->object_count; i++)
        shape_elf_close(&run->objects[i]);
    shape_elf_close(&run->cand);
    free(run->base.items);
    free(run->next.items);
    free(run->deps_text);
    free(run->adapter);
    for (size_t i = 0; i < run->texts.count; i++)
        free(run->texts.files[i]);
}

static bool shape_still_bound(const struct zcl_hotfork_shape *shape,
                              char *why, size_t why_len)
{
    struct zcl_hotfork_shape now;
    zcl_hotfork_shape_begin(&now, shape->root, shape->source_tu,
                            shape->sibling_tus, shape->adapter_id, shape->cc,
                            shape->compiler_id, shape->cflags);
    if (now.unbound[0]) {
        if (why && why_len)
            (void)snprintf(why, why_len, "%s", now.unbound);
        return false;
    }
    return strcmp(now.generation, shape->generation) == 0 ||
           shape_refuse(why, why_len, "GENERATION_MOVED", shape->source_tu,
                        "the resident image or its build objects changed "
                        "while the candidate compiled");
}

bool zcl_hotfork_shape_admit(bool prior, const struct zcl_hotfork_shape *shape,
                             const char *candidate_object, const char *depfile,
                             char *why, size_t why_len)
{
    if (!prior)
        return false;
    if (!shape || !candidate_object || !depfile)
        return shape_refuse(why, why_len, "NO_BASELINE", "-",
                            "no shape facts were captured");
    struct shape_run *run = zcl_calloc(1, sizeof(*run), "HOT_FORK shape run");
    if (!run)
        return shape_refuse(why, why_len, "OBJECT_UNREADABLE", "-",
                            "out of memory");
    *run = (struct shape_run){ .shape = shape, .candidate = candidate_object,
                               .depfile = depfile };
    run->cand.fd = -1;
    bool ok = shape_still_bound(shape, why, why_len) &&
              shape_load_baseline(run, why, why_len) &&
              shape_resident_defines(&run->base, why, why_len) &&
              shape_record_check(run, why, why_len) &&
              shape_compare(run, why, why_len) &&
              shape_record_commit(run, why, why_len);
    shape_run_free(run);
    free(run);
    return ok;
}

#else /* !__linux__ */

void zcl_hotfork_shape_begin(struct zcl_hotfork_shape *shape, const char *root,
                             const char *source_tu, const char *sibling_tus,
                             const char *adapter_id, const char *cc,
                             const char *compiler_id, const char *cflags)
{
    memset(shape, 0, sizeof(*shape));
    shape->root = root;
    shape->source_tu = source_tu;
    shape->sibling_tus = sibling_tus;
    shape->adapter_id = adapter_id;
    shape->cc = cc;
    shape->compiler_id = compiler_id;
    shape->cflags = cflags;
    (void)snprintf(shape->generation, sizeof(shape->generation), "unbound");
    (void)shape_refuse(shape->unbound, sizeof(shape->unbound), "UNSUPPORTED",
                       source_tu, "ELF shape facts are read on Linux only");
}

bool zcl_hotfork_shape_admit(bool prior, const struct zcl_hotfork_shape *shape,
                             const char *candidate_object, const char *depfile,
                             char *why, size_t why_len)
{
    (void)candidate_object;
    (void)depfile;
    (void)shape;
    if (!prior)
        return false;
    return shape_refuse(why, why_len, "UNSUPPORTED", "-",
                        "ELF shape facts are read on Linux only");
}

#endif
