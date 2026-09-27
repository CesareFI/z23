/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: Read the functions of an x86-64 ET_REL object for the semantic-facts fuzz oracle: each STT_FUNC symbol's bytes and relocations, digested with and without addends, and an executable's DT_RUNPATH. */
#include "test/semantic_fuzz.h"

#include "base/log_macros.h"
#include "base/safe_alloc.h"
#include "base/serialize_le.h"
#include "sha3/sha3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The on-disk ELF64 encoding, spelled out so the values this reader
 * enforces are visible where they are enforced. The hotswap probe
 * (engine/modules/hotswap) reads only shared objects' dynamic segment; a
 * relocatable object needs the section, symbol and RELA tables. */
#define EHDR_SIZE 64u
#define SHDR_SIZE 64u
#define SYM_SIZE 24u
#define RELA_SIZE 24u
#define DYN_SIZE 16u
#define ET_REL_ 1u
#define EM_X86_64_ 62u
#define SHT_SYMTAB_ 2u
#define SHT_RELA_ 4u
#define SHT_DYNAMIC_ 6u
#define SHT_NOBITS_ 8u
#define STT_SECTION_ 3u
#define STT_FUNC_ 2u
#define SHN_LORESERVE_ 0xff00u
#define DT_RPATH_ 15u
#define DT_RUNPATH_ 29u
#define MAX_SHNUM 65536u
#define MAX_IMAGE (256u * 1024u * 1024u)

struct shdr {
    uint32_t name, type, link, info;
    uint64_t offset, size, entsize;
};

struct elf {
    const uint8_t *b;
    size_t n;
    struct shdr *sh;
    uint32_t shnum, shstrndx;
    uint32_t symtab; /* section index of SHT_SYMTAB, 0 none */
    char *err;
    size_t errlen;
};

/* [off, off+len) inside the image, without wraparound. */
static const uint8_t *at(const struct elf *e, uint64_t off, uint64_t len)
{
    if (len > e->n || off > e->n - len)
        return NULL;
    return e->b + off;
}

static bool refuse(struct elf *e, const char *what)
{
    (void)snprintf(e->err, e->errlen, "%s", what);
    return false;
}

/* A NUL-terminated string at offset `off` of string section `sec`. */
static const char *str_at(const struct elf *e, uint32_t sec, uint64_t off)
{
    const struct shdr *s;
    const uint8_t *p;
    if (sec == 0 || sec >= e->shnum)
        return NULL;
    s = &e->sh[sec];
    if (off >= s->size || (p = at(e, s->offset, s->size)) == NULL)
        return NULL;
    if (memchr(p + off, 0, (size_t)(s->size - off)) == NULL)
        return NULL;
    return (const char *)p + off;
}

static bool read_header(struct elf *e, bool rel)
{
    const uint8_t *h = at(e, 0, EHDR_SIZE);
    if (h == NULL || memcmp(h, "\177ELF", 4) != 0)
        return refuse(e, "not an ELF image");
    if (h[4] != 2 || h[5] != 1)
        return refuse(e, "not ELF64 little-endian");
    if (rel && (zcl_read_u16_le(h + 16) != ET_REL_ ||
                zcl_read_u16_le(h + 18) != EM_X86_64_))
        return refuse(e, "not an x86-64 relocatable object");
    if (zcl_read_u16_le(h + 58) != SHDR_SIZE)
        return refuse(e, "unexpected section header size");
    e->shnum = zcl_read_u16_le(h + 60);
    e->shstrndx = zcl_read_u16_le(h + 62);
    if (e->shnum == 0 || e->shstrndx >= e->shnum)
        return refuse(e, "extended section numbering");
    return true;
}

static bool read_sections(struct elf *e)
{
    uint64_t shoff = zcl_read_u64_le(e->b + 40);
    const uint8_t *t = at(e, shoff, (uint64_t)e->shnum * SHDR_SIZE);
    if (t == NULL)
        return refuse(e, "section header table out of range");
    e->sh = zcl_calloc(e->shnum, sizeof(*e->sh), "sfz.elf.sh");
    if (e->sh == NULL)
        return refuse(e, "out of memory");
    for (uint32_t k = 0; k < e->shnum; k++) {
        const uint8_t *s = t + (size_t)k * SHDR_SIZE;
        struct shdr *d = &e->sh[k];
        d->name = zcl_read_u32_le(s);
        d->type = zcl_read_u32_le(s + 4);
        d->offset = zcl_read_u64_le(s + 24);
        d->size = zcl_read_u64_le(s + 32);
        d->link = zcl_read_u32_le(s + 40);
        d->info = zcl_read_u32_le(s + 44);
        d->entsize = zcl_read_u64_le(s + 56);
        if (d->type != SHT_NOBITS_ && at(e, d->offset, d->size) == NULL)
            return refuse(e, "section out of range");
        if (d->type == SHT_SYMTAB_)
            e->symtab = k;
    }
    return true;
}

static bool open_elf(struct elf *e, const uint8_t *img, size_t n, bool rel,
                     char *err, size_t errlen)
{
    memset(e, 0, sizeof(*e));
    e->b = img;
    e->n = n;
    e->err = err;
    e->errlen = errlen;
    return read_header(e, rel) && read_sections(e);
}

/* Symbol k of the symbol table: its name, type, section and value. */
struct sym {
    const char *name;
    uint32_t type, shndx;
    uint64_t value, size;
};

static bool sym_at(const struct elf *e, uint64_t k, struct sym *out)
{
    const struct shdr *st = &e->sh[e->symtab];
    const uint8_t *s;
    if (k >= st->size / SYM_SIZE)
        return false;
    s = e->b + st->offset + k * SYM_SIZE;
    out->type = s[4] & 0xfu;
    out->shndx = zcl_read_u16_le(s + 6);
    out->value = zcl_read_u64_le(s + 8);
    out->size = zcl_read_u64_le(s + 16);
    /* a section symbol is named by its section, as objdump prints it */
    if (out->type == STT_SECTION_ && out->shndx < e->shnum)
        out->name = str_at(e, e->shstrndx, e->sh[out->shndx].name);
    else
        out->name = str_at(e, st->link, zcl_read_u32_le(s));
    return out->name != NULL;
}

/* Digest the RELA entries of section `sec` that patch [lo, hi) into both
 * contexts: offset from lo, type and target name; the addend only into
 * `full`. */
static bool digest_relocs(struct elf *e, uint32_t sec, uint64_t lo, uint64_t hi,
                          struct sha3_256_ctx *full, struct sha3_256_ctx *noadd)
{
    for (uint32_t r = 0; r < e->shnum; r++) {
        const struct shdr *rs = &e->sh[r];
        if (rs->type != SHT_RELA_ || rs->info != sec)
            continue;
        if (rs->link != e->symtab)
            return refuse(e, "relocations against another symbol table");
        for (uint64_t q = 0; q < rs->size / RELA_SIZE; q++) {
            const uint8_t *p = e->b + rs->offset + q * RELA_SIZE;
            uint64_t off = zcl_read_u64_le(p), info = zcl_read_u64_le(p + 8);
            uint8_t rec[12];
            struct sym t;
            if (off < lo || off >= hi)
                continue;
            if (!sym_at(e, info >> 32, &t))
                return refuse(e, "relocation names no symbol");
            zcl_write_u64_le(rec, off - lo);
            zcl_write_u32_le(rec + 8, (uint32_t)info);
            sha3_256_write(full, rec, sizeof(rec));
            sha3_256_write(noadd, rec, sizeof(rec));
            sha3_256_write(full, (const unsigned char *)t.name, strlen(t.name) + 1);
            sha3_256_write(noadd, (const unsigned char *)t.name, strlen(t.name) + 1);
            sha3_256_write(full, p + 16, 8);
        }
    }
    return true;
}

static bool add_func(struct elf *e, const struct sym *s, struct sfz_funcs *out)
{
    const struct shdr *sec;
    struct sha3_256_ctx full, noadd;
    struct sfz_func *f;
    if (s->shndx == 0 || s->shndx >= SHN_LORESERVE_ || s->shndx >= e->shnum)
        return true; /* undefined or absolute: no bytes here */
    sec = &e->sh[s->shndx];
    if (sec->type == SHT_NOBITS_ || s->size > sec->size ||
        s->value > sec->size - s->size)
        return refuse(e, "function outside its section");
    if (strlen(s->name) >= SFZ_NAME_MAX)
        return refuse(e, "function name too long");
    f = &out->v[out->n];
    memset(f, 0, sizeof(*f));
    memcpy(f->name, s->name, strlen(s->name) + 1);
    f->shndx = s->shndx;
    f->value = s->value;
    f->size = s->size;
    sha3_256_init(&full);
    sha3_256_init(&noadd);
    sha3_256_write(&full, e->b + sec->offset + s->value, (size_t)s->size);
    sha3_256_write(&noadd, e->b + sec->offset + s->value, (size_t)s->size);
    if (!digest_relocs(e, s->shndx, s->value, s->value + s->size, &full, &noadd))
        return false;
    sha3_256_finalize(&full, f->full);
    sha3_256_finalize(&noadd, f->noadd);
    out->n++;
    return true;
}

bool sfz_elf_funcs(const uint8_t *img, size_t n, struct sfz_funcs *out,
                   char *err, size_t errlen)
{
    struct elf e;
    uint64_t nsym;
    bool ok;
    memset(out, 0, sizeof(*out));
    ok = open_elf(&e, img, n, true, err, errlen);
    ok = ok && (e.symtab != 0 || refuse(&e, "no symbol table"));
    nsym = ok ? e.sh[e.symtab].size / SYM_SIZE : 0;
    out->v = ok ? zcl_calloc(nsym ? nsym : 1, sizeof(*out->v), "sfz.elf.funcs")
                : NULL;
    ok = ok && (out->v != NULL || refuse(&e, "out of memory"));
    for (uint64_t k = 1; ok && k < nsym; k++) {
        struct sym s;
        ok = sym_at(&e, k, &s) || refuse(&e, "unterminated symbol name");
        if (ok && s.type == STT_FUNC_)
            ok = add_func(&e, &s, out);
    }
    free(e.sh);
    if (!ok)
        sfz_funcs_free(out);
    return ok;
}

void sfz_funcs_free(struct sfz_funcs *f)
{
    free(f->v);
    f->v = NULL;
    f->n = 0;
}

const struct sfz_func *sfz_func_find(const struct sfz_funcs *f,
                                     const char *name, size_t k)
{
    for (size_t i = 0; i < f->n; i++)
        if (strcmp(f->v[i].name, name) == 0 && k-- == 0)
            return &f->v[i];
    return NULL;
}

/* ── DT_RUNPATH ─────────────────────────────────────────────────────────── */

static bool read_image(const char *path, uint8_t **out, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    long size;
    bool ok = false;
    *out = NULL;
    if (fp == NULL)
        return false;
    if (fseek(fp, 0, SEEK_END) == 0 && (size = ftell(fp)) > 0 &&
        (unsigned long)size <= MAX_IMAGE && fseek(fp, 0, SEEK_SET) == 0) {
        *out = zcl_malloc((size_t)size, "sfz.elf.image");
        ok = *out != NULL && fread(*out, 1, (size_t)size, fp) == (size_t)size;
        *len = (size_t)size;
    }
    (void)fclose(fp);
    if (!ok) {
        free(*out);
        *out = NULL;
    }
    return ok;
}

/* The string DT_RUNPATH names in dynamic section `sec` (DT_RPATH when
 * there is no DT_RUNPATH). */
static bool dyn_path(const struct elf *e, uint32_t sec, char *out, size_t outlen)
{
    const struct shdr *d = &e->sh[sec];
    const char *found = NULL;
    for (uint64_t q = 0; q < d->size / DYN_SIZE; q++) {
        const uint8_t *p = e->b + d->offset + q * DYN_SIZE;
        uint64_t tag = zcl_read_u64_le(p);
        const char *s;
        if (tag != DT_RUNPATH_ && tag != DT_RPATH_)
            continue;
        s = str_at(e, d->link, zcl_read_u64_le(p + 8));
        if (s != NULL && (found == NULL || tag == DT_RUNPATH_))
            found = s;
    }
    return found != NULL && (size_t)snprintf(out, outlen, "%s", found) < outlen;
}

bool sfz_elf_runpath(const char *path, char *out, size_t outlen)
{
    uint8_t *img = NULL;
    size_t n = 0;
    char err[128];
    struct elf e = {0};
    bool ok = read_image(path, &img, &n) &&
              open_elf(&e, img, n, false, err, sizeof(err));
    bool found = false;
    for (uint32_t k = 0; ok && !found && k < e.shnum; k++)
        if (e.sh[k].type == SHT_DYNAMIC_)
            found = dyn_path(&e, k, out, outlen);
    free(e.sh);
    free(img);
    return found;
}
