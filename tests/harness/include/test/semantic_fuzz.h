/* Copyright 2026 Rhett Creighton - Apache License 2.0
 * purpose: The semantic-facts differential fuzzer: its project generator, fixed reproducers, relocatable-ELF symbol reader and the per-case oracle the semantic_facts_fuzz group runs. */
#ifndef ZCL_TEST_SEMANTIC_FUZZ_H
#define ZCL_TEST_SEMANTIC_FUZZ_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ── the generator (semantic_fuzz_gen.c, semantic_fuzz_templ.c) ─────────── */

/* Profile bits: drop a mutation family so the others are not drowned out.
 * __COUNTER__ and __LINE__ users make most plans fall back
 * ("position-dependent"), so the no-ctr-line profile narrows far more. */
enum {
    SFZ_NO_CTR = 1u,  /* no __COUNTER__ anywhere, no counter_c mutation */
    SFZ_NO_LINE = 2u, /* no __LINE__ anywhere */
    SFZ_NO_FLAG = 4u, /* no flag mutation */
};

struct sfz_meta {
    char kind[32];    /* the mutation kind */
    char detail[256]; /* what it changed */
    int ntus;         /* src/t0.c .. src/t<ntus-1>.c */
};

/* Write <dir>/before/ and <dir>/after/: a random multi-TU C23 project
 * (src/, inc1/, inc2/ and a Makefile whose CFLAGS_EXTRA line carries the
 * project's extra flags) and the same project after one mutation, plus
 * <dir>/meta.txt. `kind` NULL draws one; a kind name forces it. The same
 * seed and profile always write the same bytes. */
bool sfz_generate(uint64_t seed, unsigned profile, const char *kind,
                  const char *dir, struct sfz_meta *meta);

/* True when `kind` names a mutation kind sfz_generate() can force. */
bool sfz_kind_known(const char *kind);

/* mkdir -p with mode 0700. */
bool sfz_mkdirs(const char *path);
/* Write `n` bytes to <dir>/<rel>, creating its directories. */
bool sfz_put(const char *dir, const char *rel, const char *text, size_t n);

/* ── the fixed reproducers (semantic_fuzz_repro.c) ──────────────────────── */

/* One file of a reproducer; NULL text: absent on that side. */
struct sfz_file {
    const char *path;
    const char *before, *after;
};

struct sfz_repro {
    const char *name;   /* F<n>_... a false negative found and fixed, or
                           pass_... a shape that must keep passing */
    const char *kind, *detail;
    bool gcc_deps;      /* the depfiles come from gcc, as the dev compile's */
    const char *known_red; /* non-NULL: expected to fail until this lands */
    const struct sfz_file *files;
    size_t nfiles;
    /* With known_red: the exact false-negative lines the case must report,
     * each ending in '\n', in report order. The case holds only when it
     * FAILs with exactly these lines; a PASS means the mark is stale. */
    const char *known_red_why;
};

extern const struct sfz_repro k_sfz_repros[];
extern const size_t k_sfz_nrepros;

/* Write <dir>/before/ and <dir>/after/ of reproducer r. */
bool sfz_write_repro(const struct sfz_repro *r, const char *dir);

/* ── relocatable ELF64 symbols (semantic_fuzz_elf.c) ────────────────────── */

#define SFZ_NAME_MAX 128

/* One defined STT_FUNC or STT_OBJECT symbol of an ET_REL object: its
 * bytes (a NOBITS object: its size) and the RELA entries that patch them,
 * digested twice. `raw` takes each relocation's offset, type, target
 * symbol name and addend. `resolved` takes its offset and type and what it
 * addresses: for a section or local symbol defined in the object, the
 * content at the addressed offset (a merged string or constant, the object
 * it lands in with the position inside it, the function it lands in by
 * name, else the bytes up to the next symbol or the section end); for any
 * other symbol, its name and the addend. Equal `resolved` digests with
 * different `raw` ones differ only where the same content sits. */
struct sfz_sym {
    char name[SFZ_NAME_MAX];
    bool object;         /* STT_OBJECT, else STT_FUNC */
    bool local;          /* STB_LOCAL: a static, or a compiler-made clone */
    uint32_t shndx;
    uint64_t value, size;
    uint8_t raw[32];
    uint8_t resolved[32];
};

struct sfz_syms {
    struct sfz_sym *v;
    size_t n;
};

/* Read every defined function and every data object the source names of
 * the x86-64 little-endian ET_REL image `img` (a compiler-private .L label,
 * such as a string literal, is judged only through the relocations that
 * address it). Fails closed, naming the defect in
 * err, on any other shape, any out-of-range offset, symbol or string, or
 * an unterminated name. */
bool sfz_elf_syms(const uint8_t *img, size_t n, struct sfz_syms *out,
                  char *err, size_t errlen);
void sfz_syms_free(struct sfz_syms *f);
/* The k-th (0-based) symbol named `name`, or NULL. */
const struct sfz_sym *sfz_sym_find(const struct sfz_syms *f, const char *name,
                                   size_t k);

/* The DT_RUNPATH (else DT_RPATH) string of an ELF64 executable or shared
 * object, from its section headers. False when there is none or the file
 * is not ELF64 little-endian. */
bool sfz_elf_runpath(const char *path, char *out, size_t outlen);

/* ── one differential case (semantic_fuzz_case.c) ───────────────────────── */

struct sfz_env {
    char sensor[PATH_MAX]; /* absolute build/bin/z23-clang-manifest */
    char clang[PATH_MAX];  /* the clang of the LLVM the sensor links */
    char gcc[PATH_MAX];    /* gcc for gcc_deps cases, "" when absent */
    int jobs;              /* compiles and sensor runs at once */
};

struct sfz_case {
    char label[96];        /* "seed=<n> profile=<p>" or the reproducer name */
    char dir[PATH_MAX];    /* holds before/ and after/ */
    char kind[32], detail[256];
    bool gcc_deps;
};

enum sfz_status { SFZ_PASS, SFZ_FAIL, SFZ_NOOP, SFZ_ERROR };

struct sfz_outcome {
    enum sfz_status status;
    bool narrowed;         /* the plan's verdict narrowed */
    char vreason[64];      /* its fallback reason, "" when narrowed */
    size_t tus, predicted, changed, missed, over;
    size_t cfun;           /* functions with new bytes in changed objects */
    size_t cobj;           /* data objects with new bytes in changed objects */
    size_t covered_seed, covered_other, reloc, notcov;
    size_t seeds;          /* seeds the verdict names */
    double plan_s;
    char why[2048];        /* the first false-negative lines, or the error */
};

/* Compile every TU before and after (clang -std=c23 -O1 with the
 * project's flags), sense each, plan the change set in process with the
 * facts, and judge: (1) every TU the plan leaves unaffected has a
 * byte-identical object; (2) every function and data object with new bytes
 * or new addressed content (an addend alone that resolves to the same
 * content does not count) is a seed or is covered by a fallback or a
 * broadened TU after a header change, and so is every alias of a seed.
 * A PASS removes the case directory. */
bool sfz_run_case(const struct sfz_env *env, const struct sfz_case *c,
                  struct sfz_outcome *out);

#endif /* ZCL_TEST_SEMANTIC_FUZZ_H */
